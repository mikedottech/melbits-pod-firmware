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

#ifndef _HAL_H_
#define _HAL_H_

#include "common.h"
#include "mbt_config.h"

typedef int16_t HAL_analogSample_t;  // nrf_saadc_value_t

#define HAL_SYS_TICKRATE_OFF    (0x0) 
#define HAL_SYS_TICKRATE_1HZ    (1000UL)
#define HAL_SYS_TICKRATE_10HZ   (100UL)
#define HAL_SYS_TICKRATE_25HZ   (40UL)
#define HAL_SYS_TICKRATE_50HZ   (20UL)
#define HAL_SYS_TICKRATE_100HZ  (10UL)
#define HAL_SYS_TICKRATE_MAX    (0xFFFFFFFFUL)

typedef char HAL_MBT_ProductCode_t[10];

typedef struct
{
    uint8_t                 structVersion;      // Version of this struct
    HAL_MBT_ProductCode_t   productCode;        // Product code string
    uint32_t                productId;          // Numeric product ID
    uint8_t                 hwRevision;         // Revision of the PCB
    uint32_t                reserved;           // For future use    
} MBT_CFG_PLATFORM_PACKED MBT_FactoryInfo_t;

typedef struct
{
    uint8_t                 magic;
    uint8_t                 version;
    MBT_FactoryInfo_t       factoryInfo;
    uint32_t                fwVersion;          // Firmware version
    uint8_t                 fwFeatureLevel;     // Feature level
    uint64_t                deviceId;           // CPU (pseudo) serial number
} MBT_CFG_PLATFORM_PACKED HAL_DeviceInfo_t;

typedef uint8_t HAL_AdvPacketVersion_t;

typedef struct
{
    /// ------ PLAIN SECTION -------------------
    struct
    {
        HAL_AdvPacketVersion_t  packetVersion;      // (1) Version of this packet
    } MBT_CFG_PLATFORM_PACKED plain;
    //uint32_t                hash;             // (4) Hash of this packet
    /// ------ XOR'd SECTION -------------------
    struct
    {
        uint8_t                 productId[3];       // (3) Product ID = pppphh  (p = product code, h = hw revision)
        uint16_t                fwVersionShort;     // (2) Firmware version
        uint8_t                 fwFeatureLevel;     // (1) Feature level
        uint8_t                 associationToken;   // (1) AT for MLINK pairing
        uint8_t                 advertisingFlags;   // (1) Application-specific flags
        uint32_t                deviceId;           // (4) Last 4 bytes of the device UUID
    } MBT_CFG_PLATFORM_PACKED obf;
} MBT_CFG_PLATFORM_PACKED HAL_BroadcastPacket_t;

typedef enum
{
    HALEVENTTYPE_INVALID = 0,
    HALEVENTTYPE_ACCEL,
    HALEVENTTYPE_GPIO,
    HALEVENTTYPE_BLE,
    HALEVENTTYPE_ANALOG,
    HALEVENTTYPE_FLASH
} HAL_eventType_t;

typedef enum
{
    HALACCELEVENTTYPE_MOTION,
    HALACCELEVENTTYPE_TAP,
    HALACCELEVENTTYPE_DBLTAP,    
    HALACCELEVENTTYPE_DATA
} HAL_accelEventType_t;

typedef enum
{
    HALBLEEVENTTYPE_CONNECTION,
    HALBLEEVENTTYPE_ADVERTISEMENT,
    HALBLEEVENTTYPE_DATARX,
    HALBLEEVENTTYPE_RX_OVERFLOW,
    HALBLEEVENTTYPE_TX_READY
} HAL_bleEventType_t;

typedef enum
{
    HALANALOGEVENTTYPE_CONVDONE
} HAL_analogEventType_t;

typedef struct
{
    HAL_eventType_t type;	

    union
    {
        struct
        {
            HAL_accelEventType_t type;
        } accelEvent;
        struct
        {
            uint8_t pin;
        } gpioEvent;
        struct
        {
            HAL_bleEventType_t type;
        } bleEvent;
        struct
        {
            HAL_analogEventType_t type;
        } analogEvent;
        struct
        {
            bool success;
        } flashEvent;
    } data;
} HAL_event_t;

typedef struct
{
    uint8_t *pData;
    uint8_t len;
} HAL_smallBuffer_t;

extern void HAL_onEvent(const HAL_event_t *pEventData);
extern void HAL_ISR_onBLERX(const uint8_t *pData, uint16_t len);

