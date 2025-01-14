//
//  RKScratch.c
//  RadarKit
//
//  Created by Boonleng Cheong.
//  Copyright (c) Boonleng Cheong. All rights reserved.
//
//  <  PulseScratch >
//  <CompressScratch>
//  < MomentScratch >
//  < FilterScratch >
//  <FastTimeScratch>
//  <SlowTimeScratch>

#include <RadarKit/RKScratch.h>

#pragma mark - Helper Functions

#pragma mark - Scratch Space

// Allocate a scratch space for pulse compression
size_t RKCompressionScratchAlloc(RKCompressionScratch **buffer, const uint32_t capacity, const uint8_t verbose, const char * _Nullable name) {
    uint32_t goodCapacity = (capacity * sizeof(RKFloat) / RKMemoryAlignSize) * RKMemoryAlignSize / sizeof(RKFloat);
    if (capacity == 0 || capacity != goodCapacity) {
        RKLog("Error. Unable to allocate capacity = %d. Must be greater than 0 and an integer multiple of %s!",
            RKIntegerToCommaStyleString(capacity),
            RKIntegerToCommaStyleString(RKMemoryAlignSize / sizeof(RKFloat)));
        return 0;
    }
    *buffer = (RKCompressionScratch *)malloc(sizeof(RKCompressionScratch));
    if (*buffer == NULL) {
        RKLog("Error. Unable to allocate a compression scratch space.\n");
        return 0;
    }
    memset(*buffer, 0, sizeof(RKCompressionScratch));

    const uint32_t nfft = 1 << (int)ceilf(log2f((float)capacity));

    RKCompressionScratch *scratch = (RKCompressionScratch *)*buffer;
    scratch->capacity = capacity;
    scratch->verbose = verbose;
    if (name) {
        strcpy(scratch->name, name);
    } else {
        snprintf(scratch->name, RKNameLength, "%s<  PulseScratch >%s",
                 rkGlobalParameters.showColor ? RKGetBackgroundColorOfIndex(RKEngineColorMisc) : "",
                 rkGlobalParameters.showColor ? RKNoColor : "");
    }

    if (scratch->verbose) {
        RKLog("%s Scratch space allocated.   capacity = %s   fast-nfft = %s",
            scratch->name,
            RKIntegerToCommaStyleString(capacity),
            RKIntegerToCommaStyleString(nfft));
    }

    size_t mem = sizeof(RKCompressionScratch);

    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->inBuffer, RKMemoryAlignSize, nfft * sizeof(fftwf_complex)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->outBuffer, RKMemoryAlignSize, nfft * sizeof(fftwf_complex)))
    if (scratch->inBuffer == NULL || scratch->outBuffer == NULL) {
        RKLog("%s Error. Unable to allocate resources for FFTW.\n", scratch->name);
        return 0;
    }
    mem += 2 * nfft * sizeof(fftwf_complex);
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->zi, RKMemoryAlignSize, nfft * sizeof(RKFloat)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->zo, RKMemoryAlignSize, nfft * sizeof(RKFloat)))
    if (scratch->zi == NULL || scratch->zo == NULL) {
        RKLog("%s Error. Unable to allocate resources for FFTW.\n", scratch->name);
        return 0;
    }
    mem += 2 * nfft * sizeof(RKFloat);
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user1, RKMemoryAlignSize, capacity * sizeof(RKFloat)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user2, RKMemoryAlignSize, capacity * sizeof(RKFloat)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user3, RKMemoryAlignSize, capacity * sizeof(RKFloat)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user4, RKMemoryAlignSize, capacity * sizeof(RKFloat)))
    mem += 4 * capacity * sizeof(RKFloat);
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->userComplex1, RKMemoryAlignSize, capacity * sizeof(RKComplex)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->userComplex2, RKMemoryAlignSize, capacity * sizeof(RKComplex)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->userComplex3, RKMemoryAlignSize, capacity * sizeof(RKComplex)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->userComplex4, RKMemoryAlignSize, capacity * sizeof(RKComplex)))
    mem += 4 * capacity * sizeof(RKComplex);
    return mem;
}

void RKCompressionScratchFree(RKCompressionScratch *scratch) {
    free(scratch->user1);
    free(scratch->user2);
    free(scratch->user3);
    free(scratch->user4);
    free(scratch->userComplex1);
    free(scratch->userComplex2);
    free(scratch->userComplex3);
    free(scratch->userComplex4);
    free(scratch->inBuffer);
    free(scratch->outBuffer);
    free(scratch->zi);
    free(scratch->zo);
    free(scratch);
}

