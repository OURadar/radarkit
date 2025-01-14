//
//  RKScratch.h
//  RadarKit
//
//  Created by Boonleng Cheong on 5/8/2021.
//  Copyright (c) Boonleng Cheong. All rights reserved.
//

#ifndef __RadarKit_Scratch__
#define __RadarKit_Scratch__

#include <RadarKit/RKFoundation.h>
#include <RadarKit/RKDSP.h>

typedef int8_t RKCellMask;
enum {
    RKCellMaskNull     = 0,
    RKCellMaskKeepH    = 1,
    RKCellMaskKeepV    = (1 << 1),
    RKCellMaskKeepBoth = RKCellMaskKeepH | RKCellMaskKeepV
};

//
// A scratch space for pulse compression
//

typedef struct rk_compression_scratch {
    RKName                           name;                                         //
    uint8_t                          verbose;                                      //
    uint32_t                         capacity;                                     // Capacity
    RKPulse                          *pulse;                                       //
    RKComplex                        *filter;                                      //
    RKFilterAnchor                   *filterAnchor;                                // (deprecating, use waveform->filterAnchor instead)
    RKFFTModule                      *fftModule;                                   // A reference to the common FFT module
    fftwf_complex                    *inBuffer;                                    //
    fftwf_complex                    *outBuffer;                                   //
    RKIQZ                            *zi;                                          //
    RKIQZ                            *zo;                                          //
    RKFloat                          *user1;                                       // User array #1, same storage length as pulse
    RKFloat                          *user2;                                       // User array #2, same storage length as pulse
    RKFloat                          *user3;                                       // User array #3, same storage length as pulse
    RKFloat                          *user4;                                       // User array #4, same storage length as pulse
    RKComplex                        *userComplex1;                                // User complex array #1, same storage length as pulse
    RKComplex                        *userComplex2;                                // User complex array #2, same storage length as pulse
    RKComplex                        *userComplex3;                                // User complex array #3, same storage length as pulse
    RKComplex                        *userComplex4;                                // User complex array #4, same storage length as pulse
    RKConfig                         *config;                                      //
    uint16_t                         waveformGroupdId;                             // Index of RKConfig->waveform to use
    uint16_t                         waveformFilterId;                             // Index of RKConfig->waveform->filterAnchor to use
    uint16_t                         planIndex;                                    // DFT plan index
    RKUserResource                   userResource;                                 //
} RKCompressionScratch;

