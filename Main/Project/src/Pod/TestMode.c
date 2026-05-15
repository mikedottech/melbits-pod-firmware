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

#include "TestMode.h"

#define COMPILE_TESTS (defined(FACTORY) | defined(DEBUG))

#if defined(COMPILE_TESTS) && defined(__nRF_FAMILY)

#include <nrf_error.h>
#include <nrf_soc.h>
#include <nrf_sdh_soc.h>
#include <nrf_sdm.h>
#include <app_error.h>
#endif

#include "Pod/UI.h"
#include "Pod/Activity.h"
#include "HAL/Debug.h"
#include "HAL/HAL.h"
#include "Pod/FileServer.h"
#include "Pod/StreamServer.h"

//#include "Pod/Misc/fix_fft.h"


/*
#include <arm_math.h>
arm_rfft_instance_q15 _TEST_;
static q15_t __TESTFFTV__[64];
//const arm_cfft_instance_f32 f = {32, twiddleCoef_32, armBitRevIndexTable32, ARMBITREVINDEXTABLE__32_TABLE_LENGTH};
*/

#ifdef DEBUG
static volatile int x = 0;
static volatile uint8_t go = 0;
static volatile uint8_t testClip = 0xFF;
volatile int _go = 0;
#endif


#if COMPILE_TESTS
//int xx = 0;
#endif

/// MERDER -----------------------------------------------------

#if 0
/*
Application Overview
The example application will apply a 16 point FFT to an incoming signal using the 9S12C Microcontroller. There is an external analog LPF that bandlimits the signal to 20kHz. The audio is sampled at 44100 Hz, the typical audio sampling frequency. Each number is quantized to a signed 8 bit character. The The "twiddle factors" Wk16 are precalculated and stored in memory as #define statements. This allows the compiler to optimize math operations for the microcontroller. Because of the symmetric nature of the twiddle factors, 8 numbers are required (rather than all 16). The display needs to update at least every 1/60th of a second. This allows for persistance of vision to fill in the gaps between samples on the display. The 9S12C is operating at a clock speed of 24MHz, so in 1/60th of a second, the processor can use 400,000 clock cycles per calculation (ideally). There is overhead code required for the display and other operations, so it is important to avoid pushing the limit. Additionally, since the 9S12C is an 8 bit microcontroller, all 16 bit operations cannot be performed only by the ALU and the compiler must create assembly code that performs the desired operations. Another aspect of the calculation is finding the magnitudes of the numbers. Performing a square root is very computation intensitive, so the norm squared is often used if the actual magnitude isn't needed. In this application, the square root was calculated iteratively.

C Code
The C code for the calculation is included below. Unlike with MATLAB or C on a computer, recursion cannot be used on a microcontroller because of the risks of stack overflow. This required iterating through the FFT algorithm and writing each stage independently. The fft function computes the fft of a 16 point array and the findRoot4 function finds the magnitude of the complex number passed into it.
*/

/* FFT CONSTANTS*/
#define SIN2PI 49//sin(2*pi/16) * 2^7
#define SIN4PI 90
#define SIN6PI 118
#define COS2PI_P_SIN2PI 167//(cos(2*pi/16) + sin(2*pi/16)) * 2^7
#define COS2PI_M_SIN2PI 56//cos(2*pi/16) - sin(2*pi/16)) * 2^7
#define COS6PI_P_SIN6PI 167
#define COS6PI_M_SIN6PI -56
#define OneTwentyEight 128

int findRoot4(int x, int y);

void fft(int inarr[16],int mags[8] ) {
  int atemp, temp, temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8;
  int temp9,temp10,temp11,temp12,temp13,temp14,temp15;
  int outarr[16]; temp0=inarr[0]+inarr[8];
  temp1=inarr[1]+inarr[9];
  temp2=inarr[2]+inarr[10];
  temp3=inarr[3]+inarr[11];
  temp4=inarr[4]+inarr[12];
  temp5=inarr[5]+inarr[13];
  temp6=inarr[6]+inarr[14];
  temp7=inarr[7]+inarr[15];
  temp8=inarr[0]-inarr[8];
  temp9=inarr[1]-inarr[9];
  temp10=inarr[2]-inarr[10];
  temp11=inarr[3]-inarr[11];
  temp12=inarr[12]-inarr[4];
  temp13=inarr[13]-inarr[5];
  temp14=inarr[14]-inarr[6];
  temp15=inarr[15]-inarr[7]; temp =(temp13-temp9)*(SIN2PI)/OneTwentyEight;
  temp9=temp9*(COS2PI_P_SIN2PI)/OneTwentyEight+temp;
  temp13=temp13*(COS2PI_M_SIN2PI)/OneTwentyEight+temp; temp10 = temp10 *   (SIN4PI)/OneTwentyEight;
   temp14 = temp14 * (SIN4PI)/OneTwentyEight;
  atemp = temp14;
  temp14=temp14-temp10;
  temp10+=atemp; temp = (temp15-temp11)*(SIN6PI)/OneTwentyEight;
  temp15=temp15*(COS6PI_P_SIN6PI)/OneTwentyEight+temp;
  temp11=temp11*(COS6PI_M_SIN6PI)/OneTwentyEight+temp;

  atemp = temp8;
  temp8+=temp10;
  temp10=atemp-temp10;
  atemp = temp9;
  temp9+=temp11;
  temp11=atemp-temp11;
  atemp = temp12;
  temp12+=temp14;
  temp14=atemp-temp14;
  atemp = temp13;
  temp13+=temp15;
  temp15=atemp-temp15; outarr[1]=temp8+temp9;
  outarr[3]=temp10-temp15;
  outarr[5]=temp10+temp15;
  outarr[7]=temp8-temp9;
  outarr[9]=temp12+temp13;
  outarr[11]=-temp14-temp11;
  outarr[13]=temp14-temp11;
  outarr[15]=temp13-temp12;
  atemp = temp0;
  temp0=temp0+temp4;
  temp4=atemp-temp4;
  atemp = temp1;
  temp1=temp1+temp5;
  temp5=atemp-temp5;
  atemp = temp2;
  temp2+=temp6;
  temp6=atemp-temp6;
  atemp = temp3;
  temp3+=temp7;
  temp7=atemp-temp7; outarr[0]=temp0+temp2;
  outarr[4]=temp0-temp2;
  temp1+=temp3;
  outarr[12]=(temp3<<1)-temp1;
  outarr[0]+=temp1;
  outarr[8]=outarr[0]-temp1-temp1; atemp = temp5 * (SIN4PI)/ OneTwentyEight;
  temp7 = temp7 * (SIN4PI)/ OneTwentyEight;
  temp5=atemp-temp7;
  temp7+=atemp; outarr[14]=temp6-temp7;
  outarr[2]=temp5+temp4;
  outarr[6]=temp4-temp5;
  outarr[10]=-temp7-temp6; //Magnitude calculations
  if(outarr[8] > 0) {mags[0] = outarr[8];}
  else {mags[0] = -outarr[8];}

  mags[1] = findRoot4(outarr[7], outarr[15]);
  mags[2] = findRoot4(outarr[6], outarr[14]);
  mags[3] = findRoot4(outarr[5], outarr[13]);
  mags[4] = findRoot4(outarr[4], outarr[12]);
  mags[5] = findRoot4(outarr[3], outarr[11]);
  mags[6] = findRoot4(outarr[2], outarr[10]);
  mags[7] = findRoot4(outarr[1], outarr[9]);
}

