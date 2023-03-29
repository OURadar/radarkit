
//
//  RKPositionSteerEngine.h
//  RadarKit
//
//  Created by Boonleng Cheong on 3/21/23.
//  Copyright (c) 2023 Boonleng Cheong. All rights reserved.
//
//  Most codes are ported from Ming-Duan Tze's VCP implementation
//

#ifndef __RadarKit_Position_Steer_Engine__
#define __RadarKit_Position_Steer_Engine__

#include <RadarKit/RKFoundation.h>
#include <RadarKit/RKConfig.h>
#include <RadarKit/RKDSP.h>

#define RKPedestalPositionTolerance    0.3f
#define RKPedestalVelocityTolerance    1.0f
#define RKPedestalPointTimeOut         1500
#define RKPedestalActionPeriod         2

typedef bool RKScanRepeat;
enum RKScanRepeat {
    RKScanRepeatNone                             = false,
    RKScanRepeatForever                          = true
};

typedef int RKPedestalAxis;
enum RKPedestalAxis {
    RKPedestalAxisElevation                      = 0,
    RKPedestalAxisAzimuth                        = 1
};

typedef int RKScanProgress;
enum RKScanProgress {
    RKScanProgressNone                           = 0,                          // Not active, ready for next sweep
    RKScanProgressSetup                          = 1,                          // Getting ready
    RKScanProgressReady                          = 1 << 1,                     // Ready for the next sweep (deprecating)
    RKScanProgressMiddle                         = 1 << 2,                     // Middle of a sweep
    RKScanProgressEnd                            = 1 << 3,                     // End of a sweep, waiting for pedestal to stop / reposition
    RKScanProgressMarker                         = 1 << 7                      // Sweep complete marker
};

typedef int RKScanOption;
enum RKScanOption {
    RKScanOptionNone                             = 0,
    RKScanOptionBrakeElevationDuringSweep        = 1,
    RKScanOptionRepeat                           = 1 << 1,
    RKScanOptionVerbose                          = 1 << 2
};

typedef int RKScanMode;
enum RKScanMode {
    RKScanModeNone
    RKScanModeRHI
    RKScanModeSector          // RKScanModeNewSector
    RKScanModePPI
    RKScanModePPIAzimuthStep
    RKScanModePPIContinuous   // deprecating
    RKScanModeNewSector       // deprccating
    RKScanModeSpeedDown       // need to learn more
};

typedef int RKScanHitter;
enum RKScanHitter {
    RKScanAtBat                                  = 0,                          // current vcp
    RKScanPinch                                  = 1,                          // vcp only once
    RKScanLine                                   = 2,                          // next vcp
};

typedef struct rk_scan_path {
    RKScanMode                  mode;                                          // Scan mode
    float                       azimuthStart;                                  // Azimuth start of a scan
    float                       azimuthEnd;                                    // Azimuth end of a scan
    float                       azimuthSlew;                                   // Azimuth slew speed
    float                       azimuthMark;                                   // Azimuth to mark end of scan
    float                       elevationStart;                                // Elevation start of a scan
    float                       elevationEnd;                                  // Elevation end of a scan
    float                       elevationSlew;                                 // Elevation slew speed
} RKScanPath;

typedef struct rk_scan_state {
    RKName                      name;
    RKScanOption                option;
    RKScanPath                  batterScans[RKMaximumScanCount];
    RKScanPath                  onDeckScans[RKMaximumScanCount];
    RKScanPath                  inTheHoleScans[RKMaximumScanCount];
    int                         inTheHoleCount;
    int                         onDeckCount;
    int                         sweepCount;
    bool                        active;
    int                         i;                                             // Sweep index for control
    int                         j;                                             // Sweep index for marker
    int                         tic;                                           // Counter of the run loop
    float                       elevationPrevious;                             //
    float                       azimuthPrevious;                               //
    // float                       counterTargetElevation;                        // Elevation control target  (deprecating)
    // float                       counterTargetAzimuth;                          // Azimuth control target    (deprecating)
    // float                       counterMarkerAzimuth;                          // Azimuth marker target     (deprecating)
    float                       targetElevation;                               //
    float                       targetAzimuth;                                 // Target of end sweep: counter or transition azimuth
    float                       markerAzimuth;                                 // Marker of end sweep
    // float                       sweepMarkerElevation;                          // Elevation marker   (deprecating)
    // float                       sweepElevation;                                // Elevation control  (deprecating)
    // float                       sweepAzimuth;                                  // Azimuth control    (deprecating)
    RKScanProgress              progress;                                      //
    RKScanAction                lastAction;                                    // store last action
} RKPedestalVcpHandle;

