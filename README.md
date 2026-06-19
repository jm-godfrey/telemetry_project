# Telemetry Project

A lightweight, multithreaded C++ telemetry logger for the **Raspberry Pi 5**. It reads
live data from a GPS receiver and an accelerometer/IMU over I²C, fuses the readings into
a single timestamped record, and streams them to a timestamped CSV file for later
analysis (e.g. plotting a route, reviewing speed, or analysing vehicle dynamics).

---

## Purpose

The goal of this project is to capture real-world motion telemetry from sensors physically
attached to a Raspberry Pi 5 — for example mounted in a vehicle, on a bike, or on any moving
platform. Each run produces a self-contained CSV log that can be imported into Python,
Excel, MATLAB, or a mapping tool to reconstruct where the device went, how fast it was
moving, and what accelerations it experienced.

Typical use cases:

- Recording a GPS track (latitude / longitude / speed) for a journey.
- Logging acceleration (X/Y/Z in g) to study braking, cornering, and vibration.
- Building a dataset for offline data analysis or visualisation.

---

## Hardware

| Component        | Detail                                                              |
|------------------|---------------------------------------------------------------------|
| Compute          | Raspberry Pi 5 (Raspberry Pi OS / any Linux with I²C support)       |
| GPS receiver     | u-blox GPS module on I²C, address `0x42`, configured via UBX        |
| Accelerometer/IMU| STMicroelectronics **LSM6DSL** on I²C, address `0x6A`               |
| Bus              | I²C bus 1 — `/dev/i2c-1` (Pi's default I²C header: GPIO 2 / GPIO 3) |

Both sensors share the I²C-1 bus. Make sure I²C is enabled on the Pi (see
[Enabling I²C](#enabling-i²c) below).

---

## Output

On startup the logger creates one CSV file per run inside `data/logs/`, named with the
local start time:

```
data/logs/telemetry_log_YYYYMMDD_HHMMSS.csv
```

The file begins with a header row, followed by one row per logged sample:

```csv
timestamp,lat,lon,speed,accelX,accelY,accelZ
1718719200123,53.480100,-2.242600,42.318,0.012,-0.984,0.027
```

| Column      | Units                | Source / notes                                            |
|-------------|----------------------|-----------------------------------------------------------|
| `timestamp` | Unix ms (UTC epoch)  | UTC date + time from the GPS `RMC` sentence (the Pi has no reliable clock offline) |
| `lat`       | decimal degrees      | From GPS `$GxRMC` sentence (negative = South)             |
| `lon`       | decimal degrees      | From GPS `$GxRMC` sentence (negative = West)              |
| `speed`     | km/h                 | GPS speed-over-ground, converted from knots (× 1.852)     |
| `accelX`    | g                    | LSM6DSL X axis                                            |
| `accelY`    | g                    | LSM6DSL Y axis                                            |
| `accelZ`    | g                    | LSM6DSL Z axis (≈ −1 g at rest, depending on orientation) |

Rows are written at **~10 Hz** (about 10 samples per second). Logging does **not**
begin until the GPS has its first valid fix, since the timestamp is taken from the
GPS (see [Timestamps](#timestamps) below). Any accelerometer data captured before
that first fix is not logged.

---

## How It Works

### Architecture

The program is split into three sensor/IO concerns, each owned by its own class, and three
worker threads that drive them. A single shared `TelemetryData` struct is the meeting point
between threads, protected by a mutex.

```
            ┌────────────────────┐
   I²C ───► │  GPS (0x42)        │ ──► gpsThreadFunc  ─┐
            └────────────────────┘     (~10 Hz)        │   writes gps fields
                                                       ▼
            ┌────────────────────┐              ┌──────────────────┐
   I²C ───► │  Accelerometer     │ ──► imuThread │  TelemetryData   │  (shared,
            │  LSM6DSL (0x6A)    │     (~100 Hz) │  + std::mutex    │   mutex-guarded)
            └────────────────────┘              └──────────────────┘
                                                       │   reads snapshot
                                                       ▼
                                          loggerThreadFunc (~10 Hz)
                                                       │
                                                       ▼
                                          data/logs/telemetry_log_*.csv
```

### Threads

| Thread                | Rate    | Responsibility                                                              |
|-----------------------|---------|-----------------------------------------------------------------------------|
| `imuThreadFunc`       | ~100 Hz | Read the accelerometer and write the latest values into `sharedData`.       |
| `gpsThreadFunc`       | ~10 Hz  | Read/parse an NMEA line; on a *valid fix*, write GPS values into `sharedData`. |
| `loggerThreadFunc`    | ~10 Hz  | Copy a snapshot of `sharedData`; once a valid GPS fix is available, stamp the row with the GPS UTC time, write a CSV row, and print a debug summary. Skips logging until the first fix. |

Each thread paces itself with `std::this_thread::sleep_until(start + interval)` so the loop
period stays stable regardless of how long the work took. Access to the shared struct is
serialised with a `std::lock_guard<std::mutex>`, and an `std::atomic<bool> running` flag is
the shutdown signal.

The logger thread reads whatever the GPS and IMU threads have most recently published, so
each CSV row reflects the freshest sensor state. The **timestamp** for each row comes from the
GPS, not from the logger — see [Timestamps](#timestamps) below.

### Timestamps

The Raspberry Pi 5 has no battery-backed real-time clock, and in a vehicle it has no internet
connection, so NTP never syncs and the system clock cannot be trusted. Instead of
`std::chrono::system_clock`, each CSV row is timestamped from the **UTC time and date carried
in the GPS `RMC` sentence**, converted to Unix epoch milliseconds (via `timegm`, so it is true
UTC regardless of the Pi's timezone).

Consequences of this approach:

- **Nothing is logged until the first valid GPS fix** — the logger thread skips a row whenever
  there is no valid fix or no parsed GPS time.
- Because the logger (~10 Hz) and the GPS (~10 Hz) are not phase-locked, consecutive rows can
  occasionally share a timestamp, and during a fix loss (e.g. a tunnel) the last fix's time
  persists until a fresh fix arrives.

### GPS details

The u-blox module has no flash memory, so it is reconfigured on every startup
(`configureGPS()`) by sending raw **UBX** binary commands over I²C:

- Set the navigation/measurement rate to **10 Hz**.
- Disable the `GLL`, `GSA`, `GSV`, and `VTG` NMEA sentences to reduce bus traffic, leaving
  the `RMC` sentence that carries position, speed, and fix status.

Reading (`readData → readLine → parseNMEA`) accumulates one NMEA line byte-by-byte and then
validates it:

- `0xFF` means "no data available", `0x0A` ends a line, `0x0D` is skipped, and any byte
  outside printable ASCII causes the whole line to be discarded.
- A valid line must contain exactly one `$`, be under 83 characters, and pass an **XOR
  checksum** over the characters between `$` and `*`.
- Only `$GPRMC` / `$GNRMC` sentences are parsed. NMEA `ddmm.mmmm` coordinates are converted
  to decimal degrees, and speed is converted from knots to km/h.
- The RMC **UTC time** (`hhmmss.ss`) and **date** (`ddmmyy`) fields are also parsed and combined
  into Unix epoch milliseconds, which becomes the timestamp for each logged row (see
  [Timestamps](#timestamps)).

### Accelerometer details

The **LSM6DSL** is configured over I²C in `initialise()`:

- `CTRL1_XL = 0b01001100` → output data rate **104 Hz**, full scale **±8 g**, 50 Hz bandwidth.
- `CTRL3_C = 0b01000100` → enable register auto-increment for multi-byte reads (BDU set).

Each axis is read as two bytes (low + high) combined into a signed 16-bit value, then scaled
by **0.000244 g/LSB** (the ±8 g sensitivity) to produce values in g.

---

## Project Structure

```
telemetry_project/
├── CMakeLists.txt              # CMake build definition (C++17)
├── include/
│   ├── telemetry_data.hpp      # Shared data structs (GPSData, AccelerometerData, TelemetryData)
│   ├── gps.hpp                 # GPS class interface
│   ├── accelerometer.hpp       # Accelerometer (LSM6DSL) class interface
│   ├── logger.hpp              # CSV logger interface
│   └── threads/                # Thread entry-point declarations
│       ├── gps_thread.hpp
│       ├── imu_thread.hpp
│       └── logger_thread.hpp
├── src/
│   ├── main.cpp                # Entry point: init sensors, spawn threads
│   ├── gps.cpp                 # I²C + UBX config + NMEA parsing
│   ├── accelerometer.cpp       # I²C register read/write + scaling
│   ├── logger.cpp              # Timestamped CSV file creation and writing
│   └── threads/                # Thread loop implementations
│       ├── gps_thread.cpp
│       ├── imu_thread.cpp
│       └── logger_thread.cpp
└── data/
    └── logs/                   # CSV output files are written here
```

---

## Requirements

- Raspberry Pi 5 running Linux (Raspberry Pi OS recommended).
- A C++17-capable compiler (`g++` ≥ 8, ships with Raspberry Pi OS).
- **CMake ≥ 3.15**.
- I²C enabled, with the two sensors wired to the I²C-1 header.

### Enabling I²C

```bash
sudo raspi-config        # Interface Options → I2C → Enable, then reboot
sudo apt update
sudo apt install -y i2c-tools cmake build-essential
```

Verify both sensors are visible on the bus (you should see `42` and `6a`):

```bash
i2cdetect -y 1
```

---

## Build

The project uses CMake. A standard out-of-source build:

```bash
cd telemetry_project
mkdir -p build
cd build
cmake ..
cmake --build .
```

This produces the `telemetry_project` executable inside `build/`.

> The program uses `std::thread`, which on Linux requires the pthreads library. This is
> handled in `CMakeLists.txt` via `find_package(Threads REQUIRED)` and
> `target_link_libraries(... Threads::Threads)`, so it links correctly across toolchains.

---

## Run

The logger writes to an **absolute** path,
`/home/jgodfrey/telemetry_project/data/logs/`, and creates that directory if it does not
already exist. Because the path no longer depends on the working directory, the executable can
be launched from anywhere — including a `systemd` unit at startup:

```bash
./build/telemetry_project
```

On startup you should see the sensors initialise and the log file path printed:

```
Hello, Telemetry!
[GPS] Initialized successfully on /dev/i2c-1 at address 0x42
Accelerometer initialised
[Logger] Logging to: /home/jgodfrey/telemetry_project/data/logs/telemetry_log_20260618_142600.csv
```

The console then prints a live summary of each logged data point. The CSV file grows in
`data/logs/` for the duration of the run. Press **Ctrl-C** to stop: the program catches the
signal, lets the worker threads finish their current iteration, joins them, and closes the
log file cleanly before exiting.

> Accessing `/dev/i2c-1` may require membership of the `i2c` group (default on Raspberry Pi
> OS) or running with `sudo`.

---

## Known Limitations / Notes

These are honest observations from the current state of the code, useful to anyone extending it:

- **The log path is hard-coded** to `/home/jgodfrey/telemetry_project/data/logs/` in
  `src/logger.cpp`. The logger creates the directory automatically on startup, so it no longer
  needs to exist in advance — but if you move the project you must update that path.
- **Logging is gated on a valid GPS fix.** Because the timestamp is GPS-sourced, nothing is
  written to the CSV until the receiver gets its first fix; accelerometer data captured during
  the warm-up is not logged.
- **No external library dependencies** beyond the C++ standard library and the Linux I²C
  kernel interface (`<linux/i2c-dev.h>`), keeping the build self-contained.
