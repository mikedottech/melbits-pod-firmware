#include "lzssdec.h"

#define EI 11  /* typically 10..13 */
#define EJ  4  /* typically 4..5 */
#define P   1  /* If match length <= P then output one character */
#define N (1 << EI)  /* buffer size */
#define F ((1 << EJ) + 1)  /* lookahead buffer size */
#define EOF 	(-1)

static int buf, mask = 0;

int getbitfunc(int n,int (*getByte)()) /* get n bits */
{
	int i, x;
	

	x = 0;
	for (i = 0; i < n; i++) {
		if (mask == 0) {
			if ((buf = getByte()) == EOF) return EOF;
			mask = 128;
		}
		x <<= 1;
		if (buf & mask) x++;
		mask >>= 1;
	}
	return x;
}


void decodeinit(struct _DecodeState *state)
{
	int i;
	state->ini = 1;
	state->bufptr = N - F;
	
	buf = 0;
	mask = 0;

	for (i = 0; i < N - F; i++) state->buffer[i] = ' ';
}

int decodestep(struct _DecodeState *state)
{
	int c = -1;


	if (state->ini == 0)
	{
		if (state->pos1 <= state->cnt2 + 1)
		{
			c = state->buffer[(state->cnt1 + state->pos1) & (N - 1)];
			state->buffer[state->bufptr++] = c;  state->bufptr &= (N - 1);
			++state->pos1;
		}

		//if (pos1 > cnt2 + 1)
		//	ini = 1;

		if (c != -1)
			return c;
		else
			state->ini = 1;
	}

	c = getbitfunc(1,state->getByte);
	if (c == EOF)
		return EOF;


	if (c)
	{
		if ((c = getbitfunc(8, state->getByte)) == EOF) return EOF;
		state->buffer[state->bufptr++] = c;  state->bufptr &= (N - 1);
		state->ini = 1;
		return c;
	}
	else
	{
		if (state->ini)
		{
			if ((state->cnt1 = getbitfunc(EI, state->getByte)) == EOF) return EOF;
			if ((state->cnt2 = getbitfunc(EJ, state->getByte)) == EOF) return EOF;

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
}
/*
int gb()
{
	return fgetc(infile);
}

void decode()
{
	struct _DecodeState state;

	state.buffer = buffer;
	state.getByte = gb;
	decodeinit(&state);

	int c;

	while ((c = decodestep(&state)) != EOF)
	{
		fputc(c, outfile);
	}

}


*/
