#include "../include/gps.hpp"
 
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
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
    return true;
}
 
void GPS::close()
{
    if (fd >= 0) { ::close(fd); fd = -1; }
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
            initialise(); // mirrors Python's `except IOError: connectBus()`
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
// longitude, speed. All five of Python's parseResponse() checks are kept.
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

    gpsData.validFix  = (fields[2] == "A");
    gpsData.latitude  = nmeaToDecimalDegrees(fields[3], fields[4]);
    gpsData.longitude = nmeaToDecimalDegrees(fields[5], fields[6]);
    gpsData.speed     = fields[7].empty() ? 0.0 : std::stod(fields[7]) * 1.852; // knots → km/h
 
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