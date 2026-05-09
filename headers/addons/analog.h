#ifndef _Analog_H
#define _Analog_H

#include "gpaddon.h"
#include "GamepadEnums.h"
#include "BoardConfig.h"
#include "enums.pb.h"
#include "types.h"
#include "AnalogJoystick.h"
#include "DiagonalCompensation.h"

#include <memory>

#ifndef ANALOG_INPUT_ENABLED
#define ANALOG_INPUT_ENABLED 0
#endif

#ifndef ANALOG_ADC_1_VRX
#define ANALOG_ADC_1_VRX    -1
#endif

#ifndef ANALOG_ADC_1_VRY
#define ANALOG_ADC_1_VRY    -1
#endif

#ifndef ANALOG_ADC_1_MODE
#define ANALOG_ADC_1_MODE DPAD_MODE_LEFT_ANALOG
#endif

#ifndef ANALOG_ADC_1_INVERT
#define ANALOG_ADC_1_INVERT INVERT_NONE
#endif

#ifndef ANALOG_ADC_2_VRX
#define ANALOG_ADC_2_VRX    -1
#endif

#ifndef ANALOG_ADC_2_VRY
#define ANALOG_ADC_2_VRY    -1
#endif

#ifndef ANALOG_ADC_2_MODE
#define ANALOG_ADC_2_MODE DPAD_MODE_RIGHT_ANALOG
#endif

#ifndef ANALOG_ADC_2_INVERT
#define ANALOG_ADC_2_INVERT INVERT_NONE
#endif

#ifndef FORCED_CIRCULARITY_ENABLED
#define FORCED_CIRCULARITY_ENABLED 0
#endif

#ifndef FORCED_CIRCULARITY2_ENABLED
#define FORCED_CIRCULARITY2_ENABLED 0
#endif

#ifndef DEFAULT_INNER_DEADZONE
#define DEFAULT_INNER_DEADZONE 5
#endif

#ifndef DEFAULT_INNER_DEADZONE2
#define DEFAULT_INNER_DEADZONE2 5
#endif

#ifndef DEFAULT_OUTER_DEADZONE
#define DEFAULT_OUTER_DEADZONE 95
#endif

#ifndef DEFAULT_OUTER_DEADZONE2
#define DEFAULT_OUTER_DEADZONE2 95
#endif

#ifndef ANALOG_ADC_1_NEUTRAL_X
#define ANALOG_ADC_1_NEUTRAL_X 2048
#endif

#ifndef ANALOG_ADC_1_NEUTRAL_Y
#define ANALOG_ADC_1_NEUTRAL_Y 2048
#endif

#ifndef ANALOG_ADC_1_MIN_X
#define ANALOG_ADC_1_MIN_X (ANALOG_ADC_1_NEUTRAL_X - ((ANALOG_ADC_1_NEUTRAL_X * DEFAULT_OUTER_DEADZONE) / 100))
#endif

#ifndef ANALOG_ADC_1_MAX_X
#define ANALOG_ADC_1_MAX_X (ANALOG_ADC_1_NEUTRAL_X + (((4095 - ANALOG_ADC_1_NEUTRAL_X) * DEFAULT_OUTER_DEADZONE) / 100))
#endif

#ifndef ANALOG_ADC_1_MIN_Y
#define ANALOG_ADC_1_MIN_Y (ANALOG_ADC_1_NEUTRAL_Y - ((ANALOG_ADC_1_NEUTRAL_Y * DEFAULT_OUTER_DEADZONE) / 100))
#endif

#ifndef ANALOG_ADC_1_MAX_Y
#define ANALOG_ADC_1_MAX_Y (ANALOG_ADC_1_NEUTRAL_Y + (((4095 - ANALOG_ADC_1_NEUTRAL_Y) * DEFAULT_OUTER_DEADZONE) / 100))
#endif

#ifndef ANALOG_ADC_1_DEADZONE
#define ANALOG_ADC_1_DEADZONE ((4095 * DEFAULT_INNER_DEADZONE) / 100)
#endif

#ifndef ANALOG_ADC_2_NEUTRAL_X
#define ANALOG_ADC_2_NEUTRAL_X 2048
#endif

#ifndef ANALOG_ADC_2_NEUTRAL_Y
#define ANALOG_ADC_2_NEUTRAL_Y 2048
#endif

#ifndef ANALOG_ADC_2_MIN_X
#define ANALOG_ADC_2_MIN_X (ANALOG_ADC_2_NEUTRAL_X - ((ANALOG_ADC_2_NEUTRAL_X * DEFAULT_OUTER_DEADZONE2) / 100))
#endif

