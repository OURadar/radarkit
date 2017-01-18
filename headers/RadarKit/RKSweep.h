//
//  RKSweep.h
//  RadarKit
//
//  Created by Boon Leng Cheong on 1/15/17.
//  Copyright © 2017 Boon Leng Cheong. All rights reserved.
//

#ifndef __RadarKit_RKSweep__
#define __RadarKit_RKSweep__

#include <RadarKit/RKFoundation.h>
#include <netcdf.h>

typedef uint32_t RKSweepEngineState;
enum RKSweepEngineState {
    RKSweepEngineStateNull           = 0,
    RKSweepEngineStateAllocated      = 1,
    RKSweepEngineStateActive         = (1 << 1),
    RKSweepEngineStateWritingFile    = (1 << 2)
};

typedef struct rk_sweep {
    RKRay                 *rays[RKMaxRaysPerSweep];
    uint32_t              count;
} RKSweep;

typedef struct rk_sweep_engine RKSweepEngine;

struct rk_sweep_engine {
    // User set variables
    char                   name[RKNameLength];
    RKBuffer               rayBuffer;
    uint32_t               *rayIndex;
    uint32_t               rayBufferDepth;
    RKConfig               *configBuffer;
    uint32_t               *configIndex;
    uint32_t               configBufferDepth;
    RKRadarDesc            *radarDescription;
    
    uint8_t                verbose;
    
    // Program set variables
    pthread_t              tidRayGatherer;
    RKSweep                sweep;
    
    // Status / health
    uint32_t               processedRayIndex;
    char                   statusBuffer[RKBufferSSlotCount][RKMaximumStringLength];
    uint32_t               statusBufferIndex;
    RKSweepEngineState     state;
    uint32_t               tic;
    float                  lag;
    uint32_t               almostFull;
    size_t                 memoryUsage;
};

RKSweepEngine *RKSweepEngineInit(void);
void RKSweepEngineFree(RKSweepEngine *);

void RKSweepEngineSetVerbose(RKSweepEngine *, const int verbose);
void RKSweepEngineSetInputBuffer(RKSweepEngine *, RKRadarDesc *,
                                 RKConfig *, uint32_t *, const uint32_t,
                                 RKBuffer, uint32_t *, const uint32_t);

int RKSweepEngineStart(RKSweepEngine *);
int RKSweepEngineStop(RKSweepEngine *);

#endif /* RKSweep_h */