void HAL_preinit();
void HAL_init();
void HAL_preUpdate();
void HAL_postUpdate();

// System
// ===============================================================
uint32_t HAL_sysGetRawTickCount();
uint32_t HAL_sysGetDeltaTickCount(uint32_t to, uint32_t from);
uint32_t HAL_sysSetTickRate(uint32_t interval_ms);
void HAL_sysEnterDFUMode();

void HAL_sysSchedulePendingTick();

void HAL_sysEnterCriticalSection();
void HAL_sysLeaveCriticalSection();

HAL_DeviceInfo_t *HAL_sysGetDevInfo();

#define HAL_CRITICAL_SECTION_ENTER()\
do{\
  HAL_sysEnterCriticalSection()

#define HAL_CRITICAL_SECTION_LEAVE()\
  HAL_sysLeaveCriticalSection();\
} while(0)

// LEDs
// ===============================================================
// values: Duty cycle values (0..255) for each LED in the following order: R, G, B, Q1, Q2, Q3, Q4
uint32_t HAL_LEDSetMultiple(const uint8_t* values);

// Motor
// ===============================================================
// level: Motor rotational speed (0: off, 255: max). The motor requires a minimum level to start rotating from stop.
uint32_t HAL_motorSetLevel(uint8_t level);

// Accelerometer
// ===============================================================
typedef struct
{
    int16_t x, y, z;    // Full range. Depends on scale.
} MBT_CFG_PLATFORM_PACKED HAL_accelSample_t;

typedef struct
{
    int32_t x, y, z;
} HAL_accelSampleScaled_t;


typedef struct
{
    uint8_t rate;
    uint8_t axis;
    uint8_t lowpower;
    uint8_t range;
} HAL_accelMode_t;

typedef struct
{
    uint8_t target;
    uint8_t mode;
    uint8_t cutoff;
    uint8_t ref;
} HAL_accelFilterConfig_t;

typedef struct
{
    uint8_t axis;
    uint16_t threshold_mg;
    uint16_t duration_ms;
} HAL_accelMotionDetectionConfig_t;

typedef struct
{
    uint8_t axis;
    uint16_t threshold_mg;
    uint16_t time_limit_ms;
    uint16_t time_latency_ms;
    uint16_t time_window_ms;
} HAL_accelClickDetectionConfig_t;


uint32_t HAL_accelSetConfig(const HAL_accelMode_t *acfg);
uint32_t HAL_accelSetFilterConfig(const HAL_accelFilterConfig_t *acfg);
uint32_t HAL_accelSetMotionDetectionConfig(const HAL_accelMotionDetectionConfig_t *mcfg);
uint32_t HAL_accelSetClickDetectionConfig(const HAL_accelClickDetectionConfig_t *ccfg);
uint32_t HAL_accelSetDataReporting(bool enabled);
uint32_t HAL_accelApplyConfig(void);
uint32_t _HAL_accelGetClearEventFlags(void);
bool HAL_accelPullSample(HAL_accelSample_t *buffer);
//void HAL_accelGetLastSampleScaled(HAL_accelSampleScaled_t *sample);
void HAL_accelScaleSample(const HAL_accelSample_t *sample_in, HAL_accelSampleScaled_t *sample_out);
void HAL_accelForceReset(void);

// Audio
// ===============================================================
uint32_t HAL_audioOutBegin(bool enablePA);
void HAL_setAudioPAState(bool state);
uint32_t HAL_audioOutEnd(void);
void HAL_audioAbort(void);

// GPIO
// ===============================================================
uint32_t HAL_gpioWriteDigital(uint8_t pin, bool value);
void HAL_gpioReadDigital(uint8_t pin, uint32_t *data);

// AVDD
// ===============================================================
uint32_t HAL_avddSetLevel(uint8_t level);

// ANALOG
// ===============================================================
typedef struct
{
    uint8_t pin_id;
    uint8_t scale;
} HAL_analogChannelConfig;

#define HAL_ANALOG_CH_TEMP  (0)
#define HAL_ANALOG_CH_LIGHT (1)
#define HAL_ANALOG_CH_ML    (2)
#define HAL_ANALOG_CH_BATT  (3)

