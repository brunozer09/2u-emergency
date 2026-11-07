#include "BatteryMonitor.h"

#include <Wire.h>
#include <math.h>

namespace {

// Faixa do shunt na calibração de 32 V e 2 A. Medida no limite significa saturação:
// a corrente real é desconhecida.
constexpr float kShuntLimitMv = 320.0f;
constexpr float kMaxBusVoltageV = 32.0f;
constexpr float kMvPerV = 1000.0f;

// Depois de entrar em um estado, a tensão precisa subir além da histerese para sair
// dele, assim uma tensão oscilando no limite não gera uma sequência de avisos.
BatteryState nextState(BatteryState current, float voltageV) {
  const bool wasCritical = current == BatteryState::kCritical;
  const bool wasLow = current == BatteryState::kLow || wasCritical;
  const float criticalV =
      config::kBatteryCriticalV + (wasCritical ? config::kBatteryHysteresisV : 0.0f);
  const float lowV = config::kBatteryLowV + (wasLow ? config::kBatteryHysteresisV : 0.0f);

  if (voltageV < criticalV) {
    return BatteryState::kCritical;
  }
  return voltageV < lowV ? BatteryState::kLow : BatteryState::kOk;
}

}  // namespace

bool BatteryMonitor::begin() {
  if (!ina219_.begin(&Wire)) {
    return false;
  }
  ina219_.setCalibration_32V_2A();  // Assume o shunt de 0,1 ohm dos módulos comuns
  return ina219_.success();
}

void BatteryMonitor::sample() {
  // success() informa apenas a última operação, então é conferido após cada leitura
  const float busV = ina219_.getBusVoltage_V();
  const bool busOk = ina219_.success();
  const float shuntMv = ina219_.getShuntVoltage_mV();
  const bool shuntOk = ina219_.success();
  const float currentMa = ina219_.getCurrent_mA();
  const bool currentOk = ina219_.success();

  const bool shuntInRange = shuntOk && fabsf(shuntMv) < kShuntLimitMv;

  // O barramento é medido depois do shunt, então a queda nele é somada de volta
  data_.voltageV = busV + shuntMv / kMvPerV;
  data_.currentMa = currentMa;
  data_.powerMw = data_.voltageV * currentMa;
  data_.voltageValid = busOk && shuntInRange && busV >= 0.0f && busV <= kMaxBusVoltageV;
  data_.currentValid = currentOk && shuntInRange;
  data_.powerValid = data_.voltageValid && data_.currentValid;
  data_.state =
      data_.voltageValid ? nextState(data_.state, data_.voltageV) : BatteryState::kUnknown;
}
