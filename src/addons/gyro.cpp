#include "addons/gyro.h"

#include <algorithm>
#include <vector>

#include "config.pb.h"
#include "drivermanager.h"
#include "helper.h"
#include "peripheralmanager.h"
#include "storagemanager.h"

bool GyroAddon::available() {
    const GyroOptions& options = Storage::getInstance().getAddonOptions().gyroOptions;
    if (!options.enabled) {
        return false;
    }

    std::vector<uint8_t> addresses;
    if (options.address == ADDRESS_AUTO) {
        addresses = {ADDRESS_PRIMARY, ADDRESS_SECONDARY};
    } else if (options.address == ADDRESS_PRIMARY || options.address == ADDRESS_SECONDARY) {
        addresses = {static_cast<uint8_t>(options.address)};
    } else {
        return false;
    }

    for (uint8_t block = 0; block < NUM_I2CS; block++) {
        if (!PeripheralManager::getInstance().isI2CEnabled(block)) {
            continue;
        }

        PeripheralI2C *candidateI2C = PeripheralManager::getInstance().getI2C(block);
        for (uint8_t address : addresses) {
            if (tryStartSensor(candidateI2C, address)) {
                return true;
            }
        }
    }

    return false;
}

bool GyroAddon::tryStartSensor(PeripheralI2C *candidateI2C, uint8_t address) {
    if (candidateI2C == nullptr || candidateI2C->getSDA() < 0 || candidateI2C->getSCL() < 0) {
        return false;
    }

    uint8_t whoAmI = 0;
    if (!candidateI2C->readRegister(address, 0x0F, &whoAmI, sizeof(whoAmI)) ||
        (whoAmI != 0x6C && whoAmI != 0x69)) {
        return false;
    }

    pico_lsm6ds3::LSM6DS3 *candidateSensor = new pico_lsm6ds3::LSM6DS3(candidateI2C->getController(), address);
    if (!candidateSensor->begin(static_cast<uint>(candidateI2C->getSDA()), static_cast<uint>(candidateI2C->getSCL()), candidateI2C->getSpeed())) {
        delete candidateSensor;
        return false;
    }

    i2c = candidateI2C;
    sensor = candidateSensor;
    started = true;
    return true;
}

void GyroAddon::setup() {
    nextTimer = getMillis();
}

void GyroAddon::process() {
    if (DriverManager::getInstance().getInputMode() != INPUT_MODE_SWITCH_PRO) {
        return;
    }

    Gamepad *gamepad = Storage::getInstance().GetGamepad();
    gamepad->auxState.sensors.accelerometer.enabled = started;
    gamepad->auxState.sensors.gyroscope.enabled = started;

    if (!started || nextTimer >= getMillis()) {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    const GyroOptions& options = Storage::getInstance().getAddonOptions().gyroOptions;

    bool accelerationRead = sensor->readAcceleration(x, y, z);
    if (accelerationRead) {
        gamepad->auxState.sensors.accelerometer.x = mapAxisValue(x, y, z, options.accelAxisX, GYRO_ACCEL_AXIS_X);
        gamepad->auxState.sensors.accelerometer.y = mapAxisValue(x, y, z, options.accelAxisY, GYRO_ACCEL_AXIS_Y);
        gamepad->auxState.sensors.accelerometer.z = mapAxisValue(x, y, z, options.accelAxisZ, GYRO_ACCEL_AXIS_Z);
    }
    gamepad->auxState.sensors.accelerometer.active = accelerationRead;

    bool gyroRead = sensor->readGyroscope(x, y, z);
    if (gyroRead) {
        gamepad->auxState.sensors.gyroscope.x = mapAxisValue(x, y, z, options.gyroAxisX, GYRO_GYRO_AXIS_X);
        gamepad->auxState.sensors.gyroscope.y = mapAxisValue(x, y, z, options.gyroAxisY, GYRO_GYRO_AXIS_Y);
        gamepad->auxState.sensors.gyroscope.z = mapAxisValue(x, y, z, options.gyroAxisZ, GYRO_GYRO_AXIS_Z);
    }
    gamepad->auxState.sensors.gyroscope.active = gyroRead;

    nextTimer = getMillis() + POLL_INTERVAL_MS;
}

uint16_t GyroAddon::mapAxisValue(uint16_t x, uint16_t y, uint16_t z, int32_t axis, int32_t fallbackAxis) {
    if (axis < -3 || axis > 3 || axis == 0) {
        axis = fallbackAxis;
    }

    uint16_t value = x;
    int32_t sourceAxis = axis < 0 ? -axis : axis;
    switch (sourceAxis) {
        case 2:
            value = y;
            break;
        case 3:
            value = z;
            break;
        case 1:
        default:
            value = x;
            break;
    }

    if (axis < 0) {
        int32_t inverted = -static_cast<int16_t>(value);
        inverted = std::min(std::max(inverted, (int32_t)-32768), (int32_t)32767);
        return static_cast<uint16_t>(static_cast<int16_t>(inverted));
    }

    return value;
}
