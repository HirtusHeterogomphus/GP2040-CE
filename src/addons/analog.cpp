#include "addons/analog.h"
#include "config.pb.h"
#include "drivermanager.h"
#include "enums.pb.h"
#include "hardware/adc.h"
#include "helper.h"
#include "storagemanager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

constexpr uint16_t ADC_MAX = ((1 << 12) - 1); // 4095
constexpr uint16_t ADC_NEUTRAL = 2048;
constexpr int ADC_PIN_OFFSET = 26;

bool invertX(InvertMode invertMode) {
  return invertMode == InvertMode::INVERT_X ||
         invertMode == InvertMode::INVERT_XY;
}

bool invertY(InvertMode invertMode) {
  return invertMode == InvertMode::INVERT_Y ||
         invertMode == InvertMode::INVERT_XY;
}

uint16_t clampAdc(int32_t value) {
  return static_cast<uint16_t>(std::clamp(
      value,
      static_cast<int32_t>(0),
      static_cast<int32_t>(ADC_MAX)));
}

uint16_t clampAdc(uint32_t value) {
  return static_cast<uint16_t>(std::min<uint32_t>(value, ADC_MAX));
}

uint16_t clampNeutral(uint16_t value) {
  return static_cast<uint16_t>(std::clamp(
      static_cast<int>(value),
      1,
      static_cast<int>(ADC_MAX - 1)));
}

uint16_t normalizeAdc(uint32_t rawValue, bool invert) {
  uint16_t adcValue = clampAdc(rawValue);
  if (invert) {
    adcValue = ADC_MAX - adcValue;
  }

  return adcValue;
}

uint16_t readAdc(Pin_t pin) {
  adc_gpio_init(pin);
  adc_select_input(pin - ADC_PIN_OFFSET);
  return adc_read();
}

uint16_t getConfiguredNeutral(Pin_t pin, uint32_t storedCenter, bool autoCalibration, bool invert) {
  if (autoCalibration && isValidPin(pin)) {
    return clampNeutral(normalizeAdc(readAdc(pin), invert));
  }

  const uint32_t neutral = (storedCenter == 0) ? ADC_NEUTRAL : storedCenter;
  return clampNeutral(normalizeAdc(neutral, invert));
}

uint16_t getMinTravel(const AnalogJoystickCalibration &calibration) {
  const uint16_t xLow = calibration.neutral[0] - calibration.min[0];
  const uint16_t xHigh = calibration.max[0] - calibration.neutral[0];
  const uint16_t yLow = calibration.neutral[1] - calibration.min[1];
  const uint16_t yHigh = calibration.max[1] - calibration.neutral[1];
  return std::min(std::min(xLow, xHigh), std::min(yLow, yHigh));
}

float getCompensationFactor(uint32_t diagonalCompensation) {
  return std::clamp(static_cast<float>(diagonalCompensation) / 100.0f, 0.0f, 1.0f);
}

void setAxisCalibration(
    uint32_t rawMin,
    uint32_t rawMax,
    uint16_t neutral,
    bool invert,
    uint16_t &min,
    uint16_t &max,
    uint16_t &calibratedNeutral) {
  const uint16_t valueA = normalizeAdc(rawMin, invert);
  const uint16_t valueB = normalizeAdc(rawMax, invert);

  min = std::min(valueA, valueB);
  max = std::max(valueA, valueB);
  calibratedNeutral = clampNeutral(neutral);

  if (min == max) {
    min = 0;
    max = ADC_MAX;
  }

  if (min >= calibratedNeutral) {
    min = static_cast<uint16_t>(calibratedNeutral - 1);
  }

  if (max <= calibratedNeutral) {
    max = static_cast<uint16_t>(calibratedNeutral + 1);
  }
}

