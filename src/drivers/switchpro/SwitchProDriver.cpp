#include "drivers/switchpro/SwitchProDriver.h"
#include "drivers/shared/driverhelper.h"
#include "storagemanager.h"
#include "pico/rand.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// force a report to be sent every X ms
#define SWITCH_PRO_KEEPALIVE_TIMER 5

namespace {

// Switch2 が TOGGLE_IMU=0x02 で要求する quaternion mode 用の換算定数。
// LSM6DS3 は 2000 dps full scale で初期化しているため、int16 raw 値を rad/s に変換する。
constexpr float SWITCH_PRO_IMU_PI = 3.14159265358979323846f;
constexpr float SWITCH_PRO_IMU_GYRO_RAD_PER_SEC = 2000.0f / INT16_MAX * SWITCH_PRO_IMU_PI / 180.0f;
// USB report 停止や処理遅延で dt が大きく跳ねた時に、1回の積分で姿勢が飛ぶのを防ぐ。
constexpr float SWITCH_PRO_IMU_MAX_DELTA_SECONDS = 0.05f;
// Mahony の比例ゲイン。0.0f の場合は加速度による roll/pitch 補正を無効化し、gyro 積分のみになる。
constexpr float SWITCH_PRO_IMU_MAHONY_KP = 0.0f;
// LSM6DS3 の +-8g 設定では 1g が約 4096 raw。加速度補正は重力に近い大きさの時だけ使う。
constexpr float SWITCH_PRO_IMU_ACCEL_ONE_G = 4096.0f;
constexpr float SWITCH_PRO_IMU_ACCEL_MIN = SWITCH_PRO_IMU_ACCEL_ONE_G * 0.55f;
constexpr float SWITCH_PRO_IMU_ACCEL_MAX = SWITCH_PRO_IMU_ACCEL_ONE_G * 1.45f;

// mode2 packet 内の accel ベクトルは wire layout が y,x,z 順。
// C++ 側のフィールド名もこの順にしておき、memcpy 時のバイト配置を明示する。
typedef struct __attribute((packed, aligned(1)))
{
    int16_t y;
    int16_t x;
    int16_t z;
} SwitchIMUMode2Vector;

// Switch2 mode2 の 36 byte IMU payload。
// quaternion は最大成分を省いた 3 成分として bitfield に格納し、accel は先頭の accel0 を使う。
typedef struct __attribute((packed, aligned(1)))
{
    SwitchIMUMode2Vector accel0;
    uint32_t mode : 2;
    uint32_t maxIndex : 2;
    uint32_t lastSample0 : 21;
    uint32_t lastSample1Low : 7;
    uint16_t lastSample1High : 14;
    uint16_t lastSample2Low : 2;
    SwitchIMUMode2Vector accel1;
    uint32_t lastSample2High : 19;
    uint32_t deltaLastFirst0 : 13;
    uint16_t deltaLastFirst1 : 13;
    uint16_t deltaLastFirst2Low : 3;
    SwitchIMUMode2Vector accel2;
    uint32_t deltaLastFirst2High : 10;
    uint32_t deltaMidAvg0 : 7;
    uint32_t deltaMidAvg1 : 7;
    uint32_t deltaMidAvg2 : 7;
    uint32_t timestampStartLow : 1;
    uint16_t timestampStartHigh : 10;
    uint16_t timestampCount : 6;
} SwitchIMUMode2Data;

static_assert(sizeof(SwitchIMUMode2Data) == 36, "Switch IMU mode 2 payload must be 36 bytes");

// auxState 上は uint16_t で保持されている IMU raw 値を、センサー本来の signed int16 に戻す。
int16_t signedIMUValue(uint16_t value) {
    return static_cast<int16_t>(value);
}

// int16 の符号反転 helper。INT16_MIN は正方向に表現できないため、飽和させる。
int16_t invertIMUValue(int16_t value) {
    return value == INT16_MIN ? INT16_MAX : static_cast<int16_t>(-value);
}

// 正規化済み quaternion を Switch mode2 の "smallest three" 形式に圧縮する。
// 4成分のうち絶対値が最大の成分を maxIndex として省略し、残り3成分だけを packet に詰める。
void packMode2Quaternion(SwitchIMUMode2Data& mode2Data, const float quat[4]) {
    // 符号付き値そのものではなく絶対値で最大成分を選ぶ。
    // ここを誤ると、負方向に大きく回した時だけ maxIndex が切り替わらず姿勢がジャンプする。
    uint8_t maxIndex = 0;
    for (uint8_t i = 1; i < 4; i++) {
        if (std::fabs(quat[i]) > std::fabs(quat[maxIndex])) {
            maxIndex = i;
        }
    }

    // 省略した最大成分が正になるように、残りの成分全体へ同じ符号を掛ける。
    // quaternion は q と -q が同じ姿勢を表すため、この正規化で送信表現を一意に近づける。
    int32_t quaternionComponents[3] = {};
    const float sign = quat[maxIndex] < 0.0f ? -1.0f : 1.0f;
    for (uint8_t i = 0; i < 3; i++) {
        quaternionComponents[i] = static_cast<int32_t>(quat[(maxIndex + i + 1) & 3] * 0x40000000 * sign);
    }

    // Pro Controller 互換の bitfield 配置に分割して保存する。
    // 各成分は上位側 21 bit 相当を使うため、10 bit 右シフトしてから field に分ける。
    mode2Data.maxIndex = maxIndex;
    mode2Data.lastSample0 = static_cast<uint32_t>(quaternionComponents[0] >> 10);
    mode2Data.lastSample1Low = static_cast<uint32_t>((quaternionComponents[1] >> 10) & 0x7F);
    mode2Data.lastSample1High = static_cast<uint16_t>(((quaternionComponents[1] >> 10) & 0x1FFF80) >> 7);
    mode2Data.lastSample2Low = static_cast<uint16_t>((quaternionComponents[2] >> 10) & 0x03);
    mode2Data.lastSample2High = static_cast<uint32_t>(((quaternionComponents[2] >> 10) & 0x1FFFFC) >> 2);
}

}

