// SPDX-FileCopyrightText: 2017-2020 Melbot Studios, S.L.
// SPDX-License-Identifier: LicenseRef-Melbot-Portfolio
//
// Original author: Miguel Angel Exposito (2017-2020)
//
// This source code is the property of Melbot Studios, S.L. and is published
// publicly for portfolio and educational review purposes with the express
// written authorization of Melbot Studios, S.L.
//
// All rights remain with Melbot Studios, S.L. Redistribution, modification,
// commercial use, or derivative works are not permitted without prior
// written consent of the copyright holder. See LICENSE for full terms.

// PIN ASSIGNMENTS ===================================
// DIGITAL OUTPUTS -----------------------------------
#define MBT_CFG_PIN_LED_R       (23)
#define MBT_CFG_PIN_LED_G       (20)
#define MBT_CFG_PIN_LED_B       (22)
#define MBT_CFG_PIN_LED_Q1      (26)
#define MBT_CFG_PIN_LED_Q2      (25)
#define MBT_CFG_PIN_LED_Q3      (31)
#define MBT_CFG_PIN_LED_Q4      (24)
#define MBT_CFG_PIN_MOTOR       (19)

#define MBT_CFG_PIN_FLASH_NCS   (5)

#define MBT_CFG_PIN_AUDIO_OUT   (27)
#define MBT_CFG_PIN_AUDIO_PA    (29)

#define MBT_CFG_PIN_AVDD        (30)

// DIGITAL BUSSES ------------------------------------
#define MBT_CFG_PIN_I2C_SCL     (8)
#define MBT_CFG_PIN_I2C_SDA     (7)

#define MBT_CFG_PIN_SPI_SCK     (16)
#define MBT_CFG_PIN_SPI_MISO    (15)
#define MBT_CFG_PIN_SPI_MOSI    (14)

// DIGITAL INPUTS ------------------------------------
#define MBT_CFG_PIN_RESET       (21)

#define MBT_CFG_PIN_ACCELINT1   (12)
#define MBT_CFG_PIN_ACCELINT2   (18)
#define MBT_CFG_PIN_VBUS        (13)
#define MBT_CFG_PIN_BATTCHG     (17)

// ANALOG INPUTS -------------------------------------
#define MBT_CFG_PIN_AIN_LIGHT   (2) //(NRF_SAADC_INPUT_AIN1)
#define MBT_CFG_PIN_AIN_TEMP    (1) //(NRF_SAADC_INPUT_AIN0)
#define MBT_CFG_PIN_AIN_BATT    (5) //(NRF_SAADC_INPUT_AIN4)
#define MBT_CFG_PIN_AIN_ML      (3) //(NRF_SAADC_INPUT_AIN2)

// --------------------- HAL -------------------------

// I2C ===============================================
#define MBT_CFG_TWI_INSTANCE_ID                     0
#define MBT_CFG_MAX_PENDING_TRANSACTIONS            5

// WDT ===============================================
#define MBT_CFG_WDT_TIMEOUT_MS                      (5000)

// Accelerometer =====================================
#define MBT_CFG_ACCEL_SC7A20
#define MBT_CFG_ACCEL_I2C_ADDR                      (0x19)
#define MBT_CFG_ACCEL_SC7A20_USE_INT1_FOR_MOTION
#define MBT_CFG_ACCEL_HANDLE_GPIO_ON_INT

// RTC ===============================================
#define MBT_CFG_RTC_CLK_FREQ_HZ                     (32768)
#define MBT_RTC_US_PER_TICK                         (1000000ULL / MBT_CFG_RTC_CLK_FREQ_HZ) //(30) //0,000030517578125?s = 1000000 / MBT_RTC_CLK_FREQ_HZ

// ANALOG ============================================
#define MBT_CFG_ANALOG_BUFFER_SIZE_SAMPLES          (32)  // Actual size in RAM is this x sizeof(Sample) x 2
#define MBT_CFG_ANALOG_DEFAULT_SAMPLE_RATE_HZ       (120000)
#define MBT_CFG_ANALOG_VCC_REF_MV                   (3300)
#define MBT_CFG_ANALOG_RESOLUTION_BITS              14
#define MBT_CFG_ANALOG_AVDD_DELAY_MS                (100)

// AUDIO =============================================
// The actual size in RAM will be MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES * SIZE_OF_SAMPLE * 2
#define MBT_CFG_AUDIO_BUFFER_SIZE_SAMPLES           (160)


// BLE ===============================================
#define MBT_CFG_BLE_COMPANY_IDENTIFIER              (0x091E) // Bluetooth SIG Company allocated Identifier of Melbot Studios, S.L
#define MBT_CFG_BLE_DEVICE_NAME                     "MBP"    // Short name of the device in the broadcast packet
#define MBT_CFG_BLE_TX_POWER_DBM                    (-4)     // Not all values are supported
#define MBT_CFG_BLE_ADV_MANUFDATA_PACKET_VERSION    (0)      // Version of the Advertising Manufacturer Data packet
#define MBT_CFG_BLE_MAX_PAYLOAD_LEN                 (244)

// --------------------- POD -------------------------
#define MBT_CFG_VIB_DEFAULT_TIME_MS                 (1000)

