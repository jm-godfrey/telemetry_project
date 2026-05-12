#include "../include/accelerometer.hpp"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstdint>

// I2C setup
static const char* I2C_DEVICE = "/dev/i2c-1";
static const int ACC_ADDRESS = 0x6A; // LSM6DSL address

// LSM6DSL registers
static const uint8_t CTRL1_XL = 0x10;
static const uint8_t CTRL3_C  = 0x12;

static const uint8_t OUTX_L_XL = 0x28;
static const uint8_t OUTY_L_XL = 0x2A;
static const uint8_t OUTZ_L_XL = 0x2C;


// Constructor
Accelerometer::Accelerometer() : fd(-1) {}


// Destructor
Accelerometer::~Accelerometer()
{
    close();
}


// Initialise I2C and configure accelerometer
bool Accelerometer::initialise()
{
    fd = open(I2C_DEVICE, O_RDWR);
    if (fd < 0)
    {
        std::cerr << "Failed to open I2C bus\n";
        return false;
    }

    if (ioctl(fd, I2C_SLAVE, ACC_ADDRESS) < 0)
    {
        std::cerr << "Failed to set accelerometer address\n";
        close();
        fd = -1;
        return false;
    }

    // Configure accelerometer:
    // 104 Hz, ±8g, BW = 50 Hz (stable for now)
    if (!writeRegister(CTRL1_XL, 0b01001100))
    {
        std::cerr << "Failed to configure accelerometer\n";
        return false;
    }

    // Enable auto-increment (important for multi-byte reads)
    if (!writeRegister(CTRL3_C, 0b01000100))
    {
        std::cerr << "Failed to configure CTRL3_C\n";
        return false;
    }

    std::cout << "Accelerometer initialised\n";
    return true;
}


// Read full accelerometer data
AccelerometerData Accelerometer::readData()
{
    AccelerometerData data{};

    if (fd < 0) return data;

    int16_t rawX = readAxis(OUTX_L_XL);
    int16_t rawY = readAxis(OUTY_L_XL);
    int16_t rawZ = readAxis(OUTZ_L_XL);

    // Convert to g (±8g → 0.244 mg/LSB)
    const float scale = 0.000244f;

    data.accelX = rawX * scale;
    data.accelY = rawY * scale;
    data.accelZ = rawZ * scale;

    return data;
}


// Write a register
bool Accelerometer::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};

    if (write(fd, buffer, 2) != 2)
    {
        return false;
    }

    return true;
}


// Read a single register
uint8_t Accelerometer::readRegister(uint8_t reg)
{
    if (write(fd, &reg, 1) != 1)
    {
        return 0;
    }

    uint8_t value;
    if (read(fd, &value, 1) != 1)
    {
        return 0;
    }

    return value;
}


// Read one axis (2 bytes → 16-bit signed)
int16_t Accelerometer::readAxis(uint8_t lowReg)
{
    uint8_t buffer[2];

    // Tell device which register to read from
    if (write(fd, &lowReg, 1) != 1)
    {
        return 0;
    }

    // Read 2 bytes (low + high)
    if (read(fd, buffer, 2) != 2)
    {
        return 0;
    }

    // Combine into signed 16-bit
    return (int16_t)(buffer[0] | (buffer[1] << 8));
}


// Close I2C
void Accelerometer::close()
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}