void SwitchProDriver::initialize() {
    //stdio_init_all();

    playerID = 0;
    last_report_counter = 0;
    handshakeCounter = 0;
    isReady = false;
    imuMode = 0;
    resetIMUState();

    deviceInfo = {
        .majorVersion = 0x04,
        .minorVersion = 0x91,
        .controllerType = SwitchControllerType::SWITCH_TYPE_PRO_CONTROLLER,
        .unknown00 = 0x02,
        // MAC address in reverse
        .macAddress = {0x7c, 0xbb, 0x8a, (uint8_t)(get_rand_32() % 0xff), (uint8_t)(get_rand_32() % 0xff), (uint8_t)(get_rand_32() % 0xff)},
        .unknown01 = 0x01,
        .storedColors = 0x02,
    };

	switchReport = {
        .reportID = 0x30,
        .timestamp = 0,

        .inputs {
            .connectionInfo = 0,
            .batteryLevel = 0x08,

            // byte 00
            .buttonY = 0,
            .buttonX = 0,
            .buttonB = 0,
            .buttonA = 0,
            .buttonRightSR = 0,
            .buttonRightSL = 0,
            .buttonR = 0,
            .buttonZR = 0,

            // byte 01
            .buttonMinus = 0,
            .buttonPlus = 0,
            .buttonThumbR = 0,
            .buttonThumbL = 0,
            .buttonHome = 0,
            .buttonCapture = 0,
            .dummy = 0,
            .chargingGrip = 0,

            // byte 02
            .dpadDown = 0,
            .dpadUp = 0,
            .dpadRight = 0,
            .dpadLeft = 0,
            .buttonLeftSL = 0,
            .buttonLeftSR = 0,
            .buttonL = 0,
            .buttonZL = 0,
            .leftStick = {0xFF, 0xF7, 0x7F},
            .rightStick = {0xFF, 0xF7, 0x7F},
        },
        .rumbleReport = 0,
        .imuData = {0x00},
        .padding = {0x00}
    };

    last_report_timer = to_ms_since_boot(get_absolute_time());

    factoryConfig->leftStickCalibration.getRealMin(leftMinX, leftMinY);
    factoryConfig->leftStickCalibration.getCenter(leftCenX, leftCenY);
    factoryConfig->leftStickCalibration.getRealMax(leftMaxX, leftMaxY);
    factoryConfig->rightStickCalibration.getRealMin(rightMinX, rightMinY);
    factoryConfig->rightStickCalibration.getCenter(rightCenX, rightCenY);
    factoryConfig->rightStickCalibration.getRealMax(rightMaxX, rightMaxY);

	class_driver = {
	#if CFG_TUSB_DEBUG >= 2
		.name = "SWITCHPRO",
	#endif
		.init = hidd_init,
		.reset = hidd_reset,
		.open = hidd_open,
		.control_xfer_cb = hidd_control_xfer_cb,
		.xfer_cb = hidd_xfer_cb,
		.sof = NULL
	};
}