int findRoot4(int x, int y)
{
  if(x < 0)x = -x;
  if(y < 0)y = -y;
  if (x > y) {
    if (x > 2180) return ((15 * (long)x) / 16) + ((15 * (long)y) / 32); //2180 is about 2^15 / 15
    return ((15 * x) / 16) + ((15 * y) / 32);
  } else{
     if (y > 2180) return (int)(((15 * (long)y) / 16) + ((15 * (long)x) / 32));
     return ((15 * y) / 16) + ((15 * x) / 32);
  }
}
#endif

//----------
// The inline fix_fft / fix_fftr implementation that follows is the
// 8-bit (`char`) variant of the **public domain** fixed-point FFT:
//   Written by:     Tom Roberts          11/8/89
//   Made portable:  Malcolm Slaney       12/15/94
//   Enhanced:       Dimitrios P. Bouras  14 Jun 2006
// See Main/Project/src/Pod/Misc/fix_fft.c for the canonical 16-bit
// variant and the full attribution block.
#if 0

/*
 fix_fft() - perform forward/inverse fast Fourier transform.
 fr[n],fi[n] are real and imaginary arrays, both INPUT AND
 RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
 0 for forward transform (FFT), or 1 for iFFT.
*/
int fix_fft(char fr[], char fi[], int m, int inverse);



/*
 fix_fftr() - forward/inverse FFT on array of real numbers.
 Real FFT/iFFT using half-size complex FFT by distributing
 even/odd samples into real/imaginary arrays respectively.
 In order to save data space (i.e. to avoid two arrays, one
 for real, one for imaginary samples), we proceed in the
 following two steps: a) samples are rearranged in the real
 array so that all even samples are in places 0-(N/2-1) and
 all imaginary samples in places (N/2)-(N-1), and b) fix_fft
 is called with fr and fi pointing to index 0 and index N/2
 respectively in the original array. The above guarantees
 that fix_fft "sees" consecutive real samples as alternating
 real and imaginary samples in the complex array.
*/
int fix_fftr(char f[], int m, int inverse);


//----------

/* fix_fft.c - Fixed-point in-place Fast Fourier Transform  */
/*
 All data are fixed-point short integers, in which -32768
 to +32768 represent -1.0 to +1.0 respectively. Integer
 arithmetic is used for speed, instead of the more natural
 floating-point.

 For the forward FFT (time -> freq), fixed scaling is
 performed to prevent arithmetic overflow, and to map a 0dB
 sine/cosine wave (i.e. amplitude = 32767) to two -6dB freq
 coefficients. The return value is always 0.

 For the inverse FFT (freq -> time), fixed scaling cannot be
 done, as two 0dB coefficients would sum to a peak amplitude
 of 64K, overflowing the 32k range of the fixed-point integers.
 Thus, the fix_fft() routine performs variable scaling, and
 returns a value which is the number of bits LEFT by which
 the output must be shifted to get the actual amplitude
 (i.e. if fix_fft() returns 3, each value of fr[] and fi[]
 must be multiplied by 8 (2**3) for proper scaling.
 Clearly, this cannot be done within fixed-point short
 integers. In practice, if the result is to be used as a
 filter, the scale_shift can usually be ignored, as the
 result will be approximately correctly normalized as is.

 Written by:  Tom Roberts  11/8/89
 Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
 Enhanced:  Dimitrios P. Bouras  14 Jun 2006 dbouras@ieee.org
 Modified for 8bit values David Keller  10.10.2010
*/

#define N_WAVE      256    /* full length of Sinewave[] */
#define LOG2_N_WAVE 8      /* log2(N_WAVE) */


/*
 Since we only use 3/4 of N_WAVE, we define only
 this many samples, in order to conserve data space.
*/