AnalogJoystickCalibration buildCalibration(
    uint16_t xNeutral,
    uint16_t yNeutral,
    uint32_t xMin,
    uint32_t xMax,
    uint32_t yMin,
    uint32_t yMax,
    uint32_t deadzone,
    bool xInverted,
    bool yInverted) {
  AnalogJoystickCalibration calibration;

  setAxisCalibration(
      xMin,
      xMax,
      xNeutral,
      xInverted,
      calibration.min[0],
      calibration.max[0],
      calibration.neutral[0]);
  setAxisCalibration(
      yMin,
      yMax,
      yNeutral,
      yInverted,
      calibration.min[1],
      calibration.max[1],
      calibration.neutral[1]);

  const uint16_t minTravel = getMinTravel(calibration);
  const uint16_t maxDeadzone = (minTravel > 1) ? static_cast<uint16_t>(minTravel - 1) : 0;
  calibration.deadzone = std::min(clampAdc(deadzone), maxDeadzone);

  return calibration;
}

} // namespace

bool AnalogInput::available() {
  return Storage::getInstance().getAddonOptions().analogOptions.enabled;
}

void AnalogInput::setup() {
  const AnalogOptions &analogOptions = Storage::getInstance().getAddonOptions().analogOptions;

  adc_init();

  configureStick(
      0,
      analogOptions.analogAdc1PinX,
      analogOptions.analogAdc1PinY,
      analogOptions.analogAdc1Invert,
      analogOptions.analogAdc1Mode,
      analogOptions.analog_smoothing,
      analogOptions.smoothing_factor,
      analogOptions.auto_calibrate,
      analogOptions.forced_circularity,
      analogOptions.joystick_min_x,
      analogOptions.joystick_max_x,
      analogOptions.joystick_min_y,
      analogOptions.joystick_max_y,
      analogOptions.joystick_center_x,
      analogOptions.joystick_center_y,
      analogOptions.joystick_deadzone,
      analogOptions.diagonal_compensation);

  configureStick(
      1,
      analogOptions.analogAdc2PinX,
      analogOptions.analogAdc2PinY,
      analogOptions.analogAdc2Invert,
      analogOptions.analogAdc2Mode,
      analogOptions.analog_smoothing2,
      analogOptions.smoothing_factor2,
      analogOptions.auto_calibrate2,
      analogOptions.forced_circularity2,
      analogOptions.joystick_min_x2,
      analogOptions.joystick_max_x2,
      analogOptions.joystick_min_y2,
      analogOptions.joystick_max_y2,
      analogOptions.joystick_center_x2,
      analogOptions.joystick_center_y2,
      analogOptions.joystick_deadzone2,
      analogOptions.diagonal_compensation2);
}

void AnalogInput::configureStick(
    int stick_num,
    Pin_t x_pin,
    Pin_t y_pin,
    InvertMode analog_invert,
    DpadMode analog_dpad,
    bool ema_option,
    float ema_smoothing,
    bool auto_calibration,
    bool forced_circularity,
    uint32_t joystick_min_x,
    uint32_t joystick_max_x,
    uint32_t joystick_min_y,
    uint32_t joystick_max_y,
    uint32_t joystick_center_x,
    uint32_t joystick_center_y,
    uint32_t joystick_deadzone,
    uint32_t diagonal_compensation) {
  adc_instance &adc_pair = adc_pairs[stick_num];

  adc_pair.x_pin = x_pin;
  adc_pair.y_pin = y_pin;
  adc_pair.x_center = ADC_NEUTRAL;
  adc_pair.y_center = ADC_NEUTRAL;
  adc_pair.analog_invert = analog_invert;
  adc_pair.analog_dpad = analog_dpad;
  adc_pair.x_ema = ADC_NEUTRAL;
  adc_pair.y_ema = ADC_NEUTRAL;
  adc_pair.ema_option = ema_option;
  adc_pair.ema_smoothing = std::clamp(ema_smoothing / 1000.0f, 0.0f, 1.0f);
  adc_pair.diagonal_compensation_strength = diagonal_compensation;
  adc_pair.compensation_factor = getCompensationFactor(adc_pair.diagonal_compensation_strength);
  adc_pair.auto_calibration = auto_calibration;
  adc_pair.forced_circularity = forced_circularity;
  adc_pair.joystick_min_x = joystick_min_x;
  adc_pair.joystick_max_x = joystick_max_x;
  adc_pair.joystick_min_y = joystick_min_y;
  adc_pair.joystick_max_y = joystick_max_y;
  adc_pair.joystick_center_x = joystick_center_x;
  adc_pair.joystick_center_y = joystick_center_y;
  adc_pair.joystick_deadzone = joystick_deadzone;
  adc_pair.configured = false;
  adc_pair.input = nullptr;
  adc_pair.diagonal_compensation.reset();
  adc_pair.joystick.reset();

  if (!isValidPin(adc_pair.x_pin) || !isValidPin(adc_pair.y_pin)) {
    return;
  }

  const bool xInverted = invertX(adc_pair.analog_invert);
  const bool yInverted = invertY(adc_pair.analog_invert);

  adc_pair.x_center = getConfiguredNeutral(
      adc_pair.x_pin,
      adc_pair.joystick_center_x,
      adc_pair.auto_calibration,
      xInverted);
  adc_pair.y_center = getConfiguredNeutral(
      adc_pair.y_pin,
      adc_pair.joystick_center_y,
      adc_pair.auto_calibration,
      yInverted);

  adc_pair.calibration = buildCalibration(
      adc_pair.x_center,
      adc_pair.y_center,
      adc_pair.joystick_min_x,
      adc_pair.joystick_max_x,
      adc_pair.joystick_min_y,
      adc_pair.joystick_max_y,
      adc_pair.joystick_deadzone,
      xInverted,
      yInverted);

  adc_pair.joystick = std::make_unique<AnalogJoystick>(
      adc_pair.x_pin,
      xInverted,
      adc_pair.y_pin,
      yInverted,
      adc_pair.calibration);

  if (adc_pair.forced_circularity) {
    adc_pair.diagonal_compensation = std::make_unique<DiagonalCompensation>(
        adc_pair.joystick.get(),
        adc_pair.compensation_factor);
    adc_pair.input = adc_pair.diagonal_compensation.get();
  } else {
    adc_pair.input = adc_pair.joystick.get();
  }

  adc_pair.configured = true;
}

