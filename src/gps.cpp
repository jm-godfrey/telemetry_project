#include "../include/gps.hpp"
 
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <vector>
#include <iostream>
#include <algorithm>
 
static constexpr int  GPS_I2C_ADDRESS = 0x42;
static constexpr char GPS_I2C_BUS[]   = "/dev/i2c-1";
 
// ── Helpers ──────────────────────────────────────────────────────────────────
 
static std::vector<std::string> splitString(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        tokens.push_back(token);
    return tokens;
}
 
// Builds Unix epoch milliseconds (UTC) from an RMC time field (hhmmss.ss) and
// date field (ddmmyy). Returns 0 if either field is empty or malformed.
// Uses timegm() so the tm is interpreted as UTC — NOT mktime(), which would
// wrongly apply the Pi's local timezone.
static uint64_t nmeaToEpochMs(const std::string& timeStr,
                              const std::string& dateStr)
{
    if (timeStr.size() < 6 || dateStr.size() < 6) return 0;

    std::tm tm{};
    try
    {
        tm.tm_mday = std::stoi(dateStr.substr(0, 2));
        tm.tm_mon  = std::stoi(dateStr.substr(2, 2)) - 1;     // 0-based month
        tm.tm_year = std::stoi(dateStr.substr(4, 2)) + 100;   // 20yy => years since 1900
        tm.tm_hour = std::stoi(timeStr.substr(0, 2));
        tm.tm_min  = std::stoi(timeStr.substr(2, 2));
        tm.tm_sec  = std::stoi(timeStr.substr(4, 2));
    }
    catch (...)
    {
        return 0;
    }

    const time_t secs = timegm(&tm);
    if (secs == static_cast<time_t>(-1)) return 0;

    // Fractional seconds after the '.', normalised to milliseconds.
    uint64_t fracMs = 0;
    const size_t dot = timeStr.find('.');
    if (dot != std::string::npos)
    {
        std::string frac = timeStr.substr(dot + 1);
        frac.resize(3, '0');                 // pad/truncate to exactly 3 digits
        fracMs = static_cast<uint64_t>(std::strtol(frac.c_str(), nullptr, 10));
    }

    return static_cast<uint64_t>(secs) * 1000 + fracMs;
}

// Converts NMEA ddmm.mmmm / dddmm.mmmm + direction to decimal degrees.
static double nmeaToDecimalDegrees(const std::string& coord,
                                   const std::string& dir)
{
    if (coord.empty()) return 0.0;
    const double raw     = std::stod(coord);
    const int    degrees = static_cast<int>(raw / 100);
    const double minutes = raw - degrees * 100.0;
    double       decimal = degrees + minutes / 60.0;
    if (dir == "S" || dir == "W") decimal = -decimal;
    return decimal;
}
 
// ── GPS class ────────────────────────────────────────────────────────────────
 
GPS::GPS() : fd(-1) {}
GPS::~GPS() { close(); }
 
bool GPS::initialise()
{
    fd = ::open(GPS_I2C_BUS, O_RDWR);
    if (fd < 0)
    {
        std::cerr << "[GPS] Failed to open I2C bus: " << std::strerror(errno) << "\n";
        return false;
    }
    if (::ioctl(fd, I2C_SLAVE, GPS_I2C_ADDRESS) < 0)
    {
        std::cerr << "[GPS] Failed to set I2C address: " << std::strerror(errno) << "\n";
        ::close(fd);
        fd = -1;
        return false;
    }

    configureGPS();
    
    std::cout << "[GPS] Initialized successfully on " << GPS_I2C_BUS << " at address 0x" 
              << std::hex << GPS_I2C_ADDRESS << std::dec << "\n";
    return true;
}
 
void GPS::close()
{
    if (fd >= 0) { ::close(fd); fd = -1; }
}

bool GPS::sendUBX(const std::vector<uint8_t>& msg)
{
    if (fd < 0) return false;

    ssize_t written = write(fd, msg.data(), msg.size());
    if (written != (ssize_t)msg.size())
    {
        std::cerr << "[GPS] Failed to send UBX message\n";
        return false;
    }

    // small delay to let GPS process it
    usleep(100000); // 100ms
    return true;
}

