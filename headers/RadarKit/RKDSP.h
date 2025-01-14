//
//  RKDSP.h
//  RadarKit
//
//  Created by Boonleng Cheong on 3/18/15.
//  Copyright (c) 2015 Boonleng Cheong. All rights reserved.
//

#ifndef __RadarKit_DSP__
#define __RadarKit_DSP__

#include <fftw3.h>
#include <RadarKit/RKFoundation.h>
#include <RadarKit/RKSIMD.h>
#include <RadarKit/RKWindow.h>
#include <RadarKit/RKRamp.h>

typedef struct rk_fft_resource {
    unsigned int                     size;
    unsigned int                     count;
    fftwf_plan                       forwardInPlace;
    fftwf_plan                       forwardOutPlace;
    fftwf_plan                       backwardInPlace;
    fftwf_plan                       backwardOutPlace;
} RKFFTResource;

typedef struct rk_fft_module {
    RKName                           name;
    int                              verbose;
    bool                             exportWisdom;
    unsigned int                     count;
    char                             wisdomFile[RKMaximumPathLength];
    RKFFTResource                    *plans;
} RKFFTModule;

typedef struct rk_gaussian {
    RKFloat                          A;                                            //
    RKFloat                          mu;                                           //
    RKFloat                          sigma;                                        //
} RKGaussian;

//
//
//

float RKGetSignedMinorSectorInDegrees(const float angle1, const float angle2);
float RKGetMinorSectorInDegrees(const float angle1, const float angle2);
float RKInterpolatePositiveAngles(const float angleBefore, const float angleAfter, const float alpha);
float RKInterpolateAngles(const float angleLeft, const float angleRight, const float alpha);

int RKMeasureNoiseFromPulse(RKFloat *noise, RKPulse *pulse, const int origin);
int RKBestStrideOfHopsV1(const int hopCount, const bool showNumbers);
int RKBestStrideOfHops(const int hopCount, const bool showNumbers);

void RKHilbertTransform(RKFloat *x, RKComplex *y, const int n);

void RKFasterSineCosine(float x, float *sin, float *cos);
void RKFastSineCosine(float x, float *sin, float *cos);

//
// FIR + IIR Filters
//

void RKGetFilterCoefficients(RKIIRFilter *filter, const RKFilterType type);

//
// Common FFT plans
//

RKFFTModule *RKFFTModuleInit(const uint32_t capacity, const int verb);
void RKFFTModuleFree(RKFFTModule *);

// xcorr() ?
// ambiguity function
//

RKGaussian RKSGFit(RKFloat *x, RKComplex *y, const int count);

//
// Half, Single, and Double Precision Floats
//

RKWordFloat64 RKSingle2Double(const RKWordFloat32 x);
RKWordFloat32 RKHalf2Single(const RKWordFloat16 x);

//
// Bit Manipulation
//
uint32_t RKBitReverse32(uint32_t);
uint16_t RKBitReverse64(uint16_t);
uint8_t RKBitReverse8(uint8_t);

//
// Visual
//
void RKShowWordFloat16(const RKWordFloat16, const float);
void RKShowWordFloat32(const RKWordFloat32, const float);
void RKShowWordFloat64(const RKWordFloat64, const float);

#endif /* defined(__RadarKit_RKDSP__) */