const int8_t Sinewave[N_WAVE-N_WAVE/4] = {
0, 3, 6, 9, 12, 15, 18, 21,
24, 28, 31, 34, 37, 40, 43, 46,
48, 51, 54, 57, 60, 63, 65, 68,
71, 73, 76, 78, 81, 83, 85, 88,
90, 92, 94, 96, 98, 100, 102, 104,
106, 108, 109, 111, 112, 114, 115, 117,
118, 119, 120, 121, 122, 123, 124, 124,
125, 126, 126, 127, 127, 127, 127, 127,

127, 127, 127, 127, 127, 127, 126, 126,
125, 124, 124, 123, 122, 121, 120, 119,
118, 117, 115, 114, 112, 111, 109, 108,
106, 104, 102, 100, 98, 96, 94, 92,
90, 88, 85, 83, 81, 78, 76, 73,
71, 68, 65, 63, 60, 57, 54, 51,
48, 46, 43, 40, 37, 34, 31, 28,
24, 21, 18, 15, 12, 9, 6, 3,

0, -3, -6, -9, -12, -15, -18, -21,
-24, -28, -31, -34, -37, -40, -43, -46,
-48, -51, -54, -57, -60, -63, -65, -68,
-71, -73, -76, -78, -81, -83, -85, -88,
-90, -92, -94, -96, -98, -100, -102, -104,
-106, -108, -109, -111, -112, -114, -115, -117,
-118, -119, -120, -121, -122, -123, -124, -124,
-125, -126, -126, -127, -127, -127, -127, -127,

/*-127, -127, -127, -127, -127, -127, -126, -126,
-125, -124, -124, -123, -122, -121, -120, -119,
-118, -117, -115, -114, -112, -111, -109, -108,
-106, -104, -102, -100, -98, -96, -94, -92,
-90, -88, -85, -83, -81, -78, -76, -73,
-71, -68, -65, -63, -60, -57, -54, -51,
-48, -46, -43, -40, -37, -34, -31, -28,
-24, -21, -18, -15, -12, -9, -6, -3, */
};


#define pgm_read_word_near(X) (*(X))

/*
 FIX_MPY() - fixed-point multiplication & scaling.
 Substitute inline assembly for hardware-specific
 optimization suited to a particluar DSP processor.
 Scaling ensures that result remains 16-bit.
*/
inline char FIX_MPY(char a, char b)
{
 
 //Serial.println(a);
//Serial.println(b);
 
 
   /* shift right one less bit (i.e. 15-1) */
   int c = ((int)a * (int)b) >> 6;
   /* last bit shifted out = rounding-bit */
   b = c & 0x01;
   /* last shift + rounding bit */
   a = (c >> 1) + b;

       /*
       Serial.println(Sinewave[3]);
       Serial.println(c);
       Serial.println(a);
       while(1);*/

   return a;
}

/*
 fix_fft() - perform forward/inverse fast Fourier transform.
 fr[n],fi[n] are real and imaginary arrays, both INPUT AND
 RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
 0 for forward transform (FFT), or 1 for iFFT.
*/
int fix_fft(char fr[], char fi[], int m, int inverse)
{
   int mr, nn, i, j, l, k, istep, n, scale, shift;
   char qr, qi, tr, ti, wr, wi;

   n = 1 << m;

   /* max FFT size = N_WAVE */
   if (n > N_WAVE)
       return -1;

   mr = 0;
   nn = n - 1;
   scale = 0;

   /* decimation in time - re-order data */
   for (m=1; m<=nn; ++m) {
       l = n;
       do {
           l >>= 1;
       } while (mr+l > nn);
       mr = (mr & (l-1)) + l;

       if (mr <= m)
           continue;
       tr = fr[m];
       fr[m] = fr[mr];
       fr[mr] = tr;
       ti = fi[m];
       fi[m] = fi[mr];
       fi[mr] = ti;
   }

   l = 1;
   k = LOG2_N_WAVE-1;
   while (l < n) {
       if (inverse) {
           /* variable scaling, depending upon data */
           shift = 0;
           for (i=0; i<n; ++i) {
               j = fr[i];
               if (j < 0)
                   j = -j;
               m = fi[i];
               if (m < 0)
                   m = -m;
               if (j > 16383 || m > 16383) {
                   shift = 1;
                   break;
               }
           }
           if (shift)
               ++scale;
       } else {
           /*
             fixed scaling, for proper normalization --
             there will be log2(n) passes, so this results
             in an overall factor of 1/n, distributed to
             maximize arithmetic accuracy.
           */
           shift = 1;
       }
       /*
         it may not be obvious, but the shift will be
         performed on each data point exactly once,
         during this pass.
       */
       istep = l << 1;
       for (m=0; m<l; ++m) {
           j = m << k;
           /* 0 <= j < N_WAVE/2 */
           wr =  pgm_read_word_near(Sinewave + j+N_WAVE/4);

/*Serial.println("asdfasdf");
Serial.println(wr);
Serial.println(j+N_WAVE/4);
Serial.println(Sinewave[256]);

Serial.println("");*/


           wi = -pgm_read_word_near(Sinewave + j);
           if (inverse)
               wi = -wi;
           if (shift) {
               wr >>= 1;
               wi >>= 1;
           }
           for (i=m; i<n; i+=istep) {
               j = i + l;
               tr = FIX_MPY(wr,fr[j]) - FIX_MPY(wi,fi[j]);
               ti = FIX_MPY(wr,fi[j]) + FIX_MPY(wi,fr[j]);
               qr = fr[i];
               qi = fi[i];
               if (shift) {
                   qr >>= 1;
                   qi >>= 1;
               }
               fr[j] = qr - tr;
               fi[j] = qi - ti;
               fr[i] = qr + tr;
               fi[i] = qi + ti;
           }
       }
       --k;
       l = istep;
   }
   return scale;
}


/*
 fix_fftr() - forward/inverse FFT on array of real numbers.
 Real FFT/iFFT using half-size complex FFT by distributing
 even/odd samples into real/imaginary arrays respectively.
 In order to save data space (i.e. to avoid two arrays, one
 for real, one for imaginary samples), we proceed in the
 following two steps: a) samples are rearranged in the real
 array so that all even samples are in places 0-(N/2-1) and
 all imaginary samples in places (N/2)-(N-1), and b) fix_fft
 is called with fr and fi pointing to index 0 and index N/2
 respectively in the original array. The above guarantees
 that fix_fft "sees" consecutive real samples as alternating
 real and imaginary samples in the complex array.
*/
int fix_fftr(char f[], int m, int inverse)
{
   int i, N = 1<<(m-1), scale = 0;
   char tt, *fr=f, *fi=&f[N];

   if (inverse)
       scale = fix_fft(fi, fr, m-1, inverse);
   for (i=1; i<N; i+=2) {
       tt = f[N+i-1];
       f[N+i-1] = f[i];
       f[i] = tt;
   }
   if (! inverse)
       scale = fix_fft(fi, fr, m-1, inverse);
   return scale;
}

