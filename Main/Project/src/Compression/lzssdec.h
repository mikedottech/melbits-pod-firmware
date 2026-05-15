
#ifndef _LZSSDEC_H_
#define _LZSSDEC_H_

#include <inttypes.h>

#define LZSS_E1 (8)
#define LZSS_N (1 << LZSS_E1)
#define LZSS_BUFSIZE (LZSS_N << 1)

typedef struct
{
    int32_t ini;
    int32_t cnt1, cnt2;
    int32_t pos1;
    int32_t bufptr;
    uint32_t streamPos;
    uint8_t *buffer;
    int32_t(*getByte)();
    void(*seek)(uint16_t offset);
} LZSSDecoderState_t;

void LZSSDecodeInit(LZSSDecoderState_t *state);
int16_t LZSSDecodeStep(LZSSDecoderState_t *state);

void LZSSDecodeSeekTo(LZSSDecoderState_t *state, uint16_t offset);

#endif