// Configures the GPS module by sending a series of UBX messages to set the
// update rate and required sentence types.
bool GPS::configureGPS()
{   
    // Sets update rate to 10hz
    sendUBX({
        0xB5,0x62,0x06,0x08,0x06,0x00,
        0x64,0x00,
        0x01,0x00,
        0x01,0x00,
        0x7A,0x12
    });

    // Disables gll
    sendUBX({
        0xB5,0x62,0x06,0x01,0x08,0x00,
        0xF0,0x01,
        0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x2A
    });

    // Disables gsa
    sendUBX({
        0xB5,0x62,0x06,0x01,0x08,0x00,
        0xF0,0x02,
        0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x31
    });

    // Disables gsv
    sendUBX({
        0xB5,0x62,0x06,0x01,0x08,0x00,
        0xF0,0x03,
        0x00,0x00,0x00,0x00,0x00,0x00,
        0x02,0x38
    });

    // Disables vtg
    sendUBX({
        0xB5,0x62,0x06,0x01,0x08,0x00,
        0xF0,0x05,
        0x00,0x00,0x00,0x00,0x00,0x00,
        0x04,0x46
    });

    return true;
}
 
// Reads one NMEA line from the GPS module byte-by-byte over I2C.
// Mirrors Python's readGPS() accumulation loop + parseResponse() Check #3:
//   - 0xFF        → no data available, return false
//   - 0x0A        → end of line, return true
//   - bad ASCII   → discard the ENTIRE line (return false), matching Python's
//                   CharError check which drops any line with a single bad byte
bool GPS::readLine(std::string& line)
{
    line.clear();
 
    while (true)
    {
        uint8_t byte = 0;
        if (::read(fd, &byte, 1) != 1)
        {
            initialise();
            return false;
        }
 
        if (byte == 0xFF)  return false;   // c == 255: no data
        if (byte == 0x0A)  break;          // c == 10:  end of line
        if (byte == 0x0D)  continue;       // carriage return: skip
 
        // Check #3: if ANY byte is outside readable ASCII, discard whole line
        if (byte < 32 || byte > 122)
            return false;
 
        line += static_cast<char>(byte);
    }
 
    return !line.empty();
}
 
// Validates checksum and parses a GPRMC/GNRMC sentence into gpsData.
// Only populates the four fields we care about: validFix, latitude,
// longitude, speed.
bool GPS::parseNMEA(const std::string& line, GPSData& gpsData)
{
    // Check #1: '$' must appear exactly once
    if (std::count(line.begin(), line.end(), '$') != 1) return false;
 
    // Check #2: max NMEA sentence length is 83 chars
    if (line.length() >= 83) return false;
 
    // Check #3 is already enforced in readLine() — bad chars never reach here
 
    // Check #4: skip txbuf allocation errors
    if (line.find("txbuf") != std::string::npos) return false;
 
    // Check #5: split on '*' to isolate checksum
    const size_t starPos = line.find('*');
    if (starPos == std::string::npos) return false;
 
    const std::string sentence  = line.substr(0, starPos);
    const std::string chkSumStr = line.substr(starPos + 1);
 
    // XOR checksum over all chars between '$' and '*' (skip '$' at index 0)
    uint8_t calcChk = 0;
    for (size_t i = 1; i < sentence.size(); ++i)
        calcChk ^= static_cast<uint8_t>(sentence[i]);
 
    const uint8_t providedChk =
        static_cast<uint8_t>(std::strtol(chkSumStr.c_str(), nullptr, 16));
 
    if (calcChk != providedChk) return false;
 
    // Parse GPRMC / GNRMC — the only sentence type that carries all four
    // fields we need: validFix, latitude, longitude, speed.
    // Fields: type, time, status, lat, N/S, lon, E/W, speed(knots), course, date
    const std::vector<std::string> fields = splitString(sentence, ',');
    if (fields.size() < 8) return false;
 
    const std::string& msgType = fields[0];
    if (msgType != "$GPRMC" && msgType != "$GNRMC") return false;

    // Need at least up to the date field (index 9) to derive a timestamp.
    if (fields.size() < 10) return false;

    gpsData.validFix  = (fields[2] == "A");
    gpsData.latitude  = nmeaToDecimalDegrees(fields[3], fields[4]);
    gpsData.longitude = nmeaToDecimalDegrees(fields[5], fields[6]);
    gpsData.speed     = fields[7].empty() ? 0.0 : std::stod(fields[7]) * 1.852; // knots => km/h
    gpsData.timeMs    = nmeaToEpochMs(fields[1], fields[9]); // UTC time + date => epoch ms

    return true;
}
 
GPSData GPS::readData()
{
    GPSData data{};
    data.validFix = false;
 
    if (fd >= 0)
    {
        std::string line;
        if (readLine(line))
            parseNMEA(line, data);
    }
 
    return data;
}