#ifndef ANALOG_ADC_2_MAX_X
#define ANALOG_ADC_2_MAX_X (ANALOG_ADC_2_NEUTRAL_X + (((4095 - ANALOG_ADC_2_NEUTRAL_X) * DEFAULT_OUTER_DEADZONE2) / 100))
#endif

#ifndef ANALOG_ADC_2_MIN_Y
#define ANALOG_ADC_2_MIN_Y (ANALOG_ADC_2_NEUTRAL_Y - ((ANALOG_ADC_2_NEUTRAL_Y * DEFAULT_OUTER_DEADZONE2) / 100))
#endif

#ifndef ANALOG_ADC_2_MAX_Y
#define ANALOG_ADC_2_MAX_Y (ANALOG_ADC_2_NEUTRAL_Y + (((4095 - ANALOG_ADC_2_NEUTRAL_Y) * DEFAULT_OUTER_DEADZONE2) / 100))
#endif

#ifndef ANALOG_ADC_2_DEADZONE
#define ANALOG_ADC_2_DEADZONE ((4095 * DEFAULT_INNER_DEADZONE2) / 100)
#endif

#ifndef AUTO_CALIBRATE_ENABLED
#define AUTO_CALIBRATE_ENABLED 0
#endif

#ifndef AUTO_CALIBRATE2_ENABLED
#define AUTO_CALIBRATE2_ENABLED 0
#endif

#ifndef ANALOG_SMOOTHING_ENABLED
#define ANALOG_SMOOTHING_ENABLED 0
#endif

#ifndef ANALOG_SMOOTHING2_ENABLED
#define ANALOG_SMOOTHING2_ENABLED 0
#endif

#ifndef SMOOTHING_FACTOR
#define SMOOTHING_FACTOR 5
#endif

#ifndef SMOOTHING_FACTOR2
#define SMOOTHING_FACTOR2 5
#endif

#ifndef ANALOG_ERROR
#define ANALOG_ERROR 1000
#endif

#ifndef ANALOG_ERROR2
#define ANALOG_ERROR2 1000
#endif

#define LEGACY_ANALOG_ERROR_TO_DIAGONAL_COMPENSATION(value) (((value) >= 1000) ? 0 : ((1000 - (value) + 5) / 10))

#ifndef DIAGONAL_COMPENSATION_STRENGTH
#define DIAGONAL_COMPENSATION_STRENGTH LEGACY_ANALOG_ERROR_TO_DIAGONAL_COMPENSATION(ANALOG_ERROR)
#endif

#ifndef DIAGONAL_COMPENSATION2_STRENGTH
#define DIAGONAL_COMPENSATION2_STRENGTH LEGACY_ANALOG_ERROR_TO_DIAGONAL_COMPENSATION(ANALOG_ERROR2)
#endif

// Analog Module Name
#define AnalogName "Analog"

#define ADC_COUNT 2

typedef struct
{
    Pin_t x_pin;
    Pin_t y_pin;
    uint16_t x_center;
    uint16_t y_center;
    InvertMode analog_invert;
    DpadMode analog_dpad;
    uint16_t x_ema;
    uint16_t y_ema;
    bool ema_option;
    float ema_smoothing;
    float compensation_factor;
    uint32_t diagonal_compensation_strength;
    bool auto_calibration;
    bool forced_circularity;
    uint32_t joystick_min_x;
    uint32_t joystick_max_x;
    uint32_t joystick_min_y;
    uint32_t joystick_max_y;
    uint32_t joystick_center_x;
    uint32_t joystick_center_y;
    uint32_t joystick_deadzone;
    bool configured;
    AnalogJoystickCalibration calibration;
    std::unique_ptr<AnalogJoystick> joystick;
    std::unique_ptr<DiagonalCompensation> diagonal_compensation;
    AnalogBase * input;
} adc_instance;

class AnalogInput : public GPAddon {
public:
    virtual bool available();
    virtual void setup();       // Analog Setup
    virtual void process();     // Analog Process
    virtual void preprocess() {}
    virtual void postprocess(bool sent) {}
    virtual void reinit() {}
    virtual std::string name() { return AnalogName; }
private:
    void configureStick(
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
        uint32_t diagonal_compensation);
    uint16_t scaleAnalogValue(uint16_t value, uint32_t joystickMid, uint32_t joystickMax);
    adc_instance adc_pairs[ADC_COUNT];
};

#endif  // _Analog_H_