// SYNTH =============================================
#define MBT_CFG_SYNTH_N_VOICES                      (11)
#define MBT_CFG_SYNTH_SAMPLE_RATE_HZ                (16000UL)

// MOTION ============================================
#define MBT_CFG_MOTION_ALG_SHAKE_NUMPEAKS                   (6)
#define MBT_CFG_MOTION_ALG_SHAKE_DEADZONE_mG                (80)
#define MBT_CFG_MOTION_ALG_SHAKE_INACTIVITY_TIMEOUT_MS      (500)
#define MBT_CFG_MOTION_ALG_SHAKE_MIN_TIME_BETWEEN_PEAKS_MS  (50)
#define MBT_CFG_MOTION_ALG_SHAKE_ACCEL_WINDOW_SIZE_mG       (40)


#define MBT_CFG_MOTION_INACTIVITY_TIMEOUT_MS        (MBT_SECONDS_TO_MS(1))
#define MBT_CFG_MOTION_INACTIVITY_DEADZONE_mG       (50)
#define MBT_CFG_MOTION_AWARENESS_TIMEOUT_MS         (MBT_MINUTES_TO_MS(5))
#define MBT_CFG_MOTION_SUBSTREAM_RATE_HZ            (25) // Must be  amultiple of 25

// MLINK =============================================
#define MBT_CFG_ML_TIMEOUT_MS                       (5*60*1000)
#define MBT_CFG_ML_INITIAL_TIMEOUT_MS               (3*60*1000)         // Time to keep ML active on first boot
#define MBT_CFG_ML_SAMPLE_BUFFER_SIZE_SAMPLES       (64)

// STREAM SERVER =====================================
//#define MBT_CFG_SS_WAIT_FOR_ALL_SAMPLES                               // Wait for all sensor samples to become available before sending the next packet

// POWER =============================================
#define MBT_CFG_POWER_BATT_LOW_THRESHOLD_MV         (3200)             // Low battery voltage threshold
#define MBT_CFG_POWER_BATT_DEAD_THRESHOLD_MV        (3010)             // Battery dead voltage threshold

#define MBT_CFG_POWER_VBUS_FSM_DELAY_TIME_MS        (100)              // Delay time to debounce the USB VBUS line
#define MBT_CFG_POWER_BATTCHG_DELAY_TIME_MS         (200)              // Time to measure changes in the BATTCHG line (No batt)
#define MBT_CFG_POWER_BATT_MON_DELAY_TIME_MS        (30000)            // Delay time to measure the battery
#define MBT_CFG_POWER_BATT_MON_DELAY_LOW_TIME_MS    (5000)             // Delay time to measure the battery when it's low
#define MBT_CFG_POWER_BATT_MON_DELAY_EXTPWR_TIME_MS (1000)             // Delay time to measure the battery when external power is attached

// UI ================================================
#define MBT_CFG_UI_BATT_USERACTIVITY_TIMEOUT_MS     (60000 * 5)        // Delay time to warn the user when the battery is low
#define MBT_CFG_UI_WELCOMEFEEDBACK_DELAY_MS         (500)               // Welcome feedback delay

// EVOLUTION ACTIVITY ================================
#define MBT_CFG_ACT_EVO_BUFFER_SIZE_SAMPLES         (128)              // Max number of samples

// RESET =============================================
#define MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_N_PRESSES          (5)     // Number of consecutive times the RESET button must be pressed in order to perform a factory reset
#define MBT_CFG_RESETBTN_TEST_MODE_ENABLE_N_PRESSES                     (2)     // Number of consecutive times the RESET button must be pressed in order to perform a factory reset
#define MBT_CFG_RESETBTN_MAX_DELAY_MS                                   (1000)  // Max time allowed between RESET presses to perform an action (test mode, reset factory settings)
#define MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_VIB_LEVEL          (255)   // Vibration level used by the motor when the factory reset is performed
#define MBT_CFG_RESETBTN_USER_SETTINGS_FACTORY_RESET_VIB_TIME_MS        (1000)   // Time that the motor vibrates when the factory reset is performed

// VERSION ===========================================
#define MBT_CFG_FW_VERSION                          (APP_VERSION)       // Defined from preprocessor cmdline
#define MBT_CFG_FW_FEATURELEVEL                     (FEATURELEVEL)      // This determines the need to update (Defined from preprocessor cmdline)

// FACTORY ===========================================
#define MBT_CFG_FACTORY_PRODUCT_CODE                ("MLB202101")
#define MBT_CFG_FACTORY_HW_REVISION                 (PRODUCT_HW_REVISION)

// PLATFORM ==========================================
#define MBT_CFG_PLATFORM_NRF52_USE_DCDC
#define MBT_CFG_PLATFORM_DATARAM_START_ADDRESS      (0x20000000UL)
#define MBT_CFG_PLATFORM_SECTION_RETAINED_DATA      __attribute__((section(".non_init"))) __attribute__((used))
#define MBT_CFG_PLATFORM_SECTION_NVM                __attribute__((section(".flash_storage"))) __attribute__((used))
#define MBT_CFG_PLATFORM_PACKED                     __attribute__((packed))
#define MBT_CFG_PLATFORM_ALIGNED(N)                 __attribute__((aligned (N)))
