//
//  RKPulseCompression.h
//  RadarKit
//
//  Created by Boon Leng Cheong on 3/18/15.
//  Copyright (c) 2015 Boon Leng Cheong. All rights reserved.
//

#ifndef __RadarKit_RKPulseCompression__
#define __RadarKit_RKPulseCompression__

#include <RadarKit/RKFoundation.h>
#include <fftw3.h>

#define RKPulseCompressionDFTPlanCount   4
#define RKMaxMatchedFilterCount          3   // Maximum filter count within each group
#define RKMaxMatchedFilterGroupCount     8   // Maximum filter group
#define RKWorkerDutyCycleBufferSize      1000

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef struct rk_filter_rect {
    int origin;
    int length;
    int maxDataLength;
} RKPulseCompressionFilterAnchor;

typedef struct rk_pulse_compression_worker RKPulseCompressionWorker;
typedef struct rk_pulse_compression_engine RKPulseCompressionEngine;

struct rk_pulse_compression_worker {
    int         id;
    int         planCount;
    int         planSizes[RKPulseCompressionDFTPlanCount];
    fftwf_plan  planInForward[RKPulseCompressionDFTPlanCount];
    fftwf_plan  planOutBackward[RKPulseCompressionDFTPlanCount];
    fftwf_plan  planFilterForward[RKMaxMatchedFilterGroupCount][RKMaxMatchedFilterCount][RKPulseCompressionDFTPlanCount];
    RKPulseCompressionEngine *parentEngine;
    double      dutyBuff[1000];
};

struct rk_pulse_compression_engine {
    RKPulse                          *pulses;
    uint32_t                         *index;
    uint32_t                         size;

    bool                             active;
    int                              verbose;

    unsigned int                     coreCount;
    pthread_t                        tidPulseWatcher;
    pthread_t                        tid[256];         // Thread ID
    uint32_t                         tic[256];         // Process count
    uint32_t                         pid[256];         // Latest processed index of pulses buffer
    double                           dutyCycle[256];   // Latest duty cycle estimate

    bool                             useSemaphore;
    char                             semaphoreName[256][16];

    uint32_t                         filterGroupCount;
    uint32_t                         filterCounts[RKMaxMatchedFilterGroupCount];
    RKComplex                        *filters[RKMaxMatchedFilterGroupCount][RKMaxMatchedFilterCount];
    RKPulseCompressionFilterAnchor   anchors[RKMaxMatchedFilterGroupCount][RKMaxMatchedFilterCount];
    RKPulseCompressionWorker         *workers;

    pthread_mutex_t                  coreMutex;
};

RKPulseCompressionEngine *RKPulseCompressionEngineInitWithCoreCount(const unsigned int count);
RKPulseCompressionEngine *RKPulseCompressionEngineInit(void);
int RKPulseCompressionEngineStart(RKPulseCompressionEngine *engine);
int RKPulseCompressionEngineStop(RKPulseCompressionEngine *engine);
void RKPulseCompressionEngineFree(RKPulseCompressionEngine *engine);
void RKPulseCompressionEngineSetInputOutputBuffers(RKPulseCompressionEngine *engine,
                                                   RKPulse *pulses,
                                                   uint32_t *index,
                                                   const uint32_t size);
int RKPulseCompressionSetFilterCountOfGroup(RKPulseCompressionEngine *engine, const int group, const int count);
int RKPulseCompressionSetFilterGroupCount(RKPulseCompressionEngine *engine, const int groupCount);
int RKPulseCompressionSetFilter(RKPulseCompressionEngine *engine,
                                const RKComplex *filter,
                                const int filterLength,
                                const int dataOrigin,
                                const int dataLength,
                                const int group,
                                const int index);
int RKPulseCompressionSetFilterToImpulse(RKPulseCompressionEngine *engine);
void RKPulseCompressionEngineLogStatus(RKPulseCompressionEngine *engine);

//#ifdef __cplusplus
//}
//#endif

#endif /* defined(__RadarKit_RKPulseCompression__) */
