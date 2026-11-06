// GPS GY-GPS6MV2: decodifica NMEA da UART e guarda a última posição válida.
#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>

struct GpsData {
  double latitudeDeg = 0.0;
  double longitudeDeg = 0.0;
  double altitudeM = 0.0;
  uint32_t satellites = 0;
  bool positionValid = false;
  bool altitudeValid = false;
  bool satellitesValid = false;
};

class Gps {
 public:
  explicit Gps(HardwareSerial& serial) : serial_(serial) {}

  void begin();
  void poll();    // Esvazia o buffer da UART: chamar a cada iteração do loop()
  void sample();  // Atualiza data() com o estado atual do decodificador

  const GpsData& data() const { return data_; }
  bool hasReceivedData() const { return parser_.charsProcessed() > 0; }

 private:
  HardwareSerial& serial_;
  TinyGPSPlus parser_;
  GpsData data_;
};
