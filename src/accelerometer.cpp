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
static const uint8_t CTRL2_G  = 0x11;
static const uint8_t CTRL3_C  = 0x12;
static const uint8_t CTRL4_C  = 0x13;
static const uint8_t CTRL6_C  = 0x15;
static const uint8_t CTRL8_XL = 0x17;

static const uint8_t OUTX_L_G = 0x22; // gyro XYZ, followed directly by accel XYZ


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
    // 416 Hz, ±8g
    if (!writeRegister(CTRL1_XL, 0b01101100))
    {
        std::cerr << "Failed to configure accelerometer\n";
        return false;
    }

    // Configure gyro:
    // 416 Hz, ±1000 deg/s
    if (!writeRegister(CTRL2_G, 0b01101000))
    {
        std::cerr << "Failed to configure gyro\n";
        return false;
    }

    // Enable auto-increment for multi-byte reads
    if (!writeRegister(CTRL3_C, 0b01000100))
    {
        std::cerr << "Failed to configure CTRL3_C\n";
        return false;
    }

    // On-chip low-pass filters stop vibration aliasing into the 100 Hz reads:
    // gyro LPF1 at its strongest setting, accel LPF2 at ODR/9 (~46 Hz)
    if (!writeRegister(CTRL4_C, 0b00000010) ||
        !writeRegister(CTRL6_C, 0b00000011) ||
        !writeRegister(CTRL8_XL, 0b11001000))
    {
        std::cerr << "Failed to configure low-pass filters\n";
        return false;
    }

    std::cout << "Accelerometer initialised\n";
    return true;
}

bool Accelerometer::readData(AccelerometerData& data)
{
    if (fd < 0) return false;

    uint8_t reg = OUTX_L_G;
    uint8_t buffer[12];

    // One burst so all six values come from the same sample
    if (write(fd, &reg, 1) != 1) return false;
    if (read(fd, buffer, 12) != 12) return false;

    // Combine low + high bytes into signed 16-bit
    int16_t raw[6];
    for (int i = 0; i < 6; i++)
        raw[i] = (int16_t)(buffer[i * 2] | (buffer[i * 2 + 1] << 8));

    // Convert to deg/s (±1000 deg/s => 35 mdps/LSB)
    const float gyroScale = 0.035f;

    data.gyroX = raw[0] * gyroScale;
    data.gyroY = raw[1] * gyroScale;
    data.gyroZ = raw[2] * gyroScale;

    // Convert to g (±8g => 0.244 mg/LSB)
    const float accelScale = 0.000244f;

    data.accelX = raw[3] * accelScale;
    data.accelY = raw[4] * accelScale;
    data.accelZ = raw[5] * accelScale;

    return true;
}


// Write to a register for configuration
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


// Close I2C connection
void Accelerometer::close()
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}