bool SwitchProDriver::process(Gamepad * gamepad) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    reportSent = false;

    switchReport.inputs.dpadUp =    ((gamepad->state.dpad & GAMEPAD_MASK_UP) == GAMEPAD_MASK_UP);
    switchReport.inputs.dpadDown =  ((gamepad->state.dpad & GAMEPAD_MASK_DOWN) == GAMEPAD_MASK_DOWN);
    switchReport.inputs.dpadLeft =  ((gamepad->state.dpad & GAMEPAD_MASK_LEFT) == GAMEPAD_MASK_LEFT);
    switchReport.inputs.dpadRight = ((gamepad->state.dpad & GAMEPAD_MASK_RIGHT) == GAMEPAD_MASK_RIGHT);

    switchReport.inputs.chargingGrip = 1;

    switchReport.inputs.buttonY = gamepad->pressedB3();
    switchReport.inputs.buttonX = gamepad->pressedB4();
    switchReport.inputs.buttonB = gamepad->pressedB1();
    switchReport.inputs.buttonA = gamepad->pressedB2();
    switchReport.inputs.buttonRightSR = 0;
    switchReport.inputs.buttonRightSL = 0;
    switchReport.inputs.buttonR = gamepad->pressedR1();
    switchReport.inputs.buttonZR = gamepad->pressedR2();
    if (gamepad->hasAnalogTriggers || gamepad->hasRightAnalogStick)
        switchReport.inputs.buttonZR |= gamepad->state.rt > 0;
    switchReport.inputs.buttonMinus = gamepad->pressedS1();
    switchReport.inputs.buttonPlus = gamepad->pressedS2();
    switchReport.inputs.buttonThumbR = gamepad->pressedR3();
    switchReport.inputs.buttonThumbL = gamepad->pressedL3();
    switchReport.inputs.buttonHome = gamepad->pressedA1();
    switchReport.inputs.buttonCapture = gamepad->pressedA2();
    switchReport.inputs.buttonLeftSR = 0;
    switchReport.inputs.buttonLeftSL = 0;
    switchReport.inputs.buttonL = gamepad->pressedL1();
    switchReport.inputs.buttonZL = gamepad->pressedL2();
    if (gamepad->hasAnalogTriggers || gamepad->hasLeftAnalogStick)
        switchReport.inputs.buttonZL |= gamepad->state.lt > 0;

    // analog
    uint16_t scaleLeftStickX = scale16To12(gamepad->state.lx);
    uint16_t scaleLeftStickY = scale16To12(gamepad->state.ly);
    uint16_t scaleRightStickX = scale16To12(gamepad->state.rx);
    uint16_t scaleRightStickY = scale16To12(gamepad->state.ry);
    
    switchReport.inputs.leftStick.setX(std::min(std::max(scaleLeftStickX,leftMinX), leftMaxX));
    switchReport.inputs.leftStick.setY(-std::min(std::max(scaleLeftStickY,leftMinY), leftMaxY));
    switchReport.inputs.rightStick.setX(std::min(std::max(scaleRightStickX,rightMinX), rightMaxX));
    switchReport.inputs.rightStick.setY(-std::min(std::max(scaleRightStickY,rightMinY), rightMaxY));

    switchReport.rumbleReport = 0x09;
    updateIMUData(gamepad);
    //switchReport.reportID = inputMode;

	// Wake up TinyUSB device
	if (tud_suspended())
		tud_remote_wakeup();

    if (isReportQueued) {
        if ((now - last_report_timer) > SWITCH_PRO_KEEPALIVE_TIMER) {
            if (tud_hid_ready() && sendReport(queuedReportID, report, 64) == true ) {
            }
            isReportQueued = false;
            last_report_timer = now;
        }
        reportSent = true;
    }

    Gamepad * processedGamepad = Storage::getInstance().GetProcessedGamepad();
    processedGamepad->auxState.playerID.active = true;
    processedGamepad->auxState.playerID.ledValue = playerID;
    processedGamepad->auxState.playerID.value = playerID;

    if (isReady && !reportSent) {
        if ((now - last_report_timer) > SWITCH_PRO_KEEPALIVE_TIMER) {
            switchReport.timestamp = last_report_counter;
            void * inputReport = &switchReport;
            uint16_t report_size = sizeof(switchReport);
            if (memcmp(last_report, inputReport, report_size) != 0) {
                // HID ready + report sent, copy previous report
                if (tud_hid_ready() && sendReport(0, inputReport, report_size) == true ) {
                    memcpy(last_report, inputReport, report_size);
                    reportSent = true;
                }

                last_report_timer = now;
            }
        }
    } else {
        if (!isInitialized) {
            // send identification
            sendIdentify();
            if (tud_hid_ready() && tud_hid_report(0, report, 64) == true) {
                isInitialized = true;
                reportSent = true;
            }

            last_report_timer = now;
        }
    }

    return reportSent;
}

// tud_hid_get_report_cb
uint16_t SwitchProDriver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    //printf("SwitchProDriver::get_report Rpt: %02x, Type: %d, Len: %d\n", report_id, report_type, reqlen);
//    if (isReady) {
//        memcpy(buffer, &switchReport, sizeof(SwitchProReport));
//        return sizeof(SwitchProReport);
//    }

    return 0;
}

void SwitchProDriver::sendIdentify() {
    memset(report, 0x00, 64);
    report[0] = SwitchReportID::REPORT_USB_INPUT_81;
    report[1] = SwitchOutputSubtypes::IDENTIFY;
    report[2] = 0x00;
    report[3] = deviceInfo.controllerType;
    // MAC address
    for (uint8_t i = 0; i < 6; i++) {
        report[4+i] = deviceInfo.macAddress[5-i];
    }
}

void SwitchProDriver::sendSubCommand(uint8_t subCommand) {

}

bool SwitchProDriver::sendReport(uint8_t reportID, void const* reportData, uint16_t reportLength) {
    bool result = tud_hid_report(reportID, reportData, reportLength);
    if (last_report_counter < 255) {
        last_report_counter++;
    } else {
        last_report_counter = 0;
    }
    return result;
}

