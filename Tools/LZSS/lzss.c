/* LZSS encoder-decoder (Haruhiko Okumura; public domain) */

#include <stdio.h>
#include <stdlib.h>

#define EI  8  /* typically 10..13 */
#define EJ  4  /* typically 4..5 */
#define P   1  /* If match length <= P then output one character */
#define N (1 << EI)  /* buffer size */
#define F ((1 << EJ) + 1)  /* lookahead buffer size */

int bit_buffer = 0, bit_mask = 128;
unsigned long codecount = 0, textcount = 0;
unsigned char buffer[N * 2];

FILE *infile, *outfile;

void error(void)
{
    printf("Output error\n");  exit(1);
}

void putbit1(void)
{
    bit_buffer |= bit_mask;
    if ((bit_mask >>= 1) == 0) {
        if (fputc(bit_buffer, outfile) == EOF) error();
        bit_buffer = 0;  bit_mask = 128;  codecount++;
    }
}

void putbit0(void)
{
    if ((bit_mask >>= 1) == 0) {
        if (fputc(bit_buffer, outfile) == EOF) error();
        bit_buffer = 0;  bit_mask = 128;  codecount++;
    }
}

void putbyte(int b)
{
	int i = 0;
	int mask = 0x80;
	for (i = 0; i < 8; ++i)
	{
		if (b & mask)
			putbit1();
		else
			putbit0();
		mask >>= 1;
	}
}

void flush_bit_buffer(void)
{
    if (bit_mask != 128) {
        if (fputc(bit_buffer, outfile) == EOF) error();
        codecount++;
    }
}

void output1(int c)
{
    int mask;
    
    putbit1();
    mask = 256;
    while (mask >>= 1) {
        if (c & mask) putbit1();
        else putbit0();
    }
}

void output2(int x, int y)
{
    int mask;
    
    putbit0();
    mask = N;
    while (mask >>= 1) {
        if (x & mask) putbit1();
        else putbit0();
    }
    mask = (1 << EJ);
    while (mask >>= 1) {
        if (y & mask) putbit1();
        else putbit0();
    }
}

void encode(int hdr)
{
    int i, j, f1, x, y, r, s, bufferend, c;

	if (hdr)
	{
		putbyte(0xA9);
		putbyte(0xA9);
		putbyte(0xA9);
		putbyte(0xA9);
	}
    
    for (i = 0; i < N - F; i++) buffer[i] = ' ';
    for (i = N - F; i < N * 2; i++) {
        if ((c = fgetc(infile)) == EOF) break;
        buffer[i] = c;  textcount++;
    }
    bufferend = i;  r = N - F;  s = 0;
    while (r < bufferend) {
        f1 = (F <= bufferend - r) ? F : bufferend - r;
        x = 0;  y = 1;  c = buffer[r];
        for (i = r - 1; i >= s; i--)
            if (buffer[i] == c) {
                for (j = 1; j < f1; j++)
                    if (buffer[i + j] != buffer[r + j]) break;
                if (j > y) {
                    x = i;  y = j;
                }
            }
        if (y <= P) {  y = 1;  output1(c);  }
        else output2(x & (N - 1), y - 2);
        r += y;  s += y;
        if (r >= N * 2 - F) {
            for (i = 0; i < N; i++) buffer[i] = buffer[i + N];
            bufferend -= N;  r -= N;  s -= N;
            while (bufferend < N * 2) {
                if ((c = fgetc(infile)) == EOF) break;
                buffer[bufferend++] = c;  textcount++;
            }
        }
    }
    flush_bit_buffer();
    printf("text:  %ld bytes\n", textcount);
    printf("code:  %ld bytes (%ld%%)\n",
        codecount, (codecount * 100) / textcount);
}

int getbit(int n) /* get n bits */
{
    int i, x;
    static int buf, mask = 0;
    
    x = 0;
    for (i = 0; i < n; i++) {
        if (mask == 0) {
            if ((buf = fgetc(infile)) == EOF) return EOF;
            mask = 128;
        }
        x <<= 1;
        if (buf & mask) x++;
        mask >>= 1;
    }
    return x;
}

int getbitfunc(int n,int (*getByte)()) /* get n bits */
{
	int i, x;
	static int buf, mask = 0;

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


void decodefull(void)
{
    int i, j, k, r, c;
    
    for (i = 0; i < N - F; i++) buffer[i] = ' ';
    r = N - F;
    while ((c = getbit(1)) != EOF) {
        if (c) {
            if ((c = getbit(8)) == EOF) break;
            fputc(c, outfile);
            buffer[r++] = c;  r &= (N - 1);
        } else {
            if ((i = getbit(EI)) == EOF) break;
            if ((j = getbit(EJ)) == EOF) break;
            for (k = 0; k <= j + 1; k++) {
                c = buffer[(i + k) & (N - 1)];
                fputc(c, outfile);
                buffer[r++] = c;  r &= (N - 1);
            }
        }
    }
}

struct _DecodeState
{
	int ini;
	int cnt1, cnt2;
	int pos1;
	int bufptr;
	unsigned char *buffer;
	int(*getByte)();
};

void decodeinit(struct _DecodeState *state)
{
	int i;
	state->ini = 1;
	state->bufptr = N - F;

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

int gb()
{
	return fgetc(infile);
}

void decode()
{
	int c;
	struct _DecodeState state;

	state.buffer = buffer;
	state.getByte = gb;
	decodeinit(&state);

	while ((c = decodestep(&state)) != EOF)
	{
		fputc(c, outfile);
	}

}

unsigned char decrypted[256];

void dectest()
{
	int i;
	for (i = 0; i < 256; i++)
	{
		decrypted[i] = ((i & 0x01) << 7) | ((i & 0x02) << 5) | ((i & 0x04) << 3) | ((i & 0x08) << 1) | ((i & 0x10) >> 1) | ((i & 0x20) >> 3) | ((i & 0x40) >> 5) | ((i & 0x80) >> 7);
		if ((i & 0xf) == 0)
			printf("\n");
		printf("0x%02X,", decrypted[i]);
	}
}


int main(int argc, char *argv[])
{
    int enc;
	int hdr;
    char *s;
	
    if (argc != 4) {
        printf("Usage: lzss e/d infile outfile\n\te = encode\td = decode\n");
        return 1;
    }
    s = argv[1];
	if (s[1] == 0 && (*s == 'd' || *s == 'D' || *s == 'e' || *s == 'E' || *s == 'x'))
	{
		enc = (*s == 'e' || *s == 'E' || *s == 'x');
		hdr = (*s == 'x');
	}
    else {
        printf("? %s\n", s);  return 1;
    }
    if ((infile  = fopen(argv[2], "rb")) == NULL) {
        printf("? %s\n", argv[2]);  return 1;
    }
    if ((outfile = fopen(argv[3], "wb")) == NULL) {
        printf("? %s\n", argv[3]);  return 1;
    }
	size_t original, compressed;
	
    if (enc) encode(hdr);  else decode();
    fclose(infile);  fclose(outfile);
    return 0;
}