#endif

/*
char im[128];
char data[128];

void loop(){
 int static i = 0;
 static long tt;
 int val;
 
  if (millis() > tt){
     if (i < 128){
       val = analogRead(pin_adc);
       data[i] = val / 4 - 128;
       im[i] = 0;
       i++;  
       
     }
     else{
       //this could be done with the fix_fftr function without the im array.
       fix_fft(data,im,7,0);
       // I am only interessted in the absolute value of the transformation
       for (i=0; i< 64;i++){
          data[i] = sqrt(data[i] * data[i] + im[i] * im[i]);
       }
       
       //do something with the data values 1..64 and ignore im
       show_big_bars(data,0);
     }
   
   tt = millis();
  }
}
*/
/// ------------------------------------------------------------


//short im[128];
//short data[128];
//extern short Sinewave[];

void Pod_TMUnitTestsInit(void)
{
/*
    memcpy(&data[0], &Sinewave[0], sizeof(data));
    memset(&im[0], 0, sizeof(im));
    
    fix_fft(data,im,7,0);
    
    for (uint8_t i=0; i< 64;i++){
        data[i] = Utils_getApproximatedDistance2D(data[i] * data[i], im[i] * im[i]);
    }
*/

    //int inarr[16];
    //int mags[8];
    //fft(&inarr[0], &mags[0]);

        //arm_rfft_init_q15 (&_TEST_, 64, 0, 0);
  /*      
        _TEST_.fftLenReal = 64;
        //_TEST_.pTwiddle = &twiddleCoef_64_q15[0];
        _TEST_.ifftFlagR = 0;
        _TEST_.twidCoefRModifier = 128u;
        _TEST_.pCfft = &arm_cfft_sR_q15_len32;        
        _TEST_.pTwiddleAReal = (q15_t *) realCoefAQ15;
        _TEST_.pTwiddleBReal = (q15_t *) realCoefBQ15; 

        arm_rfft_q15(&_TEST_, &__TESTFFTV__[0], &__TESTFFTV__[0]);
*/
        //Pod_analogSetClientMode(0, ACM_IDLE);
        //Pod_analogSetClientMode(0, ACM_STREAM_0_5X);
        //Pod_analogSetClientMode(0, ACM_STREAM_0_125X);
        //Pod_analogSetClientMode(0, ACM_SINGLESHOT);

//    constPod_storageFD_t *fd = Pod_storageGetFD(POD_STORAGE_FID_FB_PCM_INSERT_SEED);
//
//    if(fd)
//    {
//        const uint8_t *ptr = Pod_storageGetPtr(fd);
//        MBT_LOG("PTR = 0x%x\n", ptr);
//    }

    {
        //synth_playSong(&shake_start_success);
    }
/*
    {
        uint8_t token = 42;
        HAL_bleAdvertisingConfig_t cfg =
        {
          .initialAdvertisingData = {
            .pData = &token,
            .len = 1
          }
        };

        HAL_bleStartAdvertising(&cfg);
    }
*/
    // TODO: Test lp-pwm on AVDD
    //HAL_gpioWriteDigital(MBT_CFG_PIN_AVDD, 0);
    //HAL_avddSetLevel(0);

//    {
        //Pod_vibSetVibration(40, 0);
//    }
    
//    {
//        const Utils_RGB888_t colA = {255,0,0};
//        const Utils_RGB888_t colB = {0,255,0};
//
//        const Utils_RGB888_t colIdle = {0,0,255};
//        Pod_LEDsCenterSetIdleColor(&colIdle);
//
//        Pod_LEDsCenterSetAnimation(&colA, &colB, 32, 500);
//        Pod_LEDsSetLevel(POD_LED_Q3, 128, 5000);
//    }

//    {
        //Pod_motionSetSubStreamEnabled(true);
//    }

    //sprintf(s_testRetain.str, "Holi");    // Adds 1 KB of code!

    /*
    Pod_UIPlaySystemClip(SC_SEND_SEED);

    {
        HAL_flashErasePage(0);
        static const char test[] = {0xAA, 0xBB, 0xCC, 0xDD};
        for(uint16_t j = 1; j < 100; j += 4)
        {
            HAL_flashWrite(&test[0], j, sizeof(test));
        }
        //HAL_flashWrite(&test[0], 5, sizeof(test));
    }*/

    /*
    {
        static const char test[] = {0xAA, 0xBB, 0xCC, 0xDD};
        HAL_flashWrite(test, 1024*3, sizeof(test));
    }*/
    

//    const Utils_RGB888_t kGreen = {0, 255, 0};
//    Pod_UISetUserSlotColor(&kGreen);
    //Pod_ActStart(ACT_EVOLUTION);

    //synth_playPCM(0x0080);
    //Pod_UIPlayFilePrio(0x0080, 1);
    // User settings
    //synth_selectGlobalVolume(SYNTH_MIXER_VOL_25);
    //HAL_sysEnterDFUMode();

    //Pod_SSSetStreamMask(SS_MSK_ITEM_ACCEL | SS_MSK_ITEM_LIGHT | SS_MSK_ITEM_TEMP);
/*
static uint8_t testToken = 0x80;
                    HAL_bleAdvertisingConfig_t cfg =
                {
                    .initialAdvertisingData = 
                    {
                        .pData = &testToken,
                        .len = 1
//                        .pData = (const char *)&pkt,
//                        .len = sizeof(pkt)
                    }
                };

                HAL_bleStartAdvertising(&cfg); */
/*
    if(_go)
    {
        _go = 0;
        static const uint8_t data[] = {0x1a, 0x3c, 0x0, 0xff, 0x93, 0xc6, 0x80, 0x0, 0x4, 0x4, 0x10, 0x4, 0xad, 0x0, 0x0, 0x0};
        Pod_storageWrite(0, &data[0], sizeof(data));
    }
    */
}

