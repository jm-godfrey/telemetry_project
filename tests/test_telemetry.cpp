// Hardware-free checks for the GPS line handling. Not part of the CMake build.
// g++ -std=c++17 -Iinclude tests/test_telemetry.cpp src/gps.cpp -o /tmp/test_telemetry && /tmp/test_telemetry

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <unistd.h>

#define private public // readData() reads from fd, so the tests swap it for a pipe
#include "gps.hpp"
#undef private

// Adds the $, checksum and line ending to a sentence body
static std::string sentence(const std::string& body)
{
    uint8_t chk = 0;
    for (char c : body) chk ^= static_cast<uint8_t>(c);
    char tail[8];
    std::snprintf(tail, sizeof(tail), "*%02X\r\n", chk);
    return "$" + body + tail;
}

static std::string rmc(const std::string& time, char status = 'A')
{
    return sentence("GNRMC," + time + "," + status +
                    ",5204.62630,N,00101.08703,W,10.000,90.00,111026,,,A");
}

// Queues bytes followed by the 0xFF the module sends when it has no more data
static void send(int writeFd, const std::string& bytes)
{
    const std::string data = bytes + '\xFF';
    assert(::write(writeFd, data.data(), data.size()) == static_cast<ssize_t>(data.size()));
}

static void testGps()
{
    int p[2];
    assert(::pipe(p) == 0);
    GPS gps;
    gps.fd = p[0];

    // Every waiting line is read and the newest fix wins
    send(p[1], rmc("161800.10") + sentence("GNGGA,161800.20,ignored") + rmc("161800.30"));
    GPSData d = gps.readData();
    assert(d.validFix);
    assert(d.timeMs % 1000 == 300);
    assert(d.rxTime.time_since_epoch().count() != 0);
    assert(std::fabs(d.speed - 18.52) < 1e-9); // 10 knots in km/h
    assert(std::fabs(d.latitude - 52.0771050) < 1e-6);

    // A line split across two passes survives
    const std::string split = rmc("161801.40");
    send(p[1], split.substr(0, 20));
    assert(!gps.readData().validFix);
    send(p[1], split.substr(20));
    d = gps.readData();
    assert(d.validFix);
    assert(d.timeMs % 1000 == 400);

    // A no-fix sentence doesn't wipe the good fix from the same pass
    send(p[1], rmc("161802.50") + rmc("161802.60", 'V'));
    d = gps.readData();
    assert(d.validFix);
    assert(d.timeMs % 1000 == 500);

    // A corrupted sentence is rejected
    std::string bad = rmc("161803.70");
    bad[12] = '9';
    send(p[1], bad);
    assert(!gps.readData().validFix);

    // A nothing-waiting pass reports no fix
    send(p[1], "");
    assert(!gps.readData().validFix);

    ::close(p[1]);
}

int main()
{
    testGps();
    std::puts("All telemetry tests passed");
}