void SwitchProDriver::handleConfigReport(uint8_t switchReportID, uint8_t switchReportSubID, const uint8_t *reportData, uint16_t reportLength) {
    bool canSend = false;

    switch (switchReportSubID) {
        case SwitchOutputSubtypes::IDENTIFY:
            //printf("SwitchProDriver::set_report: IDENTIFY\n");
            sendIdentify();
            canSend = true;
            break;
        case SwitchOutputSubtypes::HANDSHAKE:
            //printf("SwitchProDriver::set_report: HANDSHAKE\n");
            report[0] = SwitchReportID::REPORT_USB_INPUT_81;
            report[1] = SwitchOutputSubtypes::HANDSHAKE;
            canSend = true;
            break;
        case SwitchOutputSubtypes::BAUD_RATE:
            //printf("SwitchProDriver::set_report: BAUD_RATE\n");
            report[0] = SwitchReportID::REPORT_USB_INPUT_81;
            report[1] = SwitchOutputSubtypes::BAUD_RATE;
            canSend = true;
            break;
        case SwitchOutputSubtypes::DISABLE_USB_TIMEOUT:
            //printf("SwitchProDriver::set_report: DISABLE_USB_TIMEOUT\n");
            report[0] = SwitchReportID::REPORT_OUTPUT_30;
            report[1] = switchReportSubID;
            //if (handshakeCounter < 4) {
            //    handshakeCounter++;
            //} else {
                isReady = true;
            //}
            canSend = true;
            break;
        case SwitchOutputSubtypes::ENABLE_USB_TIMEOUT:
            //printf("SwitchProDriver::set_report: ENABLE_USB_TIMEOUT\n");
            report[0] = SwitchReportID::REPORT_OUTPUT_30;
            report[1] = switchReportSubID;
            canSend = true;
            break;
        default:
            //printf("SwitchProDriver::set_report: Unknown Sub ID %02x\n", switchReportSubID);
            report[0] = SwitchReportID::REPORT_OUTPUT_30;
            report[1] = switchReportSubID;
            canSend = true;
            break;
    }

    if (canSend) isReportQueued = true;
}

void SwitchProDriver::handleFeatureReport(uint8_t switchReportID, uint8_t switchReportSubID, const uint8_t *reportData, uint16_t reportLength) {
    uint8_t commandID = reportData[10];
    uint32_t spiReadAddress = 0;
    uint8_t spiReadSize = 0;
    bool canSend = false;

    //uint8_t inputReportSize = sizeof(SwitchInputReport);
    //printf("inputReportSize: %d\n", inputReportSize);

    report[0] = SwitchReportID::REPORT_OUTPUT_21;
    report[1] = last_report_counter;
    memcpy(report+2,&switchReport.inputs,sizeof(SwitchInputReport));

    switch (commandID) {
        case SwitchCommands::GET_CONTROLLER_STATE:
            //printf("SwitchProDriver::set_report: Rpt 0x01 GET_CONTROLLER_STATE\n");
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
        case SwitchCommands::BLUETOOTH_PAIR_REQUEST:
            //printf("SwitchProDriver::set_report: Rpt 0x01 BLUETOOTH_PAIR_REQUEST\n");
            report[13] = 0x81;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
        case SwitchCommands::REQUEST_DEVICE_INFO:
            //printf("SwitchProDriver::set_report: Rpt 0x01 REQUEST_DEVICE_INFO\n");
            report[13] = 0x82;
            report[14] = 0x02;
            memcpy(&report[15], &deviceInfo, sizeof(deviceInfo));
            canSend = true;
            break;
        case SwitchCommands::SET_MODE:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SET_MODE\n");
            inputMode = reportData[11];
            report[13] = 0x80;
            report[14] = 0x03;
            report[15] = inputMode;
            canSend = true;
            //printf("Input Mode set to ");
            switch (inputMode) {
                case 0x00:
                    //printf("NFC/IR Polling Data");
                    break;
                case 0x01:
                    //printf("NFC/IR Polling Config");
                    break;
                case 0x02:
                    //printf("NFC/IR Polling Data+Config");
                    break;
                case 0x03:
                    //printf("IR Scan");
                    break;
                case 0x23:
                    //printf("MCU Update");
                    break;
                case 0x30:
                    //printf("Full Input");
                    break;
                case 0x31:
                    //printf("NFC/IR");
                    break;
                case 0x3F:
                    //printf("Simple HID");
                    break;
                case 0x33:
                case 0x35:
                default:
                    //printf("Unknown");
                    break;
            }
            //printf("\n");
            break;
        case SwitchCommands::TRIGGER_BUTTONS:
            //printf("SwitchProDriver::set_report: Rpt 0x01 TRIGGER_BUTTONS\n");
            report[13] = 0x83;
            report[14] = 0x04;
            canSend = true;
            break;
        case SwitchCommands::SET_SHIPMENT:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SET_SHIPMENT\n");
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            //for (uint8_t i = 2; i < bufsize; i++) {
            //    //printf("%02x ", reportData[i]);
            //}
            //printf("\n");
            break;
        case SwitchCommands::SPI_READ:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SPI_READ\n");
            spiReadAddress = (reportData[14] << 24) | (reportData[13] << 16) | (reportData[12] << 8) | (reportData[11]);
            spiReadSize = reportData[15];
            //printf("Read From: 0x%08x Size %d\n", spiReadAddress, spiReadSize);
            report[13] = 0x90;
            report[14] = reportData[10];
            report[15] = reportData[11];
            report[16] = reportData[12];
            report[17] = reportData[13];
            report[18] = reportData[14];
            report[19] = reportData[15];
            readSPIFlash(&report[20], spiReadAddress, spiReadSize);
            canSend = true;
            //printf("----------------------------------------------\n");
            break;
        case SwitchCommands::SET_NFC_IR_CONFIG:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SET_NFC_IR_CONFIG\n");
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::SET_NFC_IR_STATE:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SET_NFC_IR_STATE\n");
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::SET_PLAYER_LIGHTS:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SET_PLAYER_LIGHTS\n");
            playerID = reportData[11];
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            //printf("Player set to %d\n", playerID);
            //printf("----------------------------------------------\n");
            break;
        case SwitchCommands::GET_PLAYER_LIGHTS:
            //printf("SwitchProDriver::set_report: Rpt 0x01 GET_PLAYER_LIGHTS\n");
            playerID = reportData[11];
            report[13] = 0xB0;
            report[14] = commandID;
            report[15] = playerID;
            canSend = true;
            //printf("Player is %d\n", playerID);
            //printf("----------------------------------------------\n");
            break;
        case SwitchCommands::COMMAND_UNKNOWN_33:
            //printf("SwitchProDriver::set_report: Rpt 0x01 COMMAND_UNKNOWN_33\n");
            // Command typically thrown by Chromium to detect if a Switch controller exists. Can ignore.
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
        case SwitchCommands::SET_HOME_LIGHT:
            //printf("SwitchProDriver::set_report: Rpt 0x01 SET_HOME_LIGHT\n");
            // NYI
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x00;
            canSend = true;
            break;
        case SwitchCommands::TOGGLE_IMU:
            //printf("SwitchProDriver::set_report: Rpt 0x01 TOGGLE_IMU\n");
            if (imuMode != reportData[11]) {
                resetIMUState();
            }
            imuMode = reportData[11];
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x00;
            canSend = true;
            //printf("IMU mode set to %d\n", imuMode);
            //printf("----------------------------------------------\n");
            break;
        case SwitchCommands::IMU_SENSITIVITY:
            //printf("SwitchProDriver::set_report: Rpt 0x01 IMU_SENSITIVITY\n");
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::ENABLE_VIBRATION:
            //printf("SwitchProDriver::set_report: Rpt 0x01 ENABLE_VIBRATION\n");
            isVibrationEnabled = reportData[11];
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x00;
            canSend = true;
            //printf("Vibration set to %d\n", isVibrationEnabled);
            //printf("----------------------------------------------\n");
            break;
        case SwitchCommands::READ_IMU:
            //printf("SwitchProDriver::set_report: Rpt 0x01 READ_IMU\n");
            report[13] = 0xC0;
            report[14] = commandID;
            report[15] = reportData[11];
            report[16] = reportData[12];
            canSend = true;
            //printf("IMU Addr: %02x, Size: %02x\n", reportData[11], reportData[12]);
            //printf("----------------------------------------------\n");
            break;
        case SwitchCommands::GET_VOLTAGE:
            //printf("SwitchProDriver::set_report: Rpt 0x01 GET_VOLTAGE\n");
            report[13] = 0xD0;
            report[14] = 0x50;
            report[15] = 0x83;
            report[16] = 0x06;
            canSend = true;
            break;
        default:
            //printf("SwitchProDriver::set_report: Rpt 0x01 Unknown 0x%02x\n", commandID);
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
    }

    if (canSend) isReportQueued = true;
}

