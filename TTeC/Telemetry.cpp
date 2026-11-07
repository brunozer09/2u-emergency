#include "Telemetry.h"

#include <stdarg.h>
#include <stdio.h>

#include "ProjectConfig.h"

namespace {
constexpr char kHeader[] =
    "id,temp_c,press_hpa,hum_pct,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,"
    "mx_ut,my_ut,mz_ut,lat_deg,lon_deg,alt_m,sats,bat_v,bat_ma,bat_mw\n";

constexpr uint8_t kAxes = 3;
constexpr uint8_t kDecimalsEnvironment = 2;
constexpr uint8_t kDecimalsAccel = 4;
constexpr uint8_t kDecimalsGyro = 2;
constexpr uint8_t kDecimalsMag = 1;
constexpr uint8_t kDecimalsCoordinate = 6;
constexpr uint8_t kDecimalsAltitude = 1;
constexpr uint8_t kDecimalsBatteryVoltage = 3;
constexpr uint8_t kDecimalsBatteryCurrent = 1;
constexpr uint8_t kDecimalsBatteryPower = 1;
}  // namespace

void Telemetry::begin() {
  serial_.setTxBufferSize(config::kTelemetryTxBufferBytes);  // Precisa vir antes do begin()
  serial_.begin(config::kTelemetryBaud, SERIAL_8N1, config::kTelemetryRxPin,
                config::kTelemetryTxPin);
}

void Telemetry::sendHeader() {
  serial_.write(kHeader, sizeof(kHeader) - 1);
}

void Telemetry::sendSample(const BarometerData& barometer, const ImuData& imu,
                           const GpsData& gps, const BatteryData& battery) {
  lineLength_ = 0;
  lineOverflow_ = false;

  // O id avança mesmo se a linha for descartada, para o salto aparecer no receptor
  const uint32_t id = nextId_++;
  appendUint(id, true);

  appendFloat(barometer.temperatureC, kDecimalsEnvironment, barometer.temperatureValid);
  appendFloat(barometer.pressureHpa, kDecimalsEnvironment, barometer.pressureValid);
  appendFloat(barometer.humidityPct, kDecimalsEnvironment, barometer.humidityValid);

  for (uint8_t axis = 0; axis < kAxes; ++axis) {
    appendFloat(imu.accelG[axis], kDecimalsAccel, imu.motionValid);
  }
  for (uint8_t axis = 0; axis < kAxes; ++axis) {
    appendFloat(imu.gyroDps[axis], kDecimalsGyro, imu.motionValid);
  }
  for (uint8_t axis = 0; axis < kAxes; ++axis) {
    appendFloat(imu.magUt[axis], kDecimalsMag, imu.magValid);
  }

  appendFloat(gps.latitudeDeg, kDecimalsCoordinate, gps.positionValid);
  appendFloat(gps.longitudeDeg, kDecimalsCoordinate, gps.positionValid);
  appendFloat(gps.altitudeM, kDecimalsAltitude, gps.altitudeValid);
  appendUint(gps.satellites, gps.satellitesValid);

  appendFloat(battery.voltageV, kDecimalsBatteryVoltage, battery.voltageValid);
  appendFloat(battery.currentMa, kDecimalsBatteryCurrent, battery.currentValid);
  appendFloat(battery.powerMw, kDecimalsBatteryPower, battery.powerValid);

  appendFormatted("\n");

  if (lineOverflow_) {
    log("linha %lu descartada: buffer insuficiente", static_cast<unsigned long>(id));
    return;
  }
  serial_.write(line_, lineLength_);
}

void Telemetry::log(const char* format, ...) {
  char message[kLogBytes];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  serial_.print("# ");
  serial_.println(message);
}

void Telemetry::appendUint(uint32_t value, bool valid) {
  if (lineLength_ > 0) {
    appendSeparator();
  }
  if (valid) {
    appendFormatted("%lu", static_cast<unsigned long>(value));
  }
}

void Telemetry::appendFloat(double value, uint8_t decimals, bool valid) {
  appendSeparator();
  if (valid) {
    appendFormatted("%.*f", decimals, value);
  }
}

void Telemetry::appendSeparator() {
  appendFormatted(",");
}

void Telemetry::appendFormatted(const char* format, ...) {
  if (lineOverflow_) {
    return;
  }

  const size_t remaining = sizeof(line_) - lineLength_;
  va_list args;
  va_start(args, format);
  const int written = vsnprintf(line_ + lineLength_, remaining, format, args);
  va_end(args);

  if (written < 0 || static_cast<size_t>(written) >= remaining) {
    lineOverflow_ = true;
    return;
  }
  lineLength_ += static_cast<size_t>(written);
}