void Pod_TMUnitTestsTick(uint32_t ticks)
{
    {
/*
        uint64_t us = MBT_TICKS_TO_US(diff);
        uint32_t ms = MBT_TICKS_TO_US(diff) / 1000;

        static uint32_t accum = 0;
        accum += diff;

        if(accum >= MBT_US_TO_TICKS(1000000))
        {
            //MBT_LOG("SEC!\n");
            accum -= MBT_US_TO_TICKS(1000000);

            if(HAL_bleGetConnState() == HAL_BLE_CONN_STATE_CONNECTED)
            {
                uint8_t dat = 0;
                for(uint32_t i = 0; i < 256; ++i)
                {
                    uint16_t len = 1;
                    uint32_t e = HAL_bleEnqueueSendNotification(&dat, &len, true);
                    if(e != 0)
                    {
                        MBT_LOG("> ENQUEUE ERR: %d\n", e);
                    } else {
                        if(len < 1)
                        {
                            MBT_LOG("> OUT OF SPACE!\n");
                        } else {
                            MBT_LOG("Enqueued\n");
                        }
                    }
                    ++dat;
                }
            }
            
            static uint8_t token = 42;
            token++;
            HAL_smallBuffer_t bft = {
                .pData = &token,
                .len = 1
            };
            uint32_t err_code = 0;
            err_code = HAL_bleUpdateAdvertisingData(&bft);
            MBT_LOG("ADV UPDATE-> 0x%x\n", err_code);
        }
*/
//        static uint8_t leds[7] = {0};
//        for(uint8_t i = 0; i < 7; ++i)
//        {
//            leds[i] = accum >> 6;
//        }  
//        HAL_LEDSetMultiple(leds);

//        if(accum >= MBT_US_TO_TICKS(1000000))
//        {
//            //MBT_LOG("SEC!\n");
//            accum -= MBT_US_TO_TICKS(1000000);
//            static bool tgl = false;
//            HAL_analogSetEnabled(tgl);
//            tgl = !tgl;
//        }


//        if(accum >= MBT_US_TO_TICKS(1000000))
//        {
//            //MBT_LOG("SEC!\n");
//            accum -= MBT_US_TO_TICKS(1000000);
//            static bool tgl = false;
//            if(tgl)
//            {
//                Pod_analogSetClientMode(1, ACM_STREAM_1X);
//            } else {
//                Pod_analogSetClientMode(1, ACM_IDLE);
//            }
//            tgl = !tgl;
//        }

        //if(accum >= MBT_US_TO_TICKS(3000000))
//        {
//            //accum -= MBT_US_TO_TICKS(3000000);
//    
//            {
//                HAL_powerEnterLockupMode();
//            }
//        }


    }

//    if(go)
//    {
//        go = 0;
//        //Pod_LEDsSetLevel(POD_LED_G, (testClip % 2) * 255, 500);
//        if(++testClip == SC_FILE)
//        {
//            MBT_LOG("DONE\n");
//            testClip = 0;
//        } else {
//            Pod_UIPlaySystemClip(testClip);
//        }        
//    }

/*
    if(go != 0)
    {
    
        switch(go)
        {
            case 1:
                Pod_FSErase();
            break;
            case 2:
                Pod_FSStartArchiveXfer(0, 8);
            break;
            case 3:
            {
                static const uint8_t dat[] = {0xAA, 0xBB, 0xCC, 0xDD};
                Pod_FSDataPacketHandler(&dat[0], sizeof(dat));
            }
            break;
            case 4:
            {
                uint8_t *pBase;
                uint16_t len;
                HAL_flashGetStorageAbsoluteAddr(&pBase, &len);
                for(uint8_t i = 0; i < 8; ++i)
                {
                    MBT_LOG("%x ", pBase[i]);
                }
                MBT_LOG("\n");                
            }
            break;
        }
        
        
        //Pod_UIPlaySystemClip(SC_LIGHTSENSORTEST);

        go = 0;
    }
*/
}

#if defined(COMPILE_TESTS) && defined(__nRF_FAMILY)

#include <nrf_error.h>
#include <nrf_soc.h>
#include <nrf_sdh_soc.h>
#include <nrf_sdm.h>

#define RFTEST_SOC_OBSERVER_PRIO 0

static void rftest_on_sys_evt(uint32_t sys_evt, void * p_context);

NRF_SDH_SOC_OBSERVER(rftest_soc_obs,     \
                     RFTEST_SOC_OBSERVER_PRIO, \
                     rftest_on_sys_evt, 0);

#define RFTEST_LEN_US                            (1000UL)
#define TX_LEN_EXTENSION_US                      (1000UL)
#define RFTEST_SAFETY_MARGIN_US                  (500UL)   /**< The timeslot activity should be finished with this much to spare. */
#define RFTEST_EXTEND_MARGIN_US                  (700UL)   /**< The timeslot activity should request an extension this long before end of timeslot. */
#define CHANNEL_INCREMENT                        (2) /** 2 MHz-wide channels in BLE **/

typedef enum
{
    RADIO_STATE_IDLE = 0, /* Default state */
    RADIO_STATE_RX,   /* Waiting for packets */
    RADIO_STATE_TX    /* Trying to transmit packet */
} Pod_TMRadioState_t;;

typedef struct
{
    Pod_TMTestType_t currentTest;
    Pod_TMTestType_t wantedTest;
    volatile bool timeslot_session_open;
    volatile Pod_TMRadioState_t radio_state;
    // Not really needed
    uint32_t          total_timeslot_length;
    uint32_t          timeslot_distance;

    volatile uint8_t         channel;
    volatile uint8_t         channel_end;
    volatile uint8_t         channel_start;
    volatile uint8_t         mode;
    volatile uint8_t         txpower;
    volatile bool            slot_open;
    
    MBT_TIMER_DEFINE(testUpdate_timer);
} Pod_TMTestState_t;

static Pod_TMTestState_t s_Pod_TMTestState;