// Allocate a scratch space for moment processors
size_t RKMomentScratchAlloc(RKMomentScratch **buffer, const uint32_t capacity, const uint8_t verbose, const char * _Nullable name) {
    uint32_t goodCapacity = (capacity * sizeof(RKFloat) / RKMemoryAlignSize) * RKMemoryAlignSize / sizeof(RKFloat);
    if (capacity == 0 || capacity != goodCapacity) {
        RKLog("Error. Scratch space capacity must be greater than 0 and an integer multiple of %s!",
              RKIntegerToCommaStyleString(RKMemoryAlignSize / sizeof(RKFloat)));
        return 0;
    }
    *buffer = malloc(sizeof(RKMomentScratch));
    if (*buffer == NULL) {
        RKLog("Error. Unable to allocate a momment scratch space.\n");
        return 0;
    }
    memset(*buffer, 0, sizeof(RKMomentScratch));

    const uint32_t nfft = 1 << (int)ceilf(log2f((float)RKMaximumPulsesPerRay));

    RKMomentScratch *scratch = *buffer;
    scratch->capacity = capacity;
    scratch->verbose = verbose;
    if (name) {
        strcpy(scratch->name, name);
    } else {
        snprintf(scratch->name, RKNameLength, "%s< MomentScratch >%s",
                 rkGlobalParameters.showColor ? RKGetBackgroundColorOfIndex(RKEngineColorMisc) : "",
                 rkGlobalParameters.showColor ? RKNoColor : "");
    }

    if (scratch->verbose) {
        RKLog("%s Scratch space allocated.   capacity = %s   slow-nfft = %s",
            scratch->name,
            RKIntegerToCommaStyleString(capacity),
            RKIntegerToCommaStyleString(nfft));
    }

    int j, k, s = 0;
    size_t bytes = sizeof(RKMomentScratch);

    for (k = 0; k < 2; k++) {
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->mX[k].i, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->mX[k].q, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->vX[k].i, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->vX[k].q, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->S[k],    RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->Z[k],    RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->V[k],    RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->W[k],    RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->Q[k],    RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->L[k],    RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->RhoXP[k],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->PhiXP[k],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->SNR[k],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->S2Z[k],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        memset(scratch->S2Z[k], 0, scratch->capacity * sizeof(RKFloat));
        s += 14;
        for (j = 0; j < RKMaximumLagCount; j++) {
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->R[k][j].i, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->R[k][j].q, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->aR[k][j],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->RX[k][j].i, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->RX[k][j].q, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->aRX[k][j],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            s += 6;
        }
    }
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->sC.i,  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->sC.q,  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->ts.i,  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->ts.q,  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->ZDR,   RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->PhiDP, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->RhoHV, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->KDP,   RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user1, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user2, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user3, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->user4, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->dcal,  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->pcal,  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    memset(scratch->dcal, 0, scratch->capacity * sizeof(RKFloat));
    memset(scratch->pcal, 0, scratch->capacity * sizeof(RKFloat));
    s += 14;

    for (j = 0; j < 2 * RKMaximumLagCount - 1; j++) {
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->C[j].i, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->C[j].q, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->aC[j],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
        s += 3;
        for (k = 0; k < 2; k++) {
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->CX[k][j].i, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->CX[k][j].q, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->aCX[k][j],  RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
            s += 3;
        }
    }
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->gC, RKMemoryAlignSize, scratch->capacity * sizeof(RKFloat)));
    s++;
    bytes += s * sizeof(RKFloat);

    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->mask, RKMemoryAlignSize, scratch->capacity * sizeof(uint8_t)));
    bytes += scratch->capacity * sizeof(uint8_t);

    scratch->inBuffer = (fftwf_complex **)malloc(scratch->capacity * sizeof(fftwf_complex *));
    scratch->outBuffer = (fftwf_complex **)malloc(scratch->capacity * sizeof(fftwf_complex *));
    scratch->fS[0] = (fftwf_complex **)malloc(scratch->capacity * sizeof(fftwf_complex *));
    scratch->fS[1] = (fftwf_complex **)malloc(scratch->capacity * sizeof(fftwf_complex *));
    scratch->fC = (fftwf_complex **)malloc(scratch->capacity * sizeof(fftwf_complex *));
    bytes += 5 * scratch->capacity * sizeof(fftwf_complex *);
    for (j = 0; j < scratch->capacity; j++) {
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->inBuffer[j], RKMemoryAlignSize, nfft * sizeof(fftwf_complex)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->outBuffer[j], RKMemoryAlignSize, nfft * sizeof(fftwf_complex)));
        POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->fC[j], RKMemoryAlignSize, nfft * sizeof(fftwf_complex)));
        for (k = 0; k < 2; k++) {
            POSIX_MEMALIGN_CHECK(posix_memalign((void **)&scratch->fS[k][j], RKMemoryAlignSize, nfft * sizeof(fftwf_complex)));
        }
    }
    bytes += scratch->capacity * 5 * nfft * sizeof(fftwf_complex);
    scratch->calculatedProducts = RKProductListFloatZVWDPRKSQ | RKProductListUInt8ZVWDPRKSQ;
    return bytes;
}

