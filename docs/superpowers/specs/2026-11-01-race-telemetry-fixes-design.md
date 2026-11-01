# Race telemetry fixes

Scope agreed with James on 2026-11-01. It covers the logger changes needed before the next race. Anything that can be done to the data afterwards is left out, so the calibration tool, mounting rotation and the Rails "small g" issue are not part of this work.

## Problem

- The GPS module on I²C produces about 20 lines/s (RMC and GGA), but the logger reads 10. The module's buffer stays full, so GPS data on each row is about 9.5 s (95 rows) older than the IMU data.
- The IMU thread reads at 100 Hz but keeps only the newest sample, so the logger keeps about 1 in 10. Vibration aliases into the CSV and can't be removed later.
- The gyro is not enabled.
- HEAD uses the knots-to-mph factor for km/h. Deployed logs still use 1.852, so the Pi predates this bug.

## GPS

- Restore `KNOTS_TO_KMH = 1.852`.
- Each pass of the GPS thread reads byte by byte until the module returns `0xFF`, then sleeps 20 ms.
- A partial line is kept between passes.
- The newest valid RMC from a pass is published, stamped with `rxTime` from `steady_clock`.
- GGA is disabled at startup, as nothing parses it.
- The backlog is flushed at the end of `initialise()`, before logging starts.
- An I²C read error closes the old `fd` before reconnecting.
- If the fix is lost, rows keep the last fix and `fix_age_ms` climbs.

## IMU and gyro

Register values checked against ST's `lsm6dsl-pid` driver:

| Register | Value | Meaning |
|---|---|---|
| `CTRL1_XL` | `0x6C` | 416 Hz, ±8 g |
| `CTRL2_G` | `0x68` | 416 Hz, ±1000 °/s |
| `CTRL3_C` | `0x44` | BDU and auto-increment (unchanged) |
| `CTRL4_C` | `0x02` | Gyro LPF1 on |
| `CTRL6_C` | `0x03` | Gyro LPF1 strongest setting (FTYPE 3) |
| `CTRL8_XL` | `0xC8` | Accel LPF2 on, ODR/9 (about 46 Hz), low-noise path |

- Scales: 0.244 mg/LSB and 35 mdps/LSB.
- One 12-byte burst from `OUTX_L_G` (`0x22`) reads gyro then accel.
- The IMU thread adds each good sample to an accumulator at about 100 Hz. Failed reads are skipped.
- The logger takes the mean for each row and resets the accumulator. A row with no good samples repeats the previous mean.
- Values stay in the sensor frame, with no rotation applied.

## CSV

```
timestamp,lat,lon,speed,heading,accelX,accelY,accelZ,gyroX,gyroY,gyroZ,t_ms,fix_age_ms
```

- The first 8 columns are unchanged.
- `gyroX/Y/Z`: °/s.
- `t_ms`: elapsed ms since the first logged row, from `steady_clock`.
- `fix_age_ms`: ms between the fix's receipt and the row being written.
- The Rails app is not changed. New columns stay in the CSV for now.

## Testing

- This machine: `g++` build with warnings on, plus `tests/test_telemetry.cpp` for line reassembly and averaging.
- James tests on the Pi once, after all the work is done. The stage-by-stage tests were dropped at James's request to save time.

Pi checks, before the race:

1. With the old build running, `i2ctransfer -y 1 w1@0x42 0xFD r2` shows a backlog around 6,800 bytes. With the new build it stays small.
2. Starting the logger after the GPS has been powered for several minutes doesn't log old data.
3. 30 s stationary: accel magnitude about 1.00 g, gyro near 0 °/s.
4. Short drive: GPS and IMU line up within 1 row, speed looks right, accel noise below about 0.1 g, `t_ms` always rises, `fix_age_ms` below about 150 ms.
5. A new CSV imports into Rails.

## Rollout

Work is on the `race-telemetry-fixes` branch, one commit per change. The tag `gps-fix-only` marks the GPS work before the IMU changes, as a fallback.

## Known limits

- `timestamp` can still step back 1 s when the receiver corrects its clock. `t_ms` is unaffected.
- A GPS fix can be up to about 100 ms older than its row, plus delivery delay. `fix_age_ms` records this.
- The gyro's filter cutoff sits above the 50 Hz limit for 100 Hz reads, so some vibration above that limit can still alias in the gyro columns.
- If reconnecting to the bus fails outright, GPS stays down until the logger restarts.