/**< This will be used when requesting the first timeslot or any time a timeslot is blocked or cancelled. */
static nrf_radio_request_t m_timeslot_req_earliest = {
        NRF_RADIO_REQ_TYPE_EARLIEST,
        .params.earliest = {
            NRF_RADIO_HFCLK_CFG_XTAL_GUARANTEED,
            NRF_RADIO_PRIORITY_NORMAL,
            RFTEST_LEN_US,
            NRF_RADIO_EARLIEST_TIMEOUT_MAX_US
        }};

/**< This will be used at the end of each timeslot to request the next timeslot. */
static nrf_radio_request_t m_timeslot_req_normal = {
        NRF_RADIO_REQ_TYPE_NORMAL,
        .params.normal = {
            NRF_RADIO_HFCLK_CFG_XTAL_GUARANTEED,
            NRF_RADIO_PRIORITY_NORMAL,
            0,
            RFTEST_LEN_US
        }};

/**< This will be used at the end of each timeslot to request the next timeslot. */
static nrf_radio_signal_callback_return_param_t m_rsc_return_sched_next_normal = {
        NRF_RADIO_SIGNAL_CALLBACK_ACTION_REQUEST_AND_END,
        .params.request = {
                (nrf_radio_request_t*) &m_timeslot_req_normal
        }};

/**< This will be used at the end of each timeslot to request the next timeslot. */
static nrf_radio_signal_callback_return_param_t m_rsc_return_sched_next_earliest = {
        NRF_RADIO_SIGNAL_CALLBACK_ACTION_REQUEST_AND_END,
        .params.request = {
                (nrf_radio_request_t*) &m_timeslot_req_earliest
        }};

/**< This will be used at the end of each timeslot to request an extension of the timeslot. */
static nrf_radio_signal_callback_return_param_t m_rsc_extend = {
        NRF_RADIO_SIGNAL_CALLBACK_ACTION_EXTEND,
        .params.extend = {TX_LEN_EXTENSION_US}
        };

/**< This will be used at the end of each timeslot to request the next timeslot. */
static nrf_radio_signal_callback_return_param_t m_rsc_return_no_action = {
        NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE,
        .params.request = {NULL}
        };

static uint32_t rftest_calcChannel(uint8_t bleChannel)
{
    // Base is 2400 MHz
    return (2*bleChannel);
}

void radio_tx_sweep_start(uint8_t txpower,
                          uint8_t mode,
                          uint8_t channel_start,
                          uint8_t channel_end,
                          uint8_t delayms)
{
    s_Pod_TMTestState.txpower       = txpower;
    s_Pod_TMTestState.mode          = mode;
    s_Pod_TMTestState.channel_start = channel_start;
    s_Pod_TMTestState.channel       = channel_start;
    s_Pod_TMTestState.channel_end   = channel_end;
//    timer0_init(delayms);
    //MBT_LOG("[TM] SWEEP START\n");
}

/**@brief Function for disabling radio.
 */
static void radio_disable(void)
{
    NRF_RADIO->SHORTS          = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;
#ifdef NRF51
    NRF_RADIO->TEST = 0;
#endif
    NRF_RADIO->TASKS_DISABLE = 1;

    while (NRF_RADIO->EVENTS_DISABLED == 0)
    {
        // Do nothing.
    }
    NRF_RADIO->EVENTS_DISABLED = 0;
}

/**brief Function for setting the channel for radio.
 *
 * @param[in] mode    Radio mode.
 * @param[in] channel Radio channel.
 */
static void radio_channel_set(uint8_t mode, uint8_t channel)
{
#ifdef NRF52840_XXAA
    if (mode == RADIO_MODE_MODE_Ieee802154_250Kbit)
    {
        if (channel >= IEEE_MIN_CHANNEL && channel <= IEEE_MAX_CHANNEL)
        {
            NRF_RADIO->FREQUENCY = IEEE_FREQ_CALC(channel);
        }

        else
        {
            NRF_RADIO->FREQUENCY = IEEE_DEFAULT_FREQ;
        }

    }
    else
    {
        NRF_RADIO->FREQUENCY = channel;
    }
#else
    NRF_RADIO->FREQUENCY = channel;
#endif // NRF52840_XXAA
}


void radio_unmodulated_tx_carrier(uint8_t txpower, uint8_t mode, uint8_t channel)
{
    radio_disable();
    NRF_RADIO->SHORTS  = RADIO_SHORTS_READY_START_Msk;
    NRF_RADIO->TXPOWER = (txpower << RADIO_TXPOWER_TXPOWER_Pos);
    NRF_RADIO->MODE    = (mode << RADIO_MODE_MODE_Pos);

    radio_channel_set(mode, channel);

#ifdef NRF51
    NRF_RADIO->TEST = (RADIO_TEST_CONST_CARRIER_Enabled << RADIO_TEST_CONST_CARRIER_Pos) \
                      | (RADIO_TEST_PLL_LOCK_Enabled << RADIO_TEST_PLL_LOCK_Pos);
#endif
    NRF_RADIO->TASKS_TXEN = 1;
    s_Pod_TMTestState.radio_state = RADIO_STATE_TX;
}


/**
 * @brief Function for handling the Timer 0 interrupt used for the TX or RX sweep. The carrier is started with the new channel,
 * and the channel is incremented for the next interrupt.
 */