void SwitchProDriver::set_report(uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer, uint16_t bufsize) {
    if (report_type != HID_REPORT_TYPE_OUTPUT) return;

    memset(report, 0x00, bufsize);

    uint8_t switchReportID = buffer[0];
    uint8_t switchReportSubID = buffer[1];
    //printf("SwitchProDriver::set_report Rpt: %02x, Type: %d, Len: %d :: SID: %02x, SSID: %02x\n", report_id, report_type, bufsize, switchReportID, switchReportSubID);
    if (switchReportID == SwitchReportID::REPORT_OUTPUT_00) {
    } else if (switchReportID == SwitchReportID::REPORT_FEATURE) {
        queuedReportID = report_id;
        handleFeatureReport(switchReportID, switchReportSubID, buffer, bufsize);
    } else if (switchReportID == SwitchReportID::REPORT_CONFIGURATION) {
        queuedReportID = report_id;
        handleConfigReport(switchReportID, switchReportSubID, buffer, bufsize);
    } else {
        //printf("SwitchProDriver::set_report Rpt: %02x, Type: %d, Len: %d :: SID: %02x, SSID: %02x\n", report_id, report_type, bufsize, switchReportID, switchReportSubID);
    }
}

void SwitchProDriver::readSPIFlash(uint8_t* dest, uint32_t address, uint8_t size) {
    uint32_t addressBank = address & 0xFFFFFF00;
    uint32_t addressOffset = address & 0x000000FF;
    //printf("Address: %08x, Bank: %04x, Offset: %04x, Size: %d\n", address, addressBank, addressOffset, size);
    std::map<uint32_t, const uint8_t*>::iterator it = spiFlashData.find(addressBank);

    if (it != spiFlashData.end()) {
        // address found
        const uint8_t* data = it->second;
        memcpy(dest, data+addressOffset, size);
        //for (uint8_t i = 0; i < size; i++) printf("%02x ", dest[i]);
        //printf("\n---\n");
    } else {
        // could not find defined address
        //printf("Not Found\n");
        memset(dest, 0xFF, size);
    }
}