//uint32_t HAL_analogSetChannelConfig(uint8_t chId, HAL_analogChannelConfig *pCfg);
uint32_t HAL_analogSetEnabled(bool enabled);
bool HAL_analogIsRunning(void);
//uint32_t HAL_analogSetSampleRate(uint8_t rate);
uint32_t HAL_analogGetLastBuffer(HAL_analogSample_t **ppBuffer, uint16_t *pLen);
uint32_t HAL_analogReturnBuffer(HAL_analogSample_t *pBuffer);
uint32_t HAL_analogRawToMv(uint8_t ch, HAL_analogSample_t sample);


// BLE
// ===============================================================

#define HAL_BLE_EVENT_POS_CONNECTION        (0)
#define HAL_BLE_EVENT_POS_ADVERTISEMENT     (1)
#define HAL_BLE_EVENT_POS_DATARX            (2)
#define HAL_BLE_EVENT_POS_RX_OVERFLOW       (3)
#define HAL_BLE_EVENT_POS_TX_READY          (4)

#define HAL_BLE_EVENT_FLAG_CONNECTION       (1UL << HAL_BLE_EVENT_POS_CONNECTION)
#define HAL_BLE_EVENT_FLAG_ADVERTISEMENT    (1UL << HAL_BLE_EVENT_POS_ADVERTISEMENT)
#define HAL_BLE_EVENT_FLAG_DATARX           (1UL << HAL_BLE_EVENT_POS_DATARX)
#define HAL_BLE_EVENT_FLAG_RX_OVERFLOW      (1UL << HAL_BLE_EVENT_POS_RX_OVERFLOW)
#define HAL_BLE_EVENT_FLAG_TX_READY         (1UL << HAL_BLE_EVENT_POS_TX_READY)

typedef enum
{
    HAL_BLE_CONN_STATE_DISCONNECTED,
    HAL_BLE_CONN_STATE_CONNECTING,
    HAL_BLE_CONN_STATE_CONNECTED
} HAL_bleConnState_t;

typedef enum
{
    HAL_BLE_ADV_STATE_OFF,
    HAL_BLE_ADV_STATE_SLOW,
    HAL_BLE_ADV_STATE_FAST
} HAL_bleAdvState_t;

typedef struct
{
    bool fast;
    //bool directed;
    HAL_smallBuffer_t initialAdvertisingData;
} HAL_bleAdvertisingConfig_t;

uint32_t HAL_bleStartAdvertising(HAL_bleAdvertisingConfig_t *adv);
uint32_t HAL_bleUpdateAdvertisingData(HAL_smallBuffer_t *pData);
bool HAL_bleIsAdvertising(void);
uint32_t HAL_bleStopAdvertising(void);
uint32_t HAL_bleDisconnect(void);
uint32_t HAL_bleGetMTU(void);
uint32_t HAL_bleEnqueueSendNotification(const uint8_t *pData, uint16_t *pLen, bool enqueue);
uint32_t HAL_bleReadBytes(const uint8_t **ppData, size_t *pLen);
uint32_t HAL_bleFreeBytes(size_t len);
size_t HAL_bleGetAvailableReadBytes(void);

HAL_bleConnState_t HAL_bleGetConnState(void);
HAL_bleAdvState_t HAL_bleGetAdvState(void);

// POWER
// ===============================================================
uint32_t HAL_powerEnterLockupMode();

// FLASH
// ===============================================================
void HAL_flashGetStorageAbsoluteAddr(uint8_t **ppAddr, uint16_t *pLen);

bool HAL_flashErasePage(uint16_t offset);
bool HAL_flashWrite(const uint32_t* data, uint16_t offset, uint16_t len);

// CRYPT
// ===============================================================

typedef volatile struct
{
    uint8_t data[16 + 16 + 16];
} HAL_cryptCtx_t;

typedef volatile uint32_t CommCtr_t;

void HAL_cryptNonceGenerate(uint8_t *p_buf);
void HAL_cryptCtrInit(HAL_cryptCtx_t *p_ctx, const uint8_t *p_nonce, const uint8_t *p_ecb_key);
uint32_t HAL_cryptCtrEncrypt(HAL_cryptCtx_t *p_ctx, uint8_t *p_clear_text, uint16_t len, CommCtr_t *pCtr);
uint32_t HAL_cryptCtrDecrypt(HAL_cryptCtx_t *p_ctx, uint8_t *p_cipher_text, uint16_t len, CommCtr_t *pCtr);

// DEBUG
// ===============================================================
void HAL_dbgLog(const char* fmt, ...);

#endif