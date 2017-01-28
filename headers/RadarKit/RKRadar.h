//
//  RKRadar.h
//  RadarKit
//
//  Created by Boon Leng Cheong on 3/17/15.
//
//

#ifndef __RadarKit_RKRadar__
#define __RadarKit_RKRadar__

#include <RadarKit/RKFoundation.h>
#include <RadarKit/RKServer.h>
#include <RadarKit/RKClient.h>
#include <RadarKit/RKClock.h>
#include <RadarKit/RKConfig.h>
#include <RadarKit/RKPosition.h>
#include <RadarKit/RKPulseCompression.h>
#include <RadarKit/RKMoment.h>
#include <RadarKit/RKSweep.h>

typedef uint32_t RKRadarState;
enum RKRadarState {
    RKRadarStateBaseAllocated                        = 1,          // 0x01
    RKRadarStateRayBufferAllocating                  = (1 << 1),   // 0x02
    RKRadarStateRayBufferInitialized                 = (1 << 2),   // 0x04
    RKRadarStateRawIQBufferAllocating                = (1 << 3),   // 0x08
    RKRadarStateRawIQBufferInitialized               = (1 << 4),   // 0x10
    RKRadarStateConfigBufferAllocating               = (1 << 5),   // 0x20
    RKRadarStateConfigBufferIntialized               = (1 << 6),   // 0x40
    RKRadarStatePositionBufferAllocating             = (1 << 7),
    RKRadarStatePositionBufferInitialized            = (1 << 8),
    RKRadarStatePulseCompressionEngineInitialized    = (1 << 9),
    RKRadarStatePositionEngineInitialized            = (1 << 10),
    RKRadarStateMomentEngineInitialized              = (1 << 11),
    RKRadarStateSweepEngineInitialized               = (1 << 12),
    RKRadarStateTransceiverInitialized               = (1 << 16),
    RKRadarStatePedestalInitialized                  = (1 << 24),
    RKRadarStateLive                                 = (1 << 31)
};

/*!
 * @typedef RKRadar
 * @brief The big structure that holds all the necessary buffers
 * @param rawPulse
 */
typedef struct rk_radar RKRadar;
struct rk_radar {
    //
    // General buffers
    //
    char                       name[RKNameLength];
    //
    // Buffers
    //
    RKConfig                   *configs;
    RKPosition                 *positions;
    RKBuffer                   pulses;
    RKBuffer                   rays;
    //
    // Anchor indices of the buffers
    //
    uint32_t                   configIndex;
    uint32_t                   positionIndex;
    uint32_t                   pulseIndex;
    uint32_t                   rayIndex;
    //
    //
    RKRadarDesc                desc;
    RKRadarState               state;
    bool                       active;
    //
    size_t                     memoryUsage;
    //
    // Internal engines
    //
    RKClock                    *pulseClock;
    RKClock                    *positionClock;
    RKPulseCompressionEngine   *pulseCompressionEngine;
    RKPositionEngine           *positionEngine;
    RKMomentEngine             *momentEngine;
    RKSweepEngine              *sweepEngine;
    //
    pthread_t                  monitorThreadId;
    //
    // Hardware protocols for controls
    //
    RKTransceiver              transceiver;
    RKTransceiver              (*transceiverInit)(RKRadar *, void *);
    int                        (*transceiverExec)(RKTransceiver, const char *);
    int                        (*transceiverFree)(RKTransceiver);
    void                       *transceiverInitInput;
    pthread_t                  transceiverThreadId;
    //
    RKPedestal                 pedestal;
    RKPedestal                 (*pedestalInit)(RKRadar *, void *);
    int                        (*pedestalExec)(RKPedestal, const char *);
    int                        (*pedestalFree)(RKPedestal);
    void                       *pedestalInitInput;
    pthread_t                  pedestalThreadId;
    //
    RKHealthMonitor            healthMonitor;
    RKHealthMonitor            (*healthMonitorInit)(RKRadar *, void *);
    int                        (*healthMonitorExec)(RKHealthMonitor, const char *);
    int                        (*healthMonitorFree)(RKHealthMonitor);
    void                       *healthMonitorInitInput;
    pthread_t                  healthMonitorThreadId;
};

//
// Life Cycle
//

RKRadar *RKInitWithDesc(RKRadarDesc);
RKRadar *RKInitQuiet(void);
RKRadar *RKInitLean(void);
RKRadar *RKInitMean(void);
RKRadar *RKInitFull(void);
RKRadar *RKInit(void);
int RKFree(RKRadar *radar);

//
// Properties
//

// Set the transceiver. Pass in function pointers: init, exec and free
int RKSetTransceiver(RKRadar *radar,
                     void *initInput,
                     RKTransceiver initRoutine(RKRadar *, void *),
                     int execRoutine(RKTransceiver, const char *),
                     int freeRoutine(RKTransceiver));

// Set the pedestal. Pass in function pointers: init, exec and free
int RKSetPedestal(RKRadar *radar,
                  void *initInput,
                  RKPedestal initRoutine(RKRadar *, void *),
                  int execRoutine(RKPedestal, const char *),
                  int freeRoutine(RKPedestal));

// Some states of the radar
int RKSetVerbose(RKRadar *radar, const int verbose);

// Some operating parameters
int RKSetWaveform(RKRadar *radar, const char *filename, const int group, const int maxDataLength);
int RKSetWaveformToImpulse(RKRadar *radar);
int RKSetWaveformTo121(RKRadar *radar);
int RKSetProcessingCoreCounts(RKRadar *radar,
                              const unsigned int pulseCompressionCoreCount,
                              const unsigned int momentProcessorCoreCount);
int RKSetPRF(RKRadar *radar, const uint32_t prf);
uint32_t RKGetPulseCapacity(RKRadar *radar);


// If there is a tic count from firmware, use it as clean reference for time derivation
void RKSetPulseTicsPerSeconds(RKRadar *radar, const double delta);
void RKSetPositionTicsPerSeconds(RKRadar *radar, const double delta);

//
// Interactions
//

int RKGoLive(RKRadar *);
int RKWaitWhileActive(RKRadar *);
int RKStop(RKRadar *);

// Positions
RKPosition *RKGetVacantPosition(RKRadar *);
void RKSetPositionReady(RKRadar *, RKPosition *);

// Pulses
RKPulse *RKGetVacantPulse(RKRadar *);
void RKSetPulseHasData(RKRadar *, RKPulse *);
void RKSetPulseReady(RKRadar *, RKPulse *);

// Rays
RKRay *RKGetVacantRay(RKRadar *);
void RKSetRayReady(RKRadar *, RKRay *);

void RKAddConfig(RKRadar *radar, ...);

#endif /* defined(__RadarKit_RKRadar__) */