typedef struct rk_scan_control {
    RKName                      name;
    RKScanOption                option;
    RKScanPath                  batterScans[RKMaximumScanCount];
    RKScanPath                  onDeckScans[RKMaximumScanCount];
    RKScanPath                  inTheHoleScans[RKMaximumScanCount];
    int                         inTheHoleCount;
    int                         onDeckCount;
    int                         sweepCount;
    bool                        active;
    int                         i;                                             // Sweep index for control
    int                         j;                                             // Sweep index for marker
    int                         tic;                                           // Counter of the run loop
    float                       elevationPrevious;                             //
    float                       azimuthPrevious;                               //
    float                       targetElevation;                               //
    float                       targetAzimuth;                                 // Target of end sweep: counter or transition azimuth
    float                       markerAzimuth;                                 // Marker of end sweep
    RKScanProgress              progress;                                      //
    RKScanAction                lastAction;                                    // store last action
} RKScanControl;

typedef struct rk_position_steer_engine RKPositionSteerEngine;

struct rk_position_steer_engine {
    // User set variables
    RKName                 name;
    RKRadarDesc            *radarDescription;
    RKPosition             *positionBuffer;
    uint32_t               *positionIndex;
    RKConfig               *configBuffer;
    uint32_t               *configIndex;
    uint8_t                verbose;

    // Program set variables
    pthread_t              threadId;
    double                 startTime;
    RKPedestalVcpHandle    vcpHandle;
    RKScanControl          scanControl;
    int                    vcpIndex;              // deprecating
    int                    vcpSweepCount;         // deprecating
    struct timeval         currentTime;
    struct timeval         statusPeriod;
    struct timeval         statusTriggerTime;
    RKScanAction           actions[RKPedestalActionBufferDepth];
    int                    actionIndex;
    RKByte                 response[RKMaximumStringLength];

    // Status / health
    size_t                 memoryUsage;
    char                   statusBuffer[RKBufferSSlotCount][RKStatusStringLength];
    uint32_t               statusBufferIndex;
    RKEngineState          state;
    uint64_t               tic;
    float                  lag;
};

RKPositionSteerEngine *RKPositionSteerEngineInit(void);
void RKPositionSteerEngineFree(RKPositionSteerEngine *);

void RKPositionSteerEngineSetVerbose(RKPositionSteerEngine *, const int);
void RKPositionSteerEngineSetInputOutputBuffers(RKPositionSteerEngine *, const RKRadarDesc *,
                                                RKPosition *, uint32_t *,
                                                RKConfig *,   uint32_t *);

int RKPositionSteerEngineStart(RKPositionSteerEngine *);
int RKPositionSteerEngineStop(RKPositionSteerEngine *);

void RKPositionSteerEngineStopSweeps(RKPositionSteerEngine *);
void RKPositionSteerEngineClearSweeps(RKPositionSteerEngine *);
void RKPositionSteerEngineClearHole(RKPositionSteerEngine *);
void RKPositionSteerEngineClearDeck(RKPositionSteerEngine *);
void RKPositionSteerEngineNextHitter(RKPositionSteerEngine *);
void RKPositionSteerEngineArmSweeps(RKPositionSteerEngine *, const RKScanRepeat);
int RKPositionSteerEngineAddLineupSweep(RKPositionSteerEngine *, const RKScanPath);
int RKPositionSteerEngineAddPinchSweep(RKPositionSteerEngine *, const RKScanPath);

// void RKPositionSteerEngineUpdatePositionFlags(RKPositionSteerEngine *, RKPosition *);

RKScanAction *RKPositionSteerEngineGetActionV1(RKPositionSteerEngine *, RKPosition *);
RKScanAction *RKPositionSteerEngineGetAction(RKPositionSteerEngine *, RKPosition *);

int RKPositionSteerEngineExecuteString(RKPositionSteerEngine *, const char *, char *);

RKScanPath RKPositionSteerEngineMakeScanPath(RKScanMode,
                                             const float elevationStart, const float elevationEnd,
                                             const float azimuthStart, const float azimuthEnd, const float azimuthMark,
                                             const float rate);

void RKPositionSteerEngineScanSummary(RKPositionSteerEngine *, char *);

char *RKPositionSteerEngineStatusString(RKPositionSteerEngine *);

#endif
