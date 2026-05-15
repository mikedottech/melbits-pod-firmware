#include "lzssdec.h"

#define EI  (LZSS_E1)                // typically 10..13
#define EJ  (4)                 // typically 4..5
#define P   (1)                 // If match length <= P then output one character
#define N   (LZSS_N)           // buffer size
#define F   ((1 << EJ) + 1)     // lookahead buffer size
#define EOF (-1)                // EOF code

static int32_t buf, mask = 0;

static int32_t getbitfunc(int32_t n,int32_t (*getByte)()) /* get n bits */
{
    int32_t i, x;

    x = 0;
    for (i = 0; i < n; i++)
    {
        if (mask == 0)
        {
            if ((buf = getByte()) == EOF) return EOF;
            mask = 128;
        }
        x <<= 1;
        if (buf & mask) x++;
        mask >>= 1;
    }
    return x;
}

void LZSSDecodeInit(LZSSDecoderState_t *state)
{
    int32_t i;

    state->ini = 1;
    state->bufptr = N - F;
    state->streamPos = 0;

    buf = 0;
    mask = 0;

    for (i = 0; i < N - F; i++) state->buffer[i] = ' ';

    state->seek(0);
}

int16_t LZSSDecodeStep(LZSSDecoderState_t *state)
{
    int32_t c = -1;
    
    ++state->streamPos;

    if (state->ini == 0)
    {
        if (state->pos1 <= state->cnt2 + 1)
        {
            c = state->buffer[(state->cnt1 + state->pos1) & (N - 1)];
            state->buffer[state->bufptr++] = c;  state->bufptr &= (N - 1);
            ++state->pos1;
        }

        if (c != -1)
            return c;
        else
            state->ini = 1;
    }

    c = getbitfunc(1,state->getByte);

    if (c == EOF) goto eof;

    if (c)
    {
        if ((c = getbitfunc(8, state->getByte)) == EOF) goto eof;
        state->buffer[state->bufptr++] = c;  state->bufptr &= (N - 1);
        state->ini = 1;
        return c;
    } else {
        if (state->ini)
        {
            if ((state->cnt1 = getbitfunc(EI, state->getByte)) == EOF) goto eof;
            if ((state->cnt2 = getbitfunc(EJ, state->getByte)) == EOF) goto eof;

            state->ini = 0;
            state->pos1 = 0;
        }
        if (state->pos1 <= state->cnt2 + 1)
        {
            c = state->buffer[(state->cnt1 + state->pos1) & (N - 1)];
            state->buffer[state->bufptr++] = c;  state->bufptr &= (N - 1);
            ++state->pos1;
        }

        if (state->pos1 > state->cnt2 + 1)
            state->ini = 1;

        return c;
    }

eof:
    --state->streamPos;
    return EOF;
}

// Slow as hell but not really used
void LZSSDecodeSeekTo(LZSSDecoderState_t *state, uint16_t offset)
{
    if(state->streamPos == offset) return;

    int16_t b = 0;
    
    // Reset decoder and walk from the beginning
    LZSSDecodeInit(state);
    do
    {
        b = LZSSDecodeStep(state);
    } while(b != EOF && state->streamPos < offset);
}

/*
int32_t gb()
{
	return fgetc(infile);
}

void decode()
{
	LZSSDecoderState_t state;

	state.buffer = buffer;
	state.getByte = gb;
	decodeinit(&state);

	int32_t c;

	while ((c = decodestep(&state)) != EOF)
	{
		fputc(c, outfile);
	}

}


*/