void AnalogInput::process() {
  Gamepad *gamepad = Storage::getInstance().GetGamepad();

  uint32_t joystickMid = GAMEPAD_JOYSTICK_MID;
  uint32_t joystickMax = GAMEPAD_JOYSTICK_MAX;
  if (DriverManager::getInstance().getDriver() != nullptr) {
    joystickMid = DriverManager::getInstance().getDriver()->GetJoystickMidValue();
    joystickMax = joystickMid * 2; // 0x8000 mid must be 0x10000 max, but we reduce by 1 if we're maxed out
  }

  for (int i = 0; i < ADC_COUNT; i++) {
    adc_instance &adc_pair = adc_pairs[i];
    if (!adc_pair.configured || adc_pair.input == nullptr) {
      continue;
    }

    std::array<uint16_t, 2> state = adc_pair.input->getState();
    uint16_t xValue = state[0];
    uint16_t yValue = state[1];

    const uint16_t clampedX = scaleAnalogValue(xValue, joystickMid, joystickMax);
    const uint16_t clampedY = scaleAnalogValue(yValue, joystickMid, joystickMax);

    if (adc_pair.analog_dpad == DpadMode::DPAD_MODE_LEFT_ANALOG) {
      gamepad->state.lx = clampedX;
      gamepad->state.ly = clampedY;
    } else if (adc_pair.analog_dpad == DpadMode::DPAD_MODE_RIGHT_ANALOG) {
      gamepad->state.rx = clampedX;
      gamepad->state.ry = clampedY;
    }
  }
}

uint16_t AnalogInput::scaleAnalogValue(uint16_t value, uint32_t joystickMid, uint32_t joystickMax) {
  uint32_t scaledValue = joystickMid;

  if (value < ADC_NEUTRAL) {
    scaledValue = (static_cast<uint32_t>(value) * joystickMid) / ADC_NEUTRAL;
  } else if (value > ADC_NEUTRAL) {
    const uint32_t highRange = (joystickMax > joystickMid) ? (joystickMax - joystickMid) : 0;
    scaledValue = joystickMid +
                  (((static_cast<uint32_t>(value) - ADC_NEUTRAL) * highRange) /
                   (ADC_MAX - ADC_NEUTRAL));
  }

  return static_cast<uint16_t>(std::min<uint32_t>(scaledValue, 0xFFFF));
}