void RKMomentScratchFree(RKMomentScratch *scratch) {
    int j, k;
    for (k = 0; k < 2; k++) {
        free(scratch->mX[k].i);
        free(scratch->mX[k].q);
        free(scratch->vX[k].i);
        free(scratch->vX[k].q);
        free(scratch->S[k]);
        free(scratch->Z[k]);
        free(scratch->V[k]);
        free(scratch->W[k]);
        free(scratch->Q[k]);
        free(scratch->SNR[k]);
        free(scratch->L[k]);
        free(scratch->RhoXP[k]);
        free(scratch->PhiXP[k]);
        free(scratch->S2Z[k]);
        for (j = 0; j < RKMaximumLagCount; j++) {
            free(scratch->R[k][j].i);
            free(scratch->R[k][j].q);
            free(scratch->aR[k][j]);
            free(scratch->RX[k][j].i);
            free(scratch->RX[k][j].q);
            free(scratch->aRX[k][j]);
        }
    }
    free(scratch->sC.i);
    free(scratch->sC.q);
    free(scratch->ts.i);
    free(scratch->ts.q);
    free(scratch->ZDR);
    free(scratch->PhiDP);
    free(scratch->RhoHV);
    free(scratch->KDP);
    free(scratch->user1);
    free(scratch->user2);
    free(scratch->user3);
    free(scratch->user4);
    free(scratch->dcal);
    free(scratch->pcal);
    for (j = 0; j < 2 * RKMaximumLagCount - 1; j++) {
        free(scratch->C[j].i);
        free(scratch->C[j].q);
        free(scratch->aC[j]);
        for (k = 0; k < 2; k++) {
            free(scratch->CX[k][j].i);
            free(scratch->CX[k][j].q);
            free(scratch->aCX[k][j]);
        }
    }
    free(scratch->gC);
    free(scratch->mask);
    for (k = 0; k < scratch->capacity; k++) {
        free(scratch->inBuffer[k]);
        free(scratch->outBuffer[k]);
    }
    free(scratch->inBuffer);
    free(scratch->outBuffer);
    free(scratch);
}

#pragma mark -

int prepareScratch(RKMomentScratch *scratch) {
    scratch->calculatedProducts = RKProductListFloatZVWDPRKSQ | RKProductListUInt8ZVWDPRKSQ;
    scratch->fftOrder = -1;
    return 0;
}

