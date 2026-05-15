#ifndef _BOARD_H_
#define _BOARD_H_

#include "nordic_common.h"
#include "inttypes.h"

// TODO: Make this common and control with macros

#define ACCELEROMETER_IRQ_POLARITY NRF_GPIO_PIN_SENSE_HIGH

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
    OUTID_AUDIO = 27
} cmp_outID_t;

typedef enum
{
    INID_USB = 13,
    INID_MLINK = 11,
    INID_RESET = 21,
    INID_ACCEL1_IRQ = 12,
    INID_ACCEL2_IRQ = 18,
    INID_BATTCHG_IRQ = 17,
    INID_USB_DETECT_IRQ = 13
} cmp_inID_t;


void board_init(void);
void board_setLED(cmp_outID_t id, uint8_t state);
void board_setDefaultOutputs(void);
void board_setupWakeupSources(void);

#endif