// mode2 の姿勢推定状態と timestamp を初期状態へ戻す。
void SwitchProDriver::resetIMUState() {
    // mode2 開始時や IMU 無効化後は identity quaternion から再開する。
    // 古い姿勢を残すと、次回接続時や mode 切替時に Switch 側へ前回の傾きが送られてしまう。
    imuQuaternionX = 0.0f;
    imuQuaternionY = 0.0f;
    imuQuaternionZ = 0.0f;
    imuQuaternionW = 1.0f;
    imuQuaternionLastTimeUs = 0;
    imuMode2TimestampRemainderUs = 0;
    imuMode2Timestamp = 0;
}

// TOGGLE_IMU で要求された mode に応じて raw IMU と quaternion mode2 を振り分ける。
void SwitchProDriver::updateIMUData(Gamepad *gamepad) {
    if (
        imuMode == 0 ||
        !gamepad->auxState.sensors.accelerometer.enabled ||
        !gamepad->auxState.sensors.accelerometer.active ||
        !gamepad->auxState.sensors.gyroscope.enabled ||
        !gamepad->auxState.sensors.gyroscope.active
    ) {
        memset(switchReport.imuData, 0x00, sizeof(switchReport.imuData));
        imuQuaternionLastTimeUs = 0;
        return;
    }

    switch (imuMode) {
        case 0x01:
            updateRawIMUData(gamepad);
            break;
        case 0x02:
            updateMode2IMUData(gamepad);
            break;
        default:
            memset(switchReport.imuData, 0x00, sizeof(switchReport.imuData));
            imuQuaternionLastTimeUs = 0;
            break;
    }
}

// 従来の Switch raw IMU mode。3 sample 分の raw accel/gyro を 36 byte に詰める。
void SwitchProDriver::updateRawIMUData(Gamepad *gamepad) {
    const GamepadAuxSensors& sensors = gamepad->auxState.sensors;
    const uint8_t sampleCount = sensors.imuSampleCount > GAMEPAD_AUX_MAX_IMU_SAMPLES ?
        GAMEPAD_AUX_MAX_IMU_SAMPLES : sensors.imuSampleCount;
    if (sampleCount == 0) {
        for (uint8_t sample = 0; sample < GAMEPAD_AUX_MAX_IMU_SAMPLES; sample++) {
            writeCurrentIMUSample(&switchReport.imuData[sample * 12], gamepad);
        }
        return;
    }

    const uint8_t firstSample = GAMEPAD_AUX_MAX_IMU_SAMPLES - sampleCount;
    for (uint8_t sample = 0; sample < GAMEPAD_AUX_MAX_IMU_SAMPLES; sample++) {
        const uint8_t sampleIndex = sample < sampleCount ?
            GAMEPAD_AUX_MAX_IMU_SAMPLES - 1 - sample :
            firstSample;
        writeIMUSample(&switchReport.imuData[sample * 12], sensors.imuSamples[sampleIndex]);
    }
}

// Switch2 が要求する quaternion mode2 payload を生成する。
void SwitchProDriver::updateMode2IMUData(Gamepad *gamepad) {
    // addon 側 FIFO の最新サンプルを取り出し、mode2 用の内部座標 frame に変換する。
    // raw mode とは別処理にして、Switch2 quaternion mode 用の補正が raw 36 bytes に影響しないようにする。
    GamepadAuxIMUSample sample = getLatestIMUSample(gamepad);
    Mode2IMUFrame frame = toMode2Frame(sample);

    // 前回 report からの経過時間を使って gyro 角速度を積分する。
    // 初回は deltaUs=0 とし、timestamp だけ初期化して姿勢は identity のまま送る。
    uint64_t nowUs = to_us_since_boot(get_absolute_time());
    uint64_t deltaUs = imuQuaternionLastTimeUs == 0 ? 0 : nowUs - imuQuaternionLastTimeUs;
    imuQuaternionLastTimeUs = nowUs;

    updateMode2Attitude(frame, deltaUs);

    // mode2 packet 内の timestamp は ms 単位の 11 bit 値として扱う。
    // report 間隔は us で測るため、1000 us 未満の端数を次回へ繰り越す。
    imuMode2TimestampRemainderUs += deltaUs;
    if (imuMode2TimestampRemainderUs >= 1000) {
        uint64_t wholeMs = imuMode2TimestampRemainderUs / 1000;
        imuMode2TimestampRemainderUs %= 1000;
        imuMode2Timestamp = static_cast<uint16_t>((imuMode2Timestamp + wholeMs) % 0x7FF);
    }

    SwitchIMUMode2Data mode2Data = {};
    float quat[4] = {
        imuQuaternionX,
        imuQuaternionY,
        imuQuaternionZ,
        imuQuaternionW,
    };

    // mode2 の accel0 は wire layout が { y, x, z }。
    // 実機確認の結果、Switch 側の accel.x 解釈は姿勢推定 frame の X と符号が逆だったため、
    // packet へ送る x 成分だけを反転する。Mahony 推定に使う frame.accelX 自体は変更しない。
    mode2Data.accel0 = {
        frame.accelY,
        static_cast<int16_t>(-frame.accelX),
        frame.accelZ,
    };

    // quaternion と accel を 36 byte の mode2 payload に詰め、SwitchProReport::imuData へコピーする。
    mode2Data.mode = 2;
    packMode2Quaternion(mode2Data, quat);
    mode2Data.timestampStartLow = imuMode2Timestamp & 0x01;
    mode2Data.timestampStartHigh = (imuMode2Timestamp >> 1) & 0x03FF;
    mode2Data.timestampCount = 3;

    memcpy(switchReport.imuData, &mode2Data, sizeof(mode2Data));
}