// This function converts the float data calculated from a chosen processor to uint8_t type, which also represent
// the display data for the front end
int makeRayFromScratch(RKMomentScratch *scratch, RKRay *ray) {
    int k;
    uint8_t *mask;
    float SNRh, SNRv;
    float SNRThreshold, SQIThreshold;

    // (Deprecating)
    // Grab the relevant data from scratch space for float to 16-bit quantization
    // RKFloat *iHmi  = scratch->mX[0].i;      int16_t *oHmi  = RKGetInt16DataFromRay(ray, RKMomentIndexHmi);
    // RKFloat *iHmq  = scratch->mX[0].q;      int16_t *oHmq  = RKGetInt16DataFromRay(ray, RKMomentIndexHmq);
    // RKFloat *iHR0  = scratch->aR[0][0];     int16_t *oHR0  = RKGetInt16DataFromRay(ray, RKMomentIndexHR0);
    // RKFloat *iHR1i = scratch->R[0][1].i;    int16_t *oHR1i = RKGetInt16DataFromRay(ray, RKMomentIndexHR1i);
    // RKFloat *iHR1q = scratch->R[0][1].q;    int16_t *oHR1q = RKGetInt16DataFromRay(ray, RKMomentIndexHR1q);
    // RKFloat *iHR2  = scratch->aR[0][2];     int16_t *oHR2  = RKGetInt16DataFromRay(ray, RKMomentIndexHR2);
    // RKFloat *iHR3  = scratch->aR[0][3];     int16_t *oHR3  = RKGetInt16DataFromRay(ray, RKMomentIndexHR3);
    // RKFloat *iHR4  = scratch->aR[0][3];     int16_t *oHR4  = RKGetInt16DataFromRay(ray, RKMomentIndexHR4);
    // RKFloat *iVmi  = scratch->mX[1].i;      int16_t *oVmi  = RKGetInt16DataFromRay(ray, RKMomentIndexVmi);
    // RKFloat *iVmq  = scratch->mX[1].q;      int16_t *oVmq  = RKGetInt16DataFromRay(ray, RKMomentIndexVmq);
    // RKFloat *iVR0  = scratch->aR[1][0];     int16_t *oVR0  = RKGetInt16DataFromRay(ray, RKMomentIndexVR0);
    // RKFloat *iVR1i = scratch->R[1][1].i;    int16_t *oVR1i = RKGetInt16DataFromRay(ray, RKMomentIndexVR1i);
    // RKFloat *iVR1q = scratch->R[1][1].q;    int16_t *oVR1q = RKGetInt16DataFromRay(ray, RKMomentIndexVR1q);
    // RKFloat *iVR2  = scratch->aR[1][2];     int16_t *oVR2  = RKGetInt16DataFromRay(ray, RKMomentIndexVR2);
    // RKFloat *iVR3  = scratch->aR[1][3];     int16_t *oVR3  = RKGetInt16DataFromRay(ray, RKMomentIndexVR3);
    // RKFloat *iVR4  = scratch->aR[1][3];     int16_t *oVR4  = RKGetInt16DataFromRay(ray, RKMomentIndexVR4);

    // // Convert float to 16-bit representation (-32768 - 32767) using ...
    // for (k = 0; k < MIN(scratch->capacity, scratch->gateCount); k++) {
    //     *oHmi++  = (int16_t)(*iHmi++);
    //     *oHmq++  = (int16_t)(*iHmq++);
    //     *oHR0++  = (int16_t)(1000.0f * log2f(*iHR0++));
    //     *oHR1i++ = (int16_t)(1000.0f * log2f(*iHR1i++));
    //     *oHR1q++ = (int16_t)(1000.0f * log2f(*iHR1q++));
    //     *oHR2++  = (int16_t)(1000.0f * log2f(*iHR2++));
    //     *oHR3++  = (int16_t)(1000.0f * log2f(*iHR3++));
    //     *oHR4++  = (int16_t)(1000.0f * log2f(*iHR4++));
    //     *oVmi++  = (int16_t)(*iVmi++);
    //     *oVmq++  = (int16_t)(*iVmq++);
    //     *oVR0++  = (int16_t)(1000.0f * log2f(*iVR0++));
    //     *oVR1i++ = (int16_t)(1000.0f * log2f(*iVR1i++));
    //     *oVR1q++ = (int16_t)(1000.0f * log2f(*iVR1q++));
    //     *oVR2++  = (int16_t)(1000.0f * log2f(*iVR2++));
    //     *oVR3++  = (int16_t)(1000.0f * log2f(*iVR3++));
    //     *oVR4++  = (int16_t)(1000.0f * log2f(*iVR4++));
    // }
    // ? Recover the float from 16-bit so that output is the same as generating products from level 15 data?
    // #if defined(EMULATE_15)
    // iHmi  = space->mX[0].i;      oHmi  = RKGetInt16DataFromRay(ray, RKMomentIndexHmi);
    // iHmq  = space->mX[0].q;      oHmq  = RKGetInt16DataFromRay(ray, RKMomentIndexHmq);
    // iHR0  = space->aR[0][0];     oHR0  = RKGetInt16DataFromRay(ray, RKMomentIndexHR0);
    // iHR1i = space->R[0][1].i;    oHR1i = RKGetInt16DataFromRay(ray, RKMomentIndexHR1i);
    // iHR1q = space->R[0][1].q;    oHR1q = RKGetInt16DataFromRay(ray, RKMomentIndexHR1q);
    // iHR2  = space->aR[0][2];     oHR2  = RKGetInt16DataFromRay(ray, RKMomentIndexHR2);
    // iHR3  = space->aR[0][3];     oHR3  = RKGetInt16DataFromRay(ray, RKMomentIndexHR3);
    // iHR4  = space->aR[0][3];     oHR4  = RKGetInt16DataFromRay(ray, RKMomentIndexHR4);
    // iVmi  = space->mX[1].i;      oVmi  = RKGetInt16DataFromRay(ray, RKMomentIndexVmi);
    // iVmq  = space->mX[1].q;      oVmq  = RKGetInt16DataFromRay(ray, RKMomentIndexVmq);
    // iVR0  = space->aR[1][0];     oVR0  = RKGetInt16DataFromRay(ray, RKMomentIndexVR0);
    // iVR1i = space->R[1][1].i;    oVR1i = RKGetInt16DataFromRay(ray, RKMomentIndexVR1i);
    // iVR1q = space->R[1][1].q;    oVR1q = RKGetInt16DataFromRay(ray, RKMomentIndexVR1q);
    // iVR2  = space->aR[1][2];     oVR2  = RKGetInt16DataFromRay(ray, RKMomentIndexVR2);
    // iVR3  = space->aR[1][3];     oVR3  = RKGetInt16DataFromRay(ray, RKMomentIndexVR3);
    // iVR4  = space->aR[1][3];     oVR4  = RKGetInt16DataFromRay(ray, RKMomentIndexVR4);
    // for (k = 0; k < MIN(space->capacity, space->gateCount); k++) {
    //     *iHmi++  = (RKFloat)(*oHmi++);
    //     *iHmq++  = (RKFloat)(*oHmq++);
    //     *iHR0++  = powf(2.0f, 0.001f * (RKFloat)*oHR0++);
    //     *iHR1i++ = powf(2.0f, 0.001f * (RKFloat)*oHR1i++);
    //     *iHR1q++ = powf(2.0f, 0.001f * (RKFloat)*oHR1q++);
    //     *iHR2++  = powf(2.0f, 0.001f * (RKFloat)*oHR2++);
    //     *iHR3++  = powf(2.0f, 0.001f * (RKFloat)*oHR3++);
    //     *iHR4++  = powf(2.0f, 0.001f * (RKFloat)*oHR4++);
    //     *iVmi++  = (RKFloat)(*oVmi++);
    //     *iVmq++  = (RKFloat)(*oVmq++);
    //     *iVR0++  = powf(2.0f, 0.001f * (RKFloat)*oVR0++);
    //     *iVR1i++ = powf(2.0f, 0.001f * (RKFloat)*oVR1i++);
    //     *iVR1q++ = powf(2.0f, 0.001f * (RKFloat)*oVR1q++);
    //     *iVR2++  = powf(2.0f, 0.001f * (RKFloat)*oVR2++);
    //     *iVR3++  = powf(2.0f, 0.001f * (RKFloat)*oVR3++);
    //     *iVR4++  = powf(2.0f, 0.001f * (RKFloat)*oVR4++);
    // }
    // #endif

    // Grab the data from scratch space.
    RKFloat *SHi = scratch->S[0],  *SHo = RKGetFloatDataFromRay(ray, RKProductIndexSh);
    RKFloat *ZHi = scratch->Z[0],  *ZHo = RKGetFloatDataFromRay(ray, RKProductIndexZ);
    RKFloat *VHi = scratch->V[0],  *VHo = RKGetFloatDataFromRay(ray, RKProductIndexV);
    RKFloat *WHi = scratch->W[0],  *WHo = RKGetFloatDataFromRay(ray, RKProductIndexW);
    RKFloat *SVi = scratch->S[1],  *SVo = RKGetFloatDataFromRay(ray, RKProductIndexSv);
    RKFloat *ZVi = scratch->Z[1],  *ZVo = RKGetFloatDataFromRay(ray, RKProductIndexZv);
    RKFloat *VVi = scratch->V[1],  *VVo = RKGetFloatDataFromRay(ray, RKProductIndexVv);
    RKFloat *WVi = scratch->W[1],  *WVo = RKGetFloatDataFromRay(ray, RKProductIndexWv);
    RKFloat *QHi = scratch->Q[0],  *QHo = RKGetFloatDataFromRay(ray, RKProductIndexQ);
    RKFloat *QVi = scratch->Q[1];
    RKFloat *LHi = scratch->L[0],  *LHo = RKGetFloatDataFromRay(ray, RKProductIndexLh);
    RKFloat *LVi = scratch->L[1],  *LVo = RKGetFloatDataFromRay(ray, RKProductIndexLv);
    RKFloat *PhiXHi = scratch->PhiXP[0],  *PhiXHo = RKGetFloatDataFromRay(ray, RKProductIndexPXh);
    RKFloat *PhiXVi = scratch->PhiXP[1],  *PhiXVo = RKGetFloatDataFromRay(ray, RKProductIndexPXv);
    RKFloat *RhoXHi = scratch->RhoXP[0],  *RhoXHo = RKGetFloatDataFromRay(ray, RKProductIndexRXh);
    RKFloat *RhoXVi = scratch->RhoXP[1],  *RhoXVo = RKGetFloatDataFromRay(ray, RKProductIndexRXv);
    RKFloat *Di = scratch->ZDR,   *Do = RKGetFloatDataFromRay(ray, RKProductIndexD);
    RKFloat *Pi = scratch->PhiDP, *Po = RKGetFloatDataFromRay(ray, RKProductIndexP);
    RKFloat *Ki = scratch->KDP,   *Ko = RKGetFloatDataFromRay(ray, RKProductIndexK);
    RKFloat *Ri = scratch->RhoHV, *Ro = RKGetFloatDataFromRay(ray, RKProductIndexR);

    SNRThreshold = powf(10.0f, 0.1f * scratch->config->SNRThreshold);
    SQIThreshold = scratch->config->SQIThreshold;
    mask = scratch->mask;
    // Masking based on SNR and SQI
    for (k = 0; k < MIN(scratch->capacity, scratch->gateCount); k++) {
        SNRh = *SHi / scratch->noise[0];
        SNRv = *SVi / scratch->noise[1];
        *SHo++ = 10.0f * log10f(*SHi++) - 80.0f;                    // Still need the mapping coefficient from ADU-dB to dBm
        *SVo++ = 10.0f * log10f(*SVi++) - 80.0f;
        *QHo++ = *QHi;
        *mask = RKCellMaskNull;
        if (SNRh > SNRThreshold && *QHi > SQIThreshold) {
            *mask |= RKCellMaskKeepH;
        }
        if (SNRv > SNRThreshold && *QVi > SQIThreshold) {
            *mask |= RKCellMaskKeepV;
        }
        mask++;
        QHi++;
        QVi++;
    }

    // Simple despeckling: censor the current cell if the next cell is censored
    mask = scratch->mask;
    for (k = 0; k < MIN(scratch->capacity, scratch->gateCount) - 1; k++) {
        if (!(*(mask + 1) & RKCellMaskKeepH)) {
            *mask &= ~RKCellMaskKeepH;
        }
        if (!(*(mask + 1) & RKCellMaskKeepV)) {
            *mask &= ~RKCellMaskKeepV;
        }
        mask++;
    }

    // Now we copy out the values based on mask
    mask = scratch->mask;
    for (k = 0; k < MIN(scratch->capacity, scratch->gateCount); k++) {
        if (*mask & RKCellMaskKeepH) {
            *ZHo++ = *ZHi;
            *VHo++ = *VHi;
            *WHo++ = *WHi;
            *ZVo++ = *ZVi;
            *VVo++ = *VVi;
            *WVo++ = *WVi;
        } else {
            *ZHo++ = NAN;
            *VHo++ = NAN;
            *WHo++ = NAN;
            *ZVo++ = NAN;
            *VVo++ = NAN;
            *WVo++ = NAN;
        }
        if (*mask == RKCellMaskKeepBoth) {
            *Do++ = *Di;
            *Po++ = *Pi;
            *Ko++ = *Ki;
            *Ro++ = *Ri;
            *LHo++ = *LHi;
            *LVo++ = *LVi;
            *PhiXHo++ = *PhiXHi;
            *PhiXVo++ = *PhiXVi;
            *RhoXHo++ = *RhoXHi;
            *RhoXVo++ = *RhoXVi;
        } else {
            *Do++ = NAN;
            *Po++ = NAN;
            *Ko++ = NAN;
            *Ro++ = NAN;
            *LHo++ = NAN;
            *LVo++ = NAN;
            *PhiXHo++ = NAN;
            *PhiXVo++ = NAN;
            *RhoXHo++ = NAN;
            *RhoXVo++ = NAN;
        }
        mask++;
        ZHi++;
        VHi++;
        WHi++;
        ZVi++;
        VVi++;
        WVi++;
        Di++;
        Pi++;
        Ki++;
        Ri++;
        LHi++;
        LVi++;
        PhiXHi++;
        PhiXVi++;
        RhoXHi++;
        RhoXVi++;
    }
    // Record down the calculated products and down-sampled gate count
    ray->header.productList = scratch->calculatedProducts;
    ray->header.gateCount = k;

    // Convert float to 8-bit representation (0.0 - 255.0) using M * (value) + A; RhoHV is special
    // #if defined(_COMPUTE_DISPLAY_8)
    RKFloat lhma[4];
    const int K = (ray->header.gateCount * sizeof(RKFloat) + sizeof(RKVec) - 1) / sizeof(RKVec);
    RKSLHMAC   RKVec sl = _rk_mm_set1(lhma[0]);  RKVec sh = _rk_mm_set1(lhma[1]);  RKVec sm = _rk_mm_set1(lhma[2]);  RKVec sa = _rk_mm_set1(lhma[3]);
    RKZLHMAC   RKVec zl = _rk_mm_set1(lhma[0]);  RKVec zh = _rk_mm_set1(lhma[1]);  RKVec zm = _rk_mm_set1(lhma[2]);  RKVec za = _rk_mm_set1(lhma[3]);
    RKV2LHMAC  RKVec vl = _rk_mm_set1(lhma[0]);  RKVec vh = _rk_mm_set1(lhma[1]);  RKVec vm = _rk_mm_set1(lhma[2]);  RKVec va = _rk_mm_set1(lhma[3]);
    RKWLHMAC   RKVec wl = _rk_mm_set1(lhma[0]);  RKVec wh = _rk_mm_set1(lhma[1]);  RKVec wm = _rk_mm_set1(lhma[2]);  RKVec wa = _rk_mm_set1(lhma[3]);
    RKQLHMAC   RKVec ql = _rk_mm_set1(lhma[0]);  RKVec qh = _rk_mm_set1(lhma[1]);  RKVec qm = _rk_mm_set1(lhma[2]);  RKVec qa = _rk_mm_set1(lhma[3]);
    RKDLHMAC   RKVec dl = _rk_mm_set1(lhma[0]);  RKVec dh = _rk_mm_set1(lhma[1]);  RKVec dm = _rk_mm_set1(lhma[2]);  RKVec da = _rk_mm_set1(lhma[3]);
    RKPLHMAC   RKVec pl = _rk_mm_set1(lhma[0]);  RKVec ph = _rk_mm_set1(lhma[1]);  RKVec pm = _rk_mm_set1(lhma[2]);  RKVec pa = _rk_mm_set1(lhma[3]);
    RKKLHMAC   RKVec kl = _rk_mm_set1(lhma[0]);  RKVec kh = _rk_mm_set1(lhma[1]);  RKVec km = _rk_mm_set1(lhma[2]);  RKVec ka = _rk_mm_set1(lhma[3]);
    RKLLHMAC   RKVec ll = _rk_mm_set1(lhma[0]);  RKVec lh = _rk_mm_set1(lhma[1]);  RKVec lm = _rk_mm_set1(lhma[2]);  RKVec la = _rk_mm_set1(lhma[3]);
    RKRLHMAC   RKVec rl = _rk_mm_set1(lhma[0]);  RKVec rh = _rk_mm_set1(lhma[1]);
    RKVec *SHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexSh); RKVec *SHo_pf = (RKVec *)scratch->S[0];
    RKVec *ZHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexZ);  RKVec *ZHo_pf = (RKVec *)scratch->Z[0];
    RKVec *VHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexV);  RKVec *VHo_pf = (RKVec *)scratch->V[0];
    RKVec *WHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexW);  RKVec *WHo_pf = (RKVec *)scratch->W[0];
    RKVec *SVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexSv); RKVec *SVo_pf = (RKVec *)scratch->S[1];
    RKVec *ZVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexZv); RKVec *ZVo_pf = (RKVec *)scratch->Z[1];
    RKVec *VVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexVv); RKVec *VVo_pf = (RKVec *)scratch->V[1];
    RKVec *WVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexWv); RKVec *WVo_pf = (RKVec *)scratch->W[1];
    RKVec *Qi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexQ);  RKVec *Qo_pf = (RKVec *)scratch->Q[0];
    RKVec *Di_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexD);  RKVec *Do_pf = (RKVec *)scratch->ZDR;
    RKVec *Pi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexP);  RKVec *Po_pf = (RKVec *)scratch->PhiDP;
    RKVec *Ki_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexK);  RKVec *Ko_pf = (RKVec *)scratch->KDP;
    RKVec *Ri_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexR);  RKVec *Ro_pf = (RKVec *)scratch->RhoHV;
    RKVec *LHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexLh);  RKVec *LHo_pf = (RKVec *)scratch->L[0];
    RKVec *LVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexLv);  RKVec *LVo_pf = (RKVec *)scratch->L[1];
    RKVec *RhoXHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexRXh);  RKVec *RhoXHo_pf = (RKVec *)scratch->RhoXP[0];
    RKVec *RhoXVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexRXv);  RKVec *RhoXVo_pf = (RKVec *)scratch->RhoXP[1];
    RKVec *PhiXHi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexPXh);  RKVec *PhiXHo_pf = (RKVec *)scratch->PhiXP[0];
    RKVec *PhiXVi_pf = (RKVec *)RKGetFloatDataFromRay(ray, RKProductIndexPXv);  RKVec *PhiXVo_pf = (RKVec *)scratch->PhiXP[1];

    for (k = 0; k < K; k++) {
        *SHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*SHi_pf++, sl), sh), sm), sa);
        *ZHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*ZHi_pf++, zl), zh), zm), za);
        *VHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*VHi_pf++, vl), vh), vm), va);
        *WHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*WHi_pf++, wl), wh), wm), wa);
        *SVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*SVi_pf++, sl), sh), sm), sa);
        *ZVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*ZVi_pf++, zl), zh), zm), za);
        *VVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*VVi_pf++, vl), vh), vm), va);
        *WVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*WVi_pf++, wl), wh), wm), wa);
        *Qo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*Qi_pf++, ql), qh), qm), qa);
        *Do_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*Di_pf++, dl), dh), dm), da);
        *Po_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*Pi_pf++, pl), ph), pm), pa);
        *Ko_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*Ki_pf++, kl), kh), km), ka);
        *LHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*LHi_pf++, ll), lh), lm), la);
        *LVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*LVi_pf++, ll), lh), lm), la);
        *RhoXHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*RhoXHi_pf++, ql), qh), qm), qa);
        *RhoXVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*RhoXVi_pf++, ql), qh), qm), qa);
        *PhiXHo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*PhiXHi_pf++, pl), ph), pm), pa);
        *PhiXVo_pf++ = _rk_mm_add(_rk_mm_mul(_rk_mm_min(_rk_mm_max(*PhiXVi_pf++, pl), ph), pm), pa);
        *Ro_pf++ = _rk_mm_min(_rk_mm_max(*Ri_pf++, rl), rh);
    }
    // Convert to uint8 type
    SHi = scratch->S[0];  uint8_t *shu = RKGetUInt8DataFromRay(ray, RKProductIndexSh);
    ZHi = scratch->Z[0];  uint8_t *zhu = RKGetUInt8DataFromRay(ray, RKProductIndexZ);
    VHi = scratch->V[0];  uint8_t *vhu = RKGetUInt8DataFromRay(ray, RKProductIndexV);
    WHi = scratch->W[0];  uint8_t *whu = RKGetUInt8DataFromRay(ray, RKProductIndexW);
    SVi = scratch->S[1];  uint8_t *svu = RKGetUInt8DataFromRay(ray, RKProductIndexSv);
    ZVi = scratch->Z[1];  uint8_t *zvu = RKGetUInt8DataFromRay(ray, RKProductIndexZv);
    VVi = scratch->V[1];  uint8_t *vvu = RKGetUInt8DataFromRay(ray, RKProductIndexVv);
    WVi = scratch->W[1];  uint8_t *wvu = RKGetUInt8DataFromRay(ray, RKProductIndexWv);
    QHi = scratch->Q[0];  uint8_t *qu = RKGetUInt8DataFromRay(ray, RKProductIndexQ);
    Di = scratch->ZDR;   uint8_t *du = RKGetUInt8DataFromRay(ray, RKProductIndexD);
    Pi = scratch->PhiDP; uint8_t *pu = RKGetUInt8DataFromRay(ray, RKProductIndexP);
    Ki = scratch->KDP;   uint8_t *ku = RKGetUInt8DataFromRay(ray, RKProductIndexK);
    Ri = scratch->RhoHV; uint8_t *ru = RKGetUInt8DataFromRay(ray, RKProductIndexR);
    LHi = scratch->L[0];  uint8_t *lhu = RKGetUInt8DataFromRay(ray, RKProductIndexLh);
    LVi = scratch->L[1];  uint8_t *lvu = RKGetUInt8DataFromRay(ray, RKProductIndexLv);
    RhoXHi = scratch->RhoXP[0];  uint8_t *rhu = RKGetUInt8DataFromRay(ray, RKProductIndexRXh);
    RhoXVi = scratch->RhoXP[1];  uint8_t *rvu = RKGetUInt8DataFromRay(ray, RKProductIndexRXv);
    PhiXHi = scratch->PhiXP[0];  uint8_t *phu = RKGetUInt8DataFromRay(ray, RKProductIndexPXh);
    PhiXVi = scratch->PhiXP[1];  uint8_t *pvu = RKGetUInt8DataFromRay(ray, RKProductIndexPXv);
    for (k = 0; k < ray->header.gateCount; k++) {
        *shu++ = *SHi++;
        *svu++ = *SVi++;
        if (isfinite(*ZHi)) {
            *zhu++ = (uint8_t)*ZHi;
            *vhu++ = (uint8_t)*VHi;
            *whu++ = (uint8_t)*WHi;
            *zvu++ = (uint8_t)*ZVi;
            *vvu++ = (uint8_t)*VVi;
            *wvu++ = (uint8_t)*WVi;
            *qu++ = (uint8_t)*QHi;
            *du++ = (uint8_t)*Di;
            *pu++ = (uint8_t)*Pi;
            *ku++ = (uint8_t)*Ki;
            *ru++ = (uint8_t)RKRho2Uint8(*Ri);
            *lhu++ = (uint8_t)*LHi;
            *lvu++ = (uint8_t)*LVi;
            *rhu++ = (uint8_t)*RhoXHi;
            *rvu++ = (uint8_t)*RhoXVi;
            *phu++ = (uint8_t)*PhiXHi;
            *pvu++ = (uint8_t)*PhiXVi;
        } else {
            // Uint8 = 0 = transparent color
            *zhu++ = 0;
            *vhu++ = 0;
            *whu++ = 0;
            *zvu++ = 0;
            *vvu++ = 0;
            *wvu++ = 0;
            *qu++ = 0;
            *du++ = 0;
            *pu++ = 0;
            *ku++ = 0;
            *ru++ = 0;
            *lhu++ = 0;
            *lvu++ = 0;
            *rhu++ = 0;
            *rvu++ = 0;
            *phu++ = 0;
            *pvu++ = 0;
        }
        ZHi++;
        VHi++;
        WHi++;
        ZVi++;
        VVi++;
        WVi++;
        QHi++;
        Di++;
        Pi++;
        Ki++;
        Ri++;
        LHi++;
        LVi++;
        RhoXHi++;
        RhoXVi++;
        PhiXHi++;
        PhiXVi++;
    }
    // If the space has been used for the same gateCount calculations, it should remain zero
    if (*ZHi != 0.0 || *zhu != 0) {
        memset(shu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(zhu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(vhu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(whu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(svu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(zvu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(vvu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(wvu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(qu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(du, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(pu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(ku, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(ru, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(lhu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(lvu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(rhu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(rvu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(phu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(pvu, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(uint8_t));
        memset(SVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(ZHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(VHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(WHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(SHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(ZVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(VVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(WVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(QHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(Di, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(Pi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(Ki, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(Ri, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(LHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(LVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(PhiXHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(PhiXVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(RhoXHi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        memset(RhoXVi, 0, (ray->header.capacity - ray->header.gateCount) * sizeof(RKFloat));
        ray->header.marker |= RKMarkerMemoryManagement;
    }
    ray->header.productList |= RKProductListUInt8ZVWDPRKSQ;
    // #endif

    if (scratch->fftOrder > 0) {
        ray->header.fftOrder = (uint8_t)scratch->fftOrder;
    }
    return k;
}

int RKNullProcessor(RKMomentScratch *scratch, RKPulse **pulses, const uint16_t count) {
    return 0;
}
