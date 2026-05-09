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

    const GyroOptions& options = Storage::getInstance().getAddonOptions().gyroOptions;
    candidateSensor->setOffsets(
        options.accelOffsetX,
        options.accelOffsetY,
        options.accelOffsetZ,
        options.gyroOffsetX,
        options.gyroOffsetY,
        options.gyroOffsetZ
    );

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

    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;
    GamepadAux3DSensor accelerometerSample;
    GamepadAux3DSensor gyroscopeSample;

    bool accelerationRead = sensor->readAcceleration(x, y, z);
    if (accelerationRead) {
        accelerometerSample.enabled = true;
        accelerometerSample.active = true;
        accelerometerSample.x = mapAxisValue(x, y, z, GYRO_ACCEL_AXIS_X);
        accelerometerSample.y = mapAxisValue(x, y, z, GYRO_ACCEL_AXIS_Y);
        accelerometerSample.z = mapAxisValue(x, y, z, GYRO_ACCEL_AXIS_Z);
        gamepad->auxState.sensors.accelerometer = accelerometerSample;
    }
    gamepad->auxState.sensors.accelerometer.active = accelerationRead;

    bool gyroRead = sensor->readGyroscope(x, y, z);
    if (gyroRead) {
        gyroscopeSample.enabled = true;
        gyroscopeSample.active = true;
        gyroscopeSample.x = mapAxisValue(x, y, z, GYRO_GYRO_AXIS_X);
        gyroscopeSample.y = mapAxisValue(x, y, z, GYRO_GYRO_AXIS_Y);
        gyroscopeSample.z = mapAxisValue(x, y, z, GYRO_GYRO_AXIS_Z);
        gamepad->auxState.sensors.gyroscope = gyroscopeSample;
    }
    gamepad->auxState.sensors.gyroscope.active = gyroRead;

    if (accelerationRead && gyroRead) {
        GamepadAuxSensors& sensors = gamepad->auxState.sensors;
        for (uint8_t i = 1; i < GAMEPAD_AUX_MAX_IMU_SAMPLES; i++) {
            sensors.imuSamples[i - 1] = sensors.imuSamples[i];
        }
        sensors.imuSamples[GAMEPAD_AUX_MAX_IMU_SAMPLES - 1].accelerometer = accelerometerSample;
        sensors.imuSamples[GAMEPAD_AUX_MAX_IMU_SAMPLES - 1].gyroscope = gyroscopeSample;
        if (sensors.imuSampleCount < GAMEPAD_AUX_MAX_IMU_SAMPLES) {
            sensors.imuSampleCount++;
        }
    }

    nextTimer = getMillis() + POLL_INTERVAL_MS;
}

uint16_t GyroAddon::mapAxisValue(uint16_t x, uint16_t y, uint16_t z, int32_t axis) {

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
