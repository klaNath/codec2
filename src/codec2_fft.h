/*
 * codec2_fft.h
 *
 *  Created on: 17.09.2016
 *      Author: danilo
 */

#ifndef DRIVERS_FREEDV_CODEC2_FFT_H_
#define DRIVERS_FREEDV_CODEC2_FFT_H_

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FDV_ARM_MATH
#define USE_KISS_FFT
#else
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

#include "comp.h"
#include "defines.h"

typedef COMP codec2_fft_cpx;
#include "kiss_fftr.h"

#ifdef USE_KISS_FFT
#include "kiss_fft.h"
typedef kiss_fftr_cfg codec2_fftr_cfg;
typedef kiss_fft_cfg codec2_fft_cfg;
typedef kiss_fft_scalar codec2_fft_scalar;
#else
typedef float32_t codec2_fft_scalar;
typedef struct {
  arm_rfft_fast_instance_f32* instance;
  int inverse;
} codec2_fftr_struct;

typedef codec2_fftr_struct* codec2_fftr_cfg;

typedef struct {
  const arm_cfft_instance_f32* instance;
  int inverse;
} codec2_fft_struct;
typedef codec2_fft_struct* codec2_fft_cfg;
#endif

static inline void codec2_fftr(codec2_fftr_cfg cfg, codec2_fft_scalar* in,
                               codec2_fft_cpx* out) {
#ifdef USE_KISS_FFT
  kiss_fftr(cfg, in, (kiss_fft_cpx*)out);
#else
  // Note: arm_rfft_fast_f32() uses in[] as scratch space
  const int half = cfg->instance->fftLenRFFT / 2;
  arm_rfft_fast_f32(cfg->instance, in, (float*)out, cfg->inverse);
  // CMSIS packs the real valued bin at Fs/2 into out[0].imag.  kiss_fftr()
  // returns it as out[N/2], which is where the rest of Codec2 expects it, so
  // out[] must have room for N/2+1 bins
  out[half].real = out[0].imag;
  out[half].imag = 0;
  out[0].imag = 0;
#endif
}

static inline void codec2_fftri(codec2_fftr_cfg cfg, codec2_fft_cpx* in,
                                codec2_fft_scalar* out) {
#ifdef USE_KISS_FFT
  kiss_fftri(cfg, (kiss_fft_cpx*)in, out);
#else
  const int n = cfg->instance->fftLenRFFT;
  // pack the Fs/2 bin (in[N/2].real) into in[0].imag as CMSIS expects
  in[0].imag = in[n / 2].real;
  arm_rfft_fast_f32(cfg->instance, (float*)in, out, cfg->inverse);
  // The CMSIS inverse includes a scale of 1/N, kiss_fftri() does not.  The
  // caller compensates (see FFTI_FACTOR in synthesise()), so don't scale here.
#endif
}

codec2_fft_cfg codec2_fft_alloc(int nfft, int inverse_fft, void* mem,
                                size_t* lenmem);
codec2_fftr_cfg codec2_fftr_alloc(int nfft, int inverse_fft, void* mem,
                                  size_t* lenmem);
void codec2_fft_free(codec2_fft_cfg cfg);
void codec2_fftr_free(codec2_fftr_cfg cfg);

static inline void codec2_fft(codec2_fft_cfg cfg, codec2_fft_cpx* in,
                              codec2_fft_cpx* out) {
#ifdef USE_KISS_FFT
  kiss_fft(cfg, (kiss_fft_cpx*)in, (kiss_fft_cpx*)out);
#else
  memcpy(out, in, cfg->instance->fftLen * 2 * sizeof(float));
  arm_cfft_f32(cfg->instance, (float*)out, cfg->inverse, 1);
  // TODO: this is not nice, but for now required to keep changes minimal
  // however, since main goal is to reduce the memory usage
  // we should convert to an in place interface
  // on PC like platforms the overhead of using the "inplace" kiss_fft calls
  // is neglectable compared to the gain in memory usage on STM32 platforms
  if (cfg->inverse) {
    arm_scale_f32((float*)out, cfg->instance->fftLen, (float*)out,
                  cfg->instance->fftLen * 2);
  }
#endif
}

void codec2_fft_inplace(codec2_fft_cfg cfg, codec2_fft_cpx* inout);

#endif