//
// A scratch space for moment processor
//
typedef struct rk_moment_scratch {
    RKName                           name;                                         //
    uint8_t                          verbose;                                      //
    uint32_t                         capacity;                                     // Capacity
    uint8_t                          userLagChoice;                                // Number of lags in multi-lag estimator from user
    uint16_t                         gateCount;                                    // Gate count of the rays
    RKFloat                          gateSizeMeters;                               // Gate size in meters for range correction
    RKFloat                          samplingAdjustment;                           // Sampling adjustment going from pulse to ray conversion
    RKIQZ                            mX[2];                                        // Mean of X, 2 for dual-pol
    RKIQZ                            vX[2];                                        // Variance of X, i.e., E{X' * X} - E{X}' * E{X}
    RKIQZ                            R[2][RKMaximumLagCount];                      // ACF up to RKMaximumLagCount - 1 for each polarization
    RKIQZ                            C[2 * RKMaximumLagCount - 1];                 // CCF in [ -RKMaximumLagCount + 1, ..., -1, 0, 1, ..., RKMaximumLagCount - 1 ]
    RKIQZ                            sC;                                           // Summation of Xh * Xv'
    RKIQZ                            ts;                                           // Temporal scratch space
    RKIQZ                            RX[2][RKMaximumLagCount];                     // Cross-polar channel ACF up to RKMaximumLagCount - 1 for each polarization
    RKIQZ                            CX[2][2 * RKMaximumLagCount - 1];             // Co-polar-to-cross-polar CCF not yet multi-multilag
    RKFloat                          *aR[2][RKMaximumLagCount];                    // abs(ACF)
    RKFloat                          *aC[2 * RKMaximumLagCount - 1];               // abs(CCF)
    RKFloat                          *aRX[2][RKMaximumLagCount];                   // abs(RX)
    RKFloat                          *aCX[2][2 * RKMaximumLagCount - 1];           // abs(CX)
    RKFloat                          *gC;                                          // Gaussian fitted CCF(0)  NOTE: Need to extend this to multi-multilag
    RKFloat                          noise[2];                                     // Noise floor of each channel
    RKFloat                          velocityFactor;                               // Velocity factor to multiply by atan2(R(1))
    RKFloat                          widthFactor;                                  // Width factor to multiply by the ln(S/|R(1)|) :
    RKFloat                          KDPFactor;                                    // Normalization factor of 1.0 / gateWidth in kilometers
    RKFloat                          *dcal;                                        // Calibration offset to D (dB)
    RKFloat                          *pcal;                                        // Calibration offset to P (radians)
    RKFloat                          *S2Z[2];                                      // Signal to reflectivity correction factor (dB)
    RKFloat                          *S[2];                                        // Signal
    RKFloat                          *Z[2];                                        // Reflectivity in dB
    RKFloat                          *V[2];                                        // Velocity in same units as aliasing velocity
    RKFloat                          *W[2];                                        // Spectrum width in same units as aliasing velocity
    RKFloat                          *Q[2];                                        // Signal quality index SQI
    RKFloat                          *L[2];                                        // Linear depolarization ratio LDR
    RKFloat                          *RhoXP[2];                                    // Co-polar-to-cross-polar correlation coefficient RhoXP
    RKFloat                          *PhiXP[2];                                    // Co-polar-to-cross-polar differential phase PhiXP
    RKFloat                          *SNR[2];                                      // Signal-to-noise ratio
    RKFloat                          *ZDR;                                         // Differential reflectivity ZDR
    RKFloat                          *PhiDP;                                       // Differential phase PhiDP
    RKFloat                          *RhoHV;                                       // Cross-correlation coefficient RhoHV
    RKFloat                          *KDP;                                         // Specific phase KDP
    RKFloat                          *user1;                                       // User array #1, same storage length as ZDR, PhiDP, etc.
    RKFloat                          *user2;                                       // User array #2, same storage length as ZDR, PhiDP, etc.
    RKFloat                          *user3;                                       // User array #3, same storage length as ZDR, PhiDP, etc.
    RKFloat                          *user4;                                       // User array #4, same storage length as ZDR, PhiDP, etc.
    uint8_t                          *mask;                                        // Mask for censoring
    RKFFTModule                      *fftModule;                                   // A reference to the common FFT module
    fftwf_complex                    **inBuffer;                                   //
    fftwf_complex                    **outBuffer;                                  //
    fftwf_complex                    **fS[2];                                      // frquenct content of singal (fft[ACF])
    fftwf_complex                    **fC;                                         // frquenct content of singal (fft[CCF])
    int8_t                           fftOrder;                                     // FFT order that was used to perform FFT. This will be copied over to rayHeader
    RKConfig                         *config;                                      // A reference to the radar configuration
    RKMomentList                     calculatedMoments;                            // Calculated moments
    RKProductList                    calculatedProducts;                           // Calculated Products
} RKMomentScratch;

size_t RKCompressionScratchAlloc(RKCompressionScratch **, const uint32_t, const uint8_t, const char * _Nullable);
void RKCompressionScratchFree(RKCompressionScratch *);

size_t RKMomentScratchAlloc(RKMomentScratch **, const uint32_t, const uint8_t, const char *);
void RKMomentScratchFree(RKMomentScratch *);

int prepareScratch(RKMomentScratch *);
int makeRayFromScratch(RKMomentScratch *, RKRay *);

int RKNullProcessor(RKMomentScratch *, RKPulse **, const uint16_t);

#endif