void rftest_irq_sweep_update(bool increment)
{
    uint8_t channel_end   = s_Pod_TMTestState.channel_end;
    uint8_t channel_start = s_Pod_TMTestState.channel_start;
    uint8_t mode          = s_Pod_TMTestState.mode;
    uint8_t txpower       = s_Pod_TMTestState.txpower;

    // COMPARE[0] handles channel sweep
//    if (NRF_TIMER0->EVENTS_COMPARE[0])
//    {
//        NRF_TIMER0->EVENTS_COMPARE[0] = 0;
//        if (m_sweep_tx)
//        {
            radio_unmodulated_tx_carrier(txpower, mode, s_Pod_TMTestState.channel);
//        }
//        else if (m_sweep_rx)
//        {
//            radio_rx(mode, m_channel);
//        }
//        else
//        {
//           //Do nothing
//        }
        
        if(increment)
        {
            s_Pod_TMTestState.channel += CHANNEL_INCREMENT;
            if (s_Pod_TMTestState.channel > channel_end)
            {
                s_Pod_TMTestState.channel = channel_start;
            }
        }
//    }
    // Timer sequence for function
    // radio_modulated_tx_carrier_with_delay(...)
//    if (NRF_TIMER0->EVENTS_COMPARE[1])
//    {
//        NRF_TIMER0->EVENTS_COMPARE[1] = 0;
//        if (!m_sweep_tx || !m_sweep_rx)
//            NRF_RADIO->TASKS_TXEN = 1;
//    }
    //MBT_LOG("[TM] SWEEP UPD\n");
}

/**@brief Function for handling the Application's system events.
 *
 * @param[in]   sys_evt   system event.
 */
void rftest_on_sys_evt(uint32_t sys_evt, void * p_context)
{
    switch(sys_evt)
    {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
        case NRF_EVT_FLASH_OPERATION_ERROR:
            break;
        case NRF_EVT_RADIO_BLOCKED:
        case NRF_EVT_RADIO_CANCELED:
        {
            // Blocked events are rescheduled with normal priority. They could also
            // be rescheduled with high priority if necessary.
            uint32_t err_code = sd_radio_request((nrf_radio_request_t*) &m_timeslot_req_earliest);
            APP_ERROR_CHECK(err_code);

            //m_blocked_cancelled_count++;

            break;
        }
        case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
            MBT_LOG("NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN\r\n");
            //app_error_handler(MAIN_DEBUG, __LINE__, (const uint8_t*)__FILE__);
            break;
        case NRF_EVT_RADIO_SESSION_CLOSED:
            {
                s_Pod_TMTestState.timeslot_session_open = false;

                MBT_LOG("NRF_EVT_RADIO_SESSION_CLOSED\r\n");
            }

            break;
        case NRF_EVT_RADIO_SESSION_IDLE:
        {
            MBT_LOG("NRF_EVT_RADIO_SESSION_IDLE\r\n");

            uint32_t err_code = sd_radio_session_close();
            APP_ERROR_CHECK(err_code);
            break;
        }
        default:
            // No implementation needed.
            MBT_LOG("Event: 0x%08x\r\n", sys_evt);
            break;
    }
}




/**@brief IRQHandler used for execution context management.
  *        Any available handler can be used as we're not using the associated hardware.
  *        This handler is used to stop and disable UESB
  */
void timeslot_end_handler(void)
{
    s_Pod_TMTestState.slot_open = false;
    radio_disable();
    s_Pod_TMTestState.radio_state = RADIO_STATE_IDLE;
    s_Pod_TMTestState.total_timeslot_length = 0;
    MBT_LOG("[TM] TS END\n");
}

/**@brief IRQHandler used for execution context management.
  *        Any available handler can be used as we're not using the associated hardware.
  *        This handler is used to initiate UESB RX/TX
  */
void timeslot_begin_handler(void)
{
    //rftest_irq_sweep_update();
    s_Pod_TMTestState.slot_open = true;
    rftest_irq_sweep_update(false);
    MBT_LOG("[TM] TS BEGIN\n");
}

void RADIO_IRQHandler(void)
{
//    if (NRF_RADIO->EVENTS_END != 0)
//    {
//        NRF_RADIO->EVENTS_END = 0;
//        (void)NRF_RADIO->EVENTS_END;
//
//        if (m_radio_state == RADIO_STATE_RX &&
//           (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) == (RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos))
//        {
//            sync_pkt_t * p_pkt;
//            bool         adjustment_procedure_started;
//
//            p_pkt = (sync_pkt_t *) NRF_RADIO->PACKETPTR;
//
//            adjustment_procedure_started = sync_timer_offset_compensate(p_pkt);
//
//            if (adjustment_procedure_started)
//            {
//                p_pkt = nrf_balloc_alloc(&m_sync_pkt_pool);
//                APP_ERROR_CHECK_BOOL(p_pkt != 0);
//
//                NRF_RADIO->PACKETPTR = (uint32_t) p_pkt;
//            }
//            ++m_rcv_count;
//        }
//
//        NRF_RADIO->TASKS_START = 1;
//    }
}

/**@brief   Function for handling timeslot events.
 */