// BoardConfig 適用後の IMU sample を、mode2 の姿勢推定で使う signed frame に変換する。
SwitchProDriver::Mode2IMUFrame SwitchProDriver::toMode2Frame(const GamepadAuxIMUSample& sample) const {
    // GamepadAuxIMUSample は BoardConfig と WebConfig の軸設定を反映済みの値。
    // ここでは mode2 姿勢推定で使う signed int16 値に戻し、raw 経路とは独立した frame として扱う。
    const int16_t accelX = signedIMUValue(sample.accelerometer.x);
    const int16_t accelY = signedIMUValue(sample.accelerometer.y);
    const int16_t accelZ = signedIMUValue(sample.accelerometer.z);
    const int16_t gyroX = signedIMUValue(sample.gyroscope.x);
    const int16_t gyroY = signedIMUValue(sample.gyroscope.y);
    const int16_t gyroZ = signedIMUValue(sample.gyroscope.z);

    Mode2IMUFrame frame;
    // 現在の BoardConfig が Switch mode2 用の軸割り当てを担う。
    // そのため、ここでは X/Y/Z を入れ替えず、Mahony 推定用の内部 frame へそのまま渡す。
    frame.accelX = accelX;
    frame.accelY = accelY;
    frame.accelZ = accelZ;

    frame.gyroX = gyroX;
    frame.gyroY = gyroY;
    frame.gyroZ = gyroZ;
    return frame;
}

// 加速度を姿勢補正に使ってよい状態かを判定する。
bool SwitchProDriver::isAccelUsable(const Mode2IMUFrame& frame) const {
    // 加速度補正は、加速度ベクトルの大きさが重力 1g 付近の時だけ使う。
    // 激しい操作中や offset 設定で重力成分が消えている時は、誤った補正を避けるため gyro 積分だけにする。
    const float accelX = static_cast<float>(frame.accelX);
    const float accelY = static_cast<float>(frame.accelY);
    const float accelZ = static_cast<float>(frame.accelZ);
    const float magnitudeSquared = (accelX * accelX) + (accelY * accelY) + (accelZ * accelZ);
    return magnitudeSquared >= (SWITCH_PRO_IMU_ACCEL_MIN * SWITCH_PRO_IMU_ACCEL_MIN) &&
        magnitudeSquared <= (SWITCH_PRO_IMU_ACCEL_MAX * SWITCH_PRO_IMU_ACCEL_MAX);
}

// gyro 積分と最小構成の Mahony 補正で内部 quaternion を更新する。
void SwitchProDriver::updateMode2Attitude(const Mode2IMUFrame& frame, uint64_t deltaUs) {
    if (deltaUs == 0) {
        return;
    }

    // 経過時間は秒に変換して quaternion 積分に使う。
    // 上限を設け、report 欠落などで dt が大きくなった時の姿勢飛びを抑える。
    const float deltaTime = std::min(static_cast<float>(deltaUs) / 1000000.0f, SWITCH_PRO_IMU_MAX_DELTA_SECONDS);

    // LSM6DS3 の 2000 dps raw 値を rad/s に変換する。
    // ここで得た角速度が quaternion 微分の入力になる。
    float gyroX = static_cast<float>(frame.gyroX) * SWITCH_PRO_IMU_GYRO_RAD_PER_SEC;
    float gyroY = static_cast<float>(frame.gyroY) * SWITCH_PRO_IMU_GYRO_RAD_PER_SEC;
    float gyroZ = static_cast<float>(frame.gyroZ) * SWITCH_PRO_IMU_GYRO_RAD_PER_SEC;

    if (isAccelUsable(frame)) {
        // 加速度ベクトルを正規化し、現在姿勢から推定される重力方向と比較する。
        // ずれを角速度へ足すことで、gyro 積分だけでは戻りにくい roll/pitch を重力方向へ寄せる。
        float accelX = static_cast<float>(frame.accelX);
        float accelY = static_cast<float>(frame.accelY);
        float accelZ = static_cast<float>(frame.accelZ);
        const float accelNorm = std::sqrt((accelX * accelX) + (accelY * accelY) + (accelZ * accelZ));
        if (accelNorm > 0.0f) {
            const float accelNormInverse = 1.0f / accelNorm;
            accelX *= accelNormInverse;
            accelY *= accelNormInverse;
            accelZ *= accelNormInverse;

            const float estimatedGravityX = 2.0f * ((imuQuaternionX * imuQuaternionZ) - (imuQuaternionW * imuQuaternionY));
            const float estimatedGravityY = 2.0f * ((imuQuaternionW * imuQuaternionX) + (imuQuaternionY * imuQuaternionZ));
            const float estimatedGravityZ =
                (imuQuaternionW * imuQuaternionW) - (imuQuaternionX * imuQuaternionX) -
                (imuQuaternionY * imuQuaternionY) + (imuQuaternionZ * imuQuaternionZ);

            // accel と推定重力の外積が、姿勢を補正する回転方向になる。
            // SWITCH_PRO_IMU_MAHONY_KP が 0.0f の時はこの補正は無効で、軸検証しやすい gyro-only 動作になる。
            const float errorX = (accelY * estimatedGravityZ) - (accelZ * estimatedGravityY);
            const float errorY = (accelZ * estimatedGravityX) - (accelX * estimatedGravityZ);
            const float errorZ = (accelX * estimatedGravityY) - (accelY * estimatedGravityX);

            gyroX += SWITCH_PRO_IMU_MAHONY_KP * errorX;
            gyroY += SWITCH_PRO_IMU_MAHONY_KP * errorY;
            gyroZ += SWITCH_PRO_IMU_MAHONY_KP * errorZ;
        }
    }

    // quaternion 微分 q_dot = 0.5 * q * omega を Euler 積分する。
    // omega は上で補正済みの gyro 角速度で、q は内部状態の現在姿勢。
    float nextW = imuQuaternionW + ((-imuQuaternionX * gyroX - imuQuaternionY * gyroY - imuQuaternionZ * gyroZ) * 0.5f * deltaTime);
    float nextX = imuQuaternionX + ((imuQuaternionW * gyroX + imuQuaternionY * gyroZ - imuQuaternionZ * gyroY) * 0.5f * deltaTime);
    float nextY = imuQuaternionY + ((imuQuaternionW * gyroY - imuQuaternionX * gyroZ + imuQuaternionZ * gyroX) * 0.5f * deltaTime);
    float nextZ = imuQuaternionZ + ((imuQuaternionW * gyroZ + imuQuaternionX * gyroY - imuQuaternionY * gyroX) * 0.5f * deltaTime);

    // 積分誤差で quaternion の長さが 1 から外れるため、毎回正規化する。
    // 非有限値が混じった場合は安全側に倒して identity へ戻す。
    const float norm = std::sqrt((nextX * nextX) + (nextY * nextY) + (nextZ * nextZ) + (nextW * nextW));
    if (!(norm > 0.0f) || !std::isfinite(norm)) {
        resetIMUState();
        return;
    }

    const float normInverse = 1.0f / norm;
    nextX *= normInverse;
    nextY *= normInverse;
    nextZ *= normInverse;
    nextW *= normInverse;

    // q と -q は同じ姿勢を表すが、符号が急に反転すると mode2 圧縮後の値が跳ねる。
    // 前回 quaternion と同じ半球にそろえて、送信値の連続性を保つ。
    const float hemisphereDot =
        (nextX * imuQuaternionX) + (nextY * imuQuaternionY) +
        (nextZ * imuQuaternionZ) + (nextW * imuQuaternionW);
    if (hemisphereDot < 0.0f) {
        nextX = -nextX;
        nextY = -nextY;
        nextZ = -nextZ;
        nextW = -nextW;
    }

    imuQuaternionX = nextX;
    imuQuaternionY = nextY;
    imuQuaternionZ = nextZ;
    imuQuaternionW = nextW;
}

