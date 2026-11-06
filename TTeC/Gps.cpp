#include "Gps.h"

#include "ProjectConfig.h"

void Gps::begin() {
  serial_.setRxBufferSize(config::kGpsRxBufferBytes);  // Precisa vir antes do begin()
  serial_.begin(config::kGpsBaud, SERIAL_8N1, config::kGpsRxPin, config::kGpsTxPin);
}

void Gps::poll() {
  while (serial_.available() > 0) {
    parser_.encode(static_cast<char>(serial_.read()));
  }
}

void Gps::sample() {
  // isValid() continua verdadeiro depois de perder o fix, por isso a idade é conferida
  data_.positionValid =
      parser_.location.isValid() && parser_.location.age() <= config::kGpsMaxDataAgeMs;
  data_.altitudeValid =
      parser_.altitude.isValid() && parser_.altitude.age() <= config::kGpsMaxDataAgeMs;
  data_.satellitesValid =
      parser_.satellites.isValid() && parser_.satellites.age() <= config::kGpsMaxDataAgeMs;

  if (data_.positionValid) {
    data_.latitudeDeg = parser_.location.lat();
    data_.longitudeDeg = parser_.location.lng();
  }
  if (data_.altitudeValid) {
    data_.altitudeM = parser_.altitude.meters();
  }
  if (data_.satellitesValid) {
    data_.satellites = parser_.satellites.value();
  }
}