static nrf_radio_signal_callback_return_param_t * radio_callback (uint8_t signal_type)
{
    // NOTE: This callback runs at lower-stack priority (the highest priority possible).
    switch (signal_type) {
    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_START:
        // TIMER0 is pre-configured for 1Mhz.
        NRF_TIMER0->TASKS_STOP          = 1;
        NRF_TIMER0->TASKS_CLEAR         = 1;
        NRF_TIMER0->MODE                = (TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos);
        NRF_TIMER0->EVENTS_COMPARE[0]   = 0;
        NRF_TIMER0->EVENTS_COMPARE[1]   = 0;

//        if (m_send_sync_pkt)
//        {
            //NRF_TIMER0->INTENSET  = (TIMER_INTENSET_COMPARE0_Set << TIMER_INTENSET_COMPARE0_Pos);
//        }
//        else
//        {
            NRF_TIMER0->INTENSET = (TIMER_INTENSET_COMPARE0_Set << TIMER_INTENSET_COMPARE0_Pos) |
                                   (TIMER_INTENSET_COMPARE1_Set << TIMER_INTENSET_COMPARE1_Pos);
//        }
        NRF_TIMER0->CC[0]               = (RFTEST_LEN_US - RFTEST_SAFETY_MARGIN_US);
        NRF_TIMER0->CC[1]               = (RFTEST_LEN_US - RFTEST_EXTEND_MARGIN_US);
        NRF_TIMER0->BITMODE             = (TIMER_BITMODE_BITMODE_24Bit << TIMER_BITMODE_BITMODE_Pos);
        NRF_TIMER0->TASKS_START         = 1;


        NRF_RADIO->POWER                = (RADIO_POWER_POWER_Enabled << RADIO_POWER_POWER_Pos);

        NVIC_EnableIRQ(TIMER0_IRQn);

        s_Pod_TMTestState.total_timeslot_length = 0;

        timeslot_begin_handler();

        break;

    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_TIMER0:
        if (NRF_TIMER0->EVENTS_COMPARE[0] &&
           (NRF_TIMER0->INTENSET & (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENCLR_COMPARE0_Pos)))
        {
            NRF_TIMER0->TASKS_STOP  = 1;
            NRF_TIMER0->EVENTS_COMPARE[0] = 0;
            (void)NRF_TIMER0->EVENTS_COMPARE[0];

            // This is the "timeslot is about to end" timeout

            timeslot_end_handler();

            // Schedule next timeslot
//            if (m_send_sync_pkt)
//            {
                m_timeslot_req_normal.params.normal.distance_us = s_Pod_TMTestState.total_timeslot_length + s_Pod_TMTestState.timeslot_distance;
                return (nrf_radio_signal_callback_return_param_t*) &m_rsc_return_sched_next_normal;
//            }
//            else
//            {
//                return (nrf_radio_signal_callback_return_param_t*) &m_rsc_return_sched_next_earliest;
//            }
        }

        bool m_send_sync_pkt = false;

        if (NRF_TIMER0->EVENTS_COMPARE[1] &&
           (NRF_TIMER0->INTENSET & (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENCLR_COMPARE1_Pos)))
        {
            NRF_TIMER0->EVENTS_COMPARE[1] = 0;
            (void)NRF_TIMER0->EVENTS_COMPARE[1];

            // This is the "try to extend timeslot" timeout

            if (s_Pod_TMTestState.total_timeslot_length < (128000000UL - 5000UL - TX_LEN_EXTENSION_US) && !m_send_sync_pkt)
            {
                // Request timeslot extension if total length does not exceed 128 seconds
                return (nrf_radio_signal_callback_return_param_t*) &m_rsc_extend;
            }
            else if (!m_send_sync_pkt)
            {
                // Don't do anything. Timeslot will end and new one requested upon the next timer0 compare.
            }
        }
// break?

    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_RADIO:
        RADIO_IRQHandler();
        break;

    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_EXTEND_FAILED:
        //MBT_LOG("[TM] EXT FAIL\n");
        // Don't do anything. Our timer will expire before timeslot ends
        return (nrf_radio_signal_callback_return_param_t*) &m_rsc_return_no_action;

    case NRF_RADIO_CALLBACK_SIGNAL_TYPE_EXTEND_SUCCEEDED:
        // Extension succeeded: update timer
        NRF_TIMER0->TASKS_STOP          = 1;
        NRF_TIMER0->EVENTS_COMPARE[0]   = 0;
        NRF_TIMER0->EVENTS_COMPARE[1]   = 0;
        NRF_TIMER0->CC[0]               += (TX_LEN_EXTENSION_US - 25);
        NRF_TIMER0->CC[1]               += (TX_LEN_EXTENSION_US - 25);
        NRF_TIMER0->TASKS_START         = 1;

        // Keep track of total length
        s_Pod_TMTestState.total_timeslot_length += TX_LEN_EXTENSION_US;
        //MBT_LOG("[TM] EXT OK\n");
        break;

    default:
        //app_error_handler(MAIN_DEBUG, __LINE__, (const uint8_t*)__FILE__);
        break;
    };

    // Fall-through return: return with no action request
    return (nrf_radio_signal_callback_return_param_t*) &m_rsc_return_no_action;
}


static uint32_t Pod_TMStartTimeslotSession(void)
{
    uint32_t err_code;

    if (s_Pod_TMTestState.timeslot_session_open)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    err_code = sd_clock_hfclk_request();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code |= sd_power_mode_set(NRF_POWER_MODE_CONSTLAT);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = sd_radio_session_open(radio_callback);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = sd_radio_request(&m_timeslot_req_earliest);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    s_Pod_TMTestState.timeslot_session_open = true;
}

void Pod_TMInit(void)
{
    memset(&s_Pod_TMTestState, 0, sizeof(s_Pod_TMTestState));    
}

#define POD_RFTEST_SWEEP_STEP_MS (100)

void Pod_TMTimer(void)
{
    MBT_TIMER_RESET_TIMEOUT(s_Pod_TMTestState.testUpdate_timer, MBT_US_TO_TICKS(POD_RFTEST_SWEEP_STEP_MS * 1000));
    if(s_Pod_TMTestState.slot_open)
    {        
        rftest_irq_sweep_update(true);
    }
}

void Pod_TMTick(uint32_t ticks)
{
    if(s_Pod_TMTestState.currentTest != s_Pod_TMTestState.wantedTest)
    {
        if(s_Pod_TMTestState.wantedTest == TT_SWEEP)
        {
            radio_tx_sweep_start(RADIO_TXPOWER_TXPOWER_Pos4dBm, RADIO_MODE_MODE_Nrf_1Mbit, rftest_calcChannel(1), rftest_calcChannel(40), 10);
            Pod_TMStartTimeslotSession();
            MBT_TIMER_RESET_TIMEOUT(s_Pod_TMTestState.testUpdate_timer, 1);
        } else {
            sd_radio_session_close();
            MBT_TIMER_DISABLE(s_Pod_TMTestState.testUpdate_timer);
        }
        s_Pod_TMTestState.currentTest = s_Pod_TMTestState.wantedTest;        
    }


    MBT_UPDATE_TIMER_ACTION_EXPIRED(s_Pod_TMTestState.testUpdate_timer, ticks,
    {        
        Pod_TMTimer();
    });
}

void Pod_TMRFTestSetMode(Pod_TMTestType_t mode)
{
    s_Pod_TMTestState.wantedTest = mode;
}

#else
void Pod_TMInit(void)
{
}

void Pod_TMTick(uint32_t ticks)
{
}
#endif
