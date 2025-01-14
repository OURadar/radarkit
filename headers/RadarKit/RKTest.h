//
//  RKTest.h
//  RadarKit
//
//  Created by Boonleng Cheong on 3/18/15.
//  Copyright (c) 2015 Boonleng Cheong. All rights reserved.
//

#ifndef __RadarKit_Test__
#define __RadarKit_Test__

#include <RadarKit/RKRadar.h>
#include <RadarKit/RKReporter.h>
#include <RadarKit/RKFileHeader.h>

#define RKTestWaveformCacheCount 2

typedef uint8_t RKTestFlag;
enum {
    RKTestFlagNone         = 0,
    RKTestFlagVerbose      = 1,
    RKTestFlagShowResults  = 1 << 1
};

typedef uint8_t RKTestSIMDFlag;
enum {
    RKTestSIMDFlagNull                       = 0,
    RKTestSIMDFlagShowNumbers                = 1,
    RKTestSIMDFlagPerformanceTestArithmetic  = 1 << 1,
    RKTestSIMDFlagPerformanceTestDuplicate   = 1 << 2,
    RKTestSIMDFlagPerformanceTestAll         = RKTestSIMDFlagPerformanceTestArithmetic | RKTestSIMDFlagPerformanceTestDuplicate
};

typedef uint8_t RKAxisAction;
enum {
    RKAxisActionStop,
    RKAxisActionSpeed,
    RKAxisActionPosition
};
typedef struct rk_test_transceiver {
    RKName         name;
    int            verbose;
    long           counter;

    int            sleepInterval;
    int            gateCapacity;
    int            gateCount;
    float          gateSizeMeters;
    double         fs;
    double         prt;
    RKByte         sprt;
    RKWaveform     *waveformCache[RKTestWaveformCacheCount];
    unsigned int   waveformCacheIndex;
	RKCommand      customCommand;
    bool           simFault;
    bool           transmitting;
    int            chunkSize;
    double         periodEven;
    double         periodOdd;
    long           ticEven;
    long           ticOdd;
    char           playbackFolder[RKMaximumFolderPathLength];
    RKFileHeader   fileHeaderCache;
    RKPulseHeader  pulseHeaderCache;

    pthread_t      tidRunLoop;
    RKEngineState  state;
    RKRadar        *radar;
    size_t         memoryUsage;
    char           response[RKMaximumStringLength];
} RKTestTransceiver;

typedef struct rk_test_pedestal {
    RKName         name;
    int            verbose;
    unsigned long  counter;

    float          azimuth;
    float          speedAzimuth;
    float          targetAzimuth;
    float          targetSpeedAzimuth;
    RKAxisAction   actionAzimuth;

    float          elevation;
    float          speedElevation;
    float          targetElevation;
    float          targetSpeedElevation;
    RKAxisAction   actionElevation;

    pthread_t      tidRunLoop;
    RKEngineState  state;
    RKRadar        *radar;
    size_t         memoryUsage;
    char           response[RKMaximumStringLength];
} RKTestPedestal;

typedef struct rk_test_health_relay {
    RKName         name;
    int            verbose;
    long           counter;

    pthread_t      tidRunLoop;
    RKEngineState  state;
    RKRadar        *radar;
    size_t         memoryUsage;
    char           response[RKMaximumStringLength];
} RKTestHealthRelay;

// Test functions

char *RKTestByNumberDescription(const int);
void RKTestByNumber(const int, const void *);

// Basic Tests

void RKTestShowTypes(void);
void RKTestTerminalColors(void);
void RKTestPrettyStrings(void);
void RKTestBasicMath(void);
void RKTestParseCommaDelimitedValues(void);
void RKTestParseJSONString(void);
void RKTestTemperatureToStatus(void);
void RKTestGetCountry(void);
void RKTestBufferOverviewText(const char *);
void RKTestHealthOverviewText(const char *);
void RKTestReviseLogicalValues(void);
void RKTestTimeConversion(void);

// File handling

void RKTestCountFiles(const char *);
void RKTestPreparePath(void);
void RKTestReadBareRKComplex(const char *);
void RKTestPreferenceReading(void);
void RKTestIQRead(const char *);
void RKTestSweepRead(const char *);
void RKTestProductRead(const char *);
void RKTestProductCollectionRead(const char *);
void RKTestProductWrite(void);
void RKTestProductWriteFromPlainToSweep(void);
void RKTestProductWriteFromPlainToProduct(void);
void RKTestProductWriteFromWDSS2ToProduct(const char *, const int);

// State machines

void RKTestFileManager(void);
void RKTestFileMonitor(void);
void RKTestHostMonitor(void);
void RKTestInitializingRadar(void);
void RKTestWebSocket(void);
void RKTestRadarHub(void);
void RKTestSimplePulseEngine(const int);
void RKTestSimpleMomentEngine(const int);

// DSP Tests

void RKTestSIMDBasic(void);
void RKTestSIMDComplex(void);
void RKTestSIMDComparison(const RKTestSIMDFlag, const int);
void RKTestSIMD(const RKTestSIMDFlag, const int);
void RKTestWindow(const int);
void RKTestHilbertTransform(void);
void RKTestWriteFFTWisdom(const int);
void RKTestRingFilterShowCoefficients(void);
void RKTestRamp(void);
void RKTestMakeHops(void);
void RKTestWaveformTFM(void);
void RKTestWaveformWrite(void);
void RKTestWaveformDownsampling(void);
void RKTestWaveformShowProperties(void);
void RKTestWaveformShowUserWaveformProperties(const char *filename);

// Numerical Tests

void RKTestHalfSingleDoubleConversion(void);
void RKTestPulseCompression(RKTestFlag);
void RKTestOnePulse(void);
void RKTestOneRay(int method(RKMomentScratch *, RKPulse **, const uint16_t), const int);
void RKTestOneRaySpectra(int method(RKMomentScratch *, RKPulse **, const uint16_t), const int lag);

// Performance Tests

void RKTestPulseCompressionSpeed(const int);
void RKTestPulseEngineSpeed(const int);
void RKTestMomentProcessorSpeed(void);
void RKTestCacheWrite(void);

// Transceiver Emulator

RKTransceiver RKTestTransceiverInit(RKRadar *, void *);
int RKTestTransceiverExec(RKTransceiver, const char *, char *);
int RKTestTransceiverFree(RKTransceiver);

// Pedestal Emulator

RKPedestal RKTestPedestalInit(RKRadar *, void *);
int RKTestPedestalExec(RKPedestal, const char *, char *);
int RKTestPedestalFree(RKPedestal);

// Health Relay Emulator

RKHealthRelay RKTestHealthRelayInit(RKRadar *, void *);
int RKTestHealthRelayExec(RKHealthRelay, const char *, char *);
int RKTestHealthRelayFree(RKHealthRelay);

//

void RKTestProductWriteFromPlainToProduct(void);

//

void RKTestCommandQueue(void);
void RKTestSingleCommand(void);
void RKTestExperiment(const char *);

#endif /* defined(__RadarKit_RKFile__) */
