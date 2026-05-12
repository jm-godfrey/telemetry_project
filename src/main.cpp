#include "../include/gps.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main()
{
    cout << "Hello, Telemetry!" << endl;

    GPS gps;

    gps.initialise();

    while (true)
    {
        GPSData data = gps.readData();

        if (data.validFix)
        {
            cout << "Latitude: " << data.latitude << endl;
            cout << " Longitude: " << data.longitude << endl;
            cout << "Speed: " << data.speed << " m/s" << endl;
        }
 
    }

    return 0;
}
