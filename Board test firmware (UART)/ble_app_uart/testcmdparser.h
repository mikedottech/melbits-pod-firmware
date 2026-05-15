#ifndef CMP_H__
#define CMP_H__

#include "sdk_common.h"
#include "nrf_saadc.h"

#ifndef ACCTRONBOARD
typedef enum
{
    OUTID_AVDD = 17,
    OUTID_VIBRATION = 18,
    OUTID_SPK = 19,
    OUTID_R = 22,
    OUTID_G = 20,
    OUTID_B = 23,
    OUTID_N = 6,
    OUTID_S = 7,
    OUTID_E = 8,
    OUTID_W = 9,
    OUTID_AUDIO = 5
} cmp_outID_t;

typedef enum
{
    INID_USB = 24,
    INID_MLINK = 25
} cmp_inID_t;

typedef enum
{
    AINID_LIGHT = NRF_SAADC_INPUT_AIN5,    // A5
    AINID_TEMP = NRF_SAADC_INPUT_AIN6,     // A6
    AINID_BATT = NRF_SAADC_INPUT_AIN4      // A4
} cmp_ainID_t;

#define PIN_I2C_SDA 14
#define PIN_I2C_SCL 15

#else
typedef enum
{
    OUTID_AVDD = 30,
    OUTID_VIBRATION = 19,
    OUTID_SPK = 29,
    OUTID_R = 23,
    OUTID_G = 20,
    OUTID_B = 22,
    OUTID_N = 24,
    OUTID_S = 25,
    OUTID_E = 26,
    OUTID_W = 31,
    OUTID_AUDIO = 27,
    OUTID_FLASH_nCS = 5,
    OUTID_SPI_SCK = 16,
    OUTID_SPI_MISO = 15,
    OUTID_SPI_MOSI = 14,
    OUTID_I2C_SDA = 7,
    OUTID_I2C_SCL = 8
} cmp_outID_t;

typedef enum
{
    INID_USB = 13,
    INID_MLINK = 11,
    INID_BATTCHG = 17,
    INID_ACCELINT1 = 12,
    INID_ACCELINT2 = 18,
    INID_RESET = 21
} cmp_inID_t;

typedef enum
{
    AINID_LIGHT = NRF_SAADC_INPUT_AIN1,    // A1
    AINID_TEMP = NRF_SAADC_INPUT_AIN0,     // A0
    AINID_BATT = NRF_SAADC_INPUT_AIN4,      // A4
    AINID_MLINK = 4                         // A2
} cmp_ainID_t;

#define PIN_I2C_SDA 7 //7
#define PIN_I2C_SCL 8 //8

#endif


typedef enum
{
    EVENTTYPE_DIGITAL_SET,
    EVENTTYPE_DIGITAL_READ,
    EVENTTYPE_ANALOG_READ,
    EVENTTYPE_TONE,
    EVENTTYPE_ACCELTEST,
    EVENTTYPE_ACCELTEST2,
    EVENTTYPE_ACCELTEST3,
    EVENTTYPE_I2C,
    EVENTTYPE_SEQ,
    EVENTTYPE_ERROR
} cmp_eventtype_t;

typedef enum
{
    ACTIVATION_OFF = 0,
    ACTIVATION_ON,
    ACTIVATION_TOGGLE
} cmp_activation_t;

typedef struct
{
    cmp_eventtype_t type;
    union
    {
        struct
        {
            cmp_inID_t inId;
        } digitalread;
        struct
        {
            cmp_outID_t outId;
            cmp_activation_t activation;
        } digitalset;
        struct
        {
            cmp_ainID_t ainID;
        } analogread;
        struct
        {
            uint8_t seqID;
        } seqtrigger;
        struct
        {
            uint8_t isRead;
            uint8_t addr;
            uint8_t value;
        } i2c;
    };
} cmp_event_t;

typedef void (* cmp_eventhandler_t)(cmp_event_t event);

void cmp_init(cmp_eventhandler_t handler);
void cmp_feed_data(const uint8_t* pData, uint16_t size);

#endif
