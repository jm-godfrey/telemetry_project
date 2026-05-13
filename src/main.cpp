#include "../include/gps.hpp"
#include "../include/accelerometer.hpp"
#include "../include/logger.hpp"
#include "../include/telemetry_data.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

int main()
{
    cout << "Hello, Telemetry!" << endl;

    GPS gps;
    Accelerometer accel;

    gps.initialise();
    accel.initialise();

    Logger logger;
    logger.openLogFile();

    int count = 0;

    while (true)
    {   

        GPSData data = gps.readData();

        //cout << "GPS Fix: " << (data.validFix ? "Yes" : "No") << endl;
        if (data.validFix)
        {   
            cout << "Data point: " << count << endl;
            cout << endl;
            count++;
            cout << "Latitude: " << data.latitude << endl;
            cout << "Longitude: " << data.longitude << endl;
            cout << "Speed: " << data.speed << " m/s" << endl;

            AccelerometerData accelData = accel.readData();

            cout << "-----------------------------\n";

            cout << "Accel X: " << accelData.accelX << " g" << endl;
            cout << "Accel Y: " << accelData.accelY << " g" << endl;
            cout << "Accel Z: " << accelData.accelZ << " g" << endl;

            cout << "=============================\n";

            TelemetryData telemetry;
            
            telemetry.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            telemetry.gps = data;
            telemetry.accelerometer = accelData;

            logger.logData(telemetry);

        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
