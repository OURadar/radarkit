//
//  RKClock.h
//  RadarKit
//
//  Created by Boon Leng Cheong on 1/5/17.
//  Copyright © 2017 Boon Leng Cheong. All rights reserved.
//

#ifndef __RadarKit_RKClock_h__
#define __RadarKit_RKClock_h__

#include <RadarKit/RKFoundation.h>

// A clock derived from counter and request time
#define RKClockDefaultBufferSize         1024
#define RKClockDefaultStride             100
#define RKClockAWhile                    2.0

typedef struct rk_clock {
    // User set parameters
    double           offsetSeconds;
    char             name[RKMaximumStringLength];
    int              verbose;
    bool             autoSync;
    bool             hasWisdom;                   // User provided dudt
    uint32_t         size;                        // User changeable depth
    uint32_t         stride;                      // Size to compute average

    // Program set parameters
    struct timeval   *tBuffer;                    // The time which a request was made (dirty)
    double           *xBuffer;                    // A double representation of timeval (dirty)
    double           *uBuffer;                    // Driving reference (clean)
    
    uint32_t         index;
    uint64_t         count;
    double           initTime;
    double           latestTime;
    double           typicalPeriod;
    double           x0;
    double           u0;
    double           dxdu;
    
} RKClock;

RKClock *RKClockInitWithSize(const uint32_t, const uint32_t);
RKClock *RKClockInitWitName(const char *);
RKClock *RKClockInit(void);
void RKClockFree(RKClock *);

void RKClockSetName(RKClock *, const char *);
void RKClockSetVerbose(RKClock *, const int);
void RKClockSetManualSync(RKClock *clock);
void RKClockSetOffset(RKClock *, const double);
void RKClockSetDxDu(RKClock *, const double);

void RKClockSync(RKClock *clock, const double u);

double RKClockGetTime(RKClock *, const double, struct timeval *);
double RKClockGetTimeSinceInit(RKClock *, const double);

#endif /* __RadarKit_RKClock_h__ */