// FIFO があれば最新 sample、なければ現在の auxState を使う。
GamepadAuxIMUSample SwitchProDriver::getLatestIMUSample(Gamepad *gamepad) {
    const GamepadAuxSensors& sensors = gamepad->auxState.sensors;
    const uint8_t sampleCount = sensors.imuSampleCount > GAMEPAD_AUX_MAX_IMU_SAMPLES ?
        GAMEPAD_AUX_MAX_IMU_SAMPLES : sensors.imuSampleCount;
    if (sampleCount > 0) {
        return sensors.imuSamples[GAMEPAD_AUX_MAX_IMU_SAMPLES - 1];
    }

    GamepadAuxIMUSample sample;
    sample.accelerometer = sensors.accelerometer;
    sample.gyroscope = sensors.gyroscope;
    return sample;
}

void SwitchProDriver::writeIMUSample(uint8_t *dest, const GamepadAuxIMUSample& sample) {
    uint16_t values[6] = {
        sample.accelerometer.x,
        sample.accelerometer.y,
        sample.accelerometer.z,
        sample.gyroscope.x,
        sample.gyroscope.y,
        sample.gyroscope.z,
    };

    for (uint8_t i = 0; i < 6; i++) {
        dest[i * 2] = values[i] & 0xFF;
        dest[(i * 2) + 1] = (values[i] >> 8) & 0xFF;
    }
}

void SwitchProDriver::writeCurrentIMUSample(uint8_t *dest, Gamepad *gamepad) {
    GamepadAuxIMUSample sample;
    sample.accelerometer = gamepad->auxState.sensors.accelerometer;
    sample.gyroscope = gamepad->auxState.sensors.gyroscope;
    writeIMUSample(dest, sample);
}

// Only XboxOG and Xbox One use vendor control xfer cb
bool SwitchProDriver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false;
}

const uint16_t * SwitchProDriver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
	const char *value = (const char *)switch_pro_string_descriptors[index];
	return getStringDescriptor(value, index); // getStringDescriptor returns a static array
}

const uint8_t * SwitchProDriver::get_descriptor_device_cb() {
    return switch_pro_device_descriptor;
}

const uint8_t * SwitchProDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return switch_pro_report_descriptor;
}

const uint8_t * SwitchProDriver::get_descriptor_configuration_cb(uint8_t index) {
    return switch_pro_configuration_descriptor;
}

const uint8_t * SwitchProDriver::get_descriptor_device_qualifier_cb() {
	return nullptr;
}

uint16_t SwitchProDriver::GetJoystickMidValue() {
    return SWITCH_PRO_JOYSTICK_MID;
}
