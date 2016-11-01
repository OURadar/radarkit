//
//  RKTest.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 10/25/16.
//  Copyright (c) 2016 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit/RKTest.h>

#define RKFMT  "%4d"

void RKTestModuloMath(void) {
    int k;
    const int N = 4;

    RKLog("Test with SlotCount = %d, N = %d\n", RKBuffer0SlotCount, N);
    k = 0;                      RKLog("k = " RKFMT " --> Next N = " RKFMT "\n", k, RKNextNModuloS(k, N, RKBuffer0SlotCount));
    k = RKBuffer0SlotCount - 4; RKLog("k = " RKFMT " --> Next N = " RKFMT "\n", k, RKNextNModuloS(k, N, RKBuffer0SlotCount));
    k = RKBuffer0SlotCount - 3; RKLog("k = " RKFMT " --> Next N = " RKFMT "\n", k, RKNextNModuloS(k, N, RKBuffer0SlotCount));
    k = RKBuffer0SlotCount - 2; RKLog("k = " RKFMT " --> Next N = " RKFMT "\n", k, RKNextNModuloS(k, N, RKBuffer0SlotCount));
    k = RKBuffer0SlotCount - 1; RKLog("k = " RKFMT " --> Next N = " RKFMT "\n", k, RKNextNModuloS(k, N, RKBuffer0SlotCount));
    k = RKBuffer0SlotCount - 1; RKLog("k = " RKFMT " --> Prev N = " RKFMT "\n", k, RKPreviousNModuloS(k, N, RKBuffer0SlotCount));
    k = 0;                      RKLog("k = " RKFMT " --> Prev N = " RKFMT "\n", k, RKPreviousNModuloS(k, N, RKBuffer0SlotCount));
    k = 1;                      RKLog("k = " RKFMT " --> Prev N = " RKFMT "\n", k, RKPreviousNModuloS(k, N, RKBuffer0SlotCount));
    k = 2;                      RKLog("k = " RKFMT " --> Prev N = " RKFMT "\n", k, RKPreviousNModuloS(k, N, RKBuffer0SlotCount));
    k = 3;                      RKLog("k = " RKFMT " --> Prev N = " RKFMT "\n", k, RKPreviousNModuloS(k, N, RKBuffer0SlotCount));
    k = 4;                      RKLog("k = " RKFMT " --> Prev N = " RKFMT "\n", k, RKPreviousNModuloS(k, N, RKBuffer0SlotCount));
    
    k = 4899;                   RKLog("k = " RKFMT " --> Next N = " RKFMT "\n", k, RKNextNModuloS(k, 100 - 1, RKBuffer0SlotCount));
}

void RKTestSimulateDataStream(RKRadar *radar, const int prf) {
    int k;
    float phi = 0.0f;
    float tau = 0.0f;
    float azimuth = 0.0f;
    struct timeval t0, t1;
    double dt = 0.0;
    double prt;
    int g = 0;
    const int chunkSize = 500;

    // Set to default value if prf = 0
    if (prf == 0) {
        prt = 0.0002;
    } else {
        prt = 1.0 / (double)prf;
    }
    
    gettimeofday(&t0, NULL);

    RKSetLogfile(NULL);

    //const int gateCount = (int)(75.0e3f / 3.0f);

    float fs = 50.0e6;
    const int gateCount = (int)(75.0e3 / 3.0e8 * fs * 2.0);

    RKLog("Using fs = %s MHz   PRF = %s Hz   gate count = %s (75 km)\n",
          RKFloatToCommaStyleString(1.0e-6 * fs),
          RKIntegerToCommaStyleString(prf),
          RKIntegerToCommaStyleString(gateCount));

    while (radar->active) {

        RKPulseCompressionEngineLogStatus(radar->pulseCompressionEngine);

        for (int j = 0; radar->active && j < chunkSize; j++) {
            RKPulse *pulse = RKGetVacantPulse(radar);
            // Fill in the data...
            pulse->header.gateCount = gateCount;
            pulse->header.azimuthDegrees = azimuth;
            pulse->header.elevationDegrees = 2.41f;
            for (k = 0; k < 1000; k++) {
                pulse->X[0][k].i = (int16_t)(32767.0f * cosf(phi * (float)k));
                pulse->X[0][k].q = (int16_t)(32767.0f * sinf(phi * (float)k));
            }
            phi += 0.02f;
            azimuth = fmodf(25.0f * tau, 360.0f);

            RKSetPulseReady(pulse);

            tau += prt;
        }

        // Wait to simulate the PRF
        g = 0;
        do {
            gettimeofday(&t1, NULL);
            dt = RKTimevalDiff(t1, t0);
            usleep(1000);
            g++;
        } while (radar->active && dt < prt * chunkSize);
        t0 = t1;
    }
}

void RKTestPulseCompression(RKRadar *radar, RKTestFlag flag) {
    RKPulse *pulse = RKGetVacantPulse(radar);
    pulse->header.gateCount = 6;

    RKInt16 *X = radar->pulses[0].X[0];
    memset(X, 0, pulse->header.gateCount * sizeof(RKComplex));
    pulse->X[0][0].i = 1;
    pulse->X[0][0].q = 0;
    pulse->X[0][1].i = 2;
    pulse->X[0][1].q = 0;
    pulse->X[0][2].i = 1;
    pulse->X[0][2].q = 0;
    RKSetPulseReady(pulse);

    while ((pulse->header.s & RKPulseStatusCompressed) == 0) {
        usleep(1000);
    }

    RKComplex *F = &radar->pulseCompressionEngine->filters[0][0][0];
    RKComplex *Y = radar->pulses[0].Y[0];

    if (flag & RKTestFlagShowResults) {
        printf("X =                    F =                           Y =\n");
        for (int k = 0; k < 8; k++) {
            printf("    %6d %s %6di         %9.2f %s %9.2fi         %9.2f %s %9.2fi\n",
                   X[k].i, X[k].q < 0 ? "-" : "+", abs(X[k].q),
                   F[k].i, F[k].q < 0.0f ? "-" : "+", fabs(F[k].q),
                   Y[k].i, Y[k].q < 0.0f ? "-" : "+", fabs(Y[k].q));
        }
    }
}
