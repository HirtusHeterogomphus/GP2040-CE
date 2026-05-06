#ifndef _GYRO_ADDON_H_
#define _GYRO_ADDON_H_

#include <stdint.h>
#include <string>

#include "gpaddon.h"
#include "peripheral_i2c.h"
#include "pico_lsm6ds3/LSM6DS3.h"

#define GyroName "Gyro"

#ifndef GYRO_ENABLED
#define GYRO_ENABLED 0
#endif

#ifndef GYRO_ADDRESS
#define GYRO_ADDRESS 0
#endif

// BoardConfig axis mapping defaults: 1=X, 2=Y, 3=Z. Negative values invert the source axis.
#ifndef GYRO_ACCEL_AXIS_X
#define GYRO_ACCEL_AXIS_X 1
#endif

#ifndef GYRO_ACCEL_AXIS_Y
#define GYRO_ACCEL_AXIS_Y 2
#endif

#ifndef GYRO_ACCEL_AXIS_Z
#define GYRO_ACCEL_AXIS_Z 3
#endif

#ifndef GYRO_GYRO_AXIS_X
#define GYRO_GYRO_AXIS_X 1
#endif

#ifndef GYRO_GYRO_AXIS_Y
#define GYRO_GYRO_AXIS_Y 2
#endif

#ifndef GYRO_GYRO_AXIS_Z
#define GYRO_GYRO_AXIS_Z 3
#endif

class GyroAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess() {}
    virtual void process();
    virtual void postprocess(bool sent) {}
    virtual void reinit() {}
    virtual std::string name() { return GyroName; }
private:
    static constexpr uint32_t POLL_INTERVAL_MS = 10;
    static constexpr uint8_t ADDRESS_AUTO = 0x00;
    static constexpr uint8_t ADDRESS_PRIMARY = 0x6A;
    static constexpr uint8_t ADDRESS_SECONDARY = 0x6B;

    pico_lsm6ds3::LSM6DS3 *sensor = nullptr;
    PeripheralI2C *i2c = nullptr;
    uint32_t nextTimer = 0;
    bool started = false;

    bool tryStartSensor(PeripheralI2C *candidateI2C, uint8_t address);
    uint16_t scaleAcceleration(float value);
    uint16_t scaleGyroscope(float value);
    uint16_t scaleSigned(float value, float range);
    uint16_t mapAxisValue(uint16_t x, uint16_t y, uint16_t z, int32_t axis, int32_t fallbackAxis);
};

#endif
