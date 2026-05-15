/*
 * fix_fft - header declarations for the fixed-point in-place FFT
 * routines.
 *
 * The implementation in fix_fft.c is in the **public domain**.
 *   Written by:     Tom Roberts                    11/8/89
 *   Made portable:  Malcolm Slaney                 12/15/94
 *   Enhanced:       Dimitrios P. Bouras            14 Jun 2006
 *
 * See fix_fft.c for the canonical attribution block.
 */

int fix_fft(short fr[], short fi[], short m, short inverse);
int fix_fftr(short f[], int m, int inverse);
