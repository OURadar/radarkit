//
//  RKRadar.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 3/17/15.
//
//

#include <RadarKit/RKRadar.h>

RKRadar *RKInitWithFlags(const RKEnum flags);

//
//
//

#pragma mark -

RKRadar *RKInit(void) {
    return RKInitWithFlags(RKInitFlagAllocEverything);
}

RKRadar *RKInitWithFlags(const RKEnum flags) {
    RKRadar *radar;
    size_t bytes;

    // Allocate itself
    bytes = sizeof(RKRadar);
    if (posix_memalign((void **)&radar, RKSIMDAlignSize, bytes)) {
        fprintf(stderr, "Error allocation memory for radar.\n");
        return NULL;
    }
    memset(radar, 0, bytes);
    radar->memoryUsage += bytes;
    radar->state |= RKRadarStateBaseAllocated;

    // Copy over the input flags
    radar->initFlags = flags;

    // Set some non-zero variables
    radar->active = true;
    
    // Other allocatinos
    if (flags & RKInitFlagAllocMomentBuffer) {
        radar->state |= RKRadarStateRayBufferAllocating;
        bytes = RKBuffer2SlotCount * sizeof(RKInt16Ray);
        if (posix_memalign((void **)&radar->rays, RKSIMDAlignSize, bytes)) {
            RKLog("ERROR. Unable to allocate memory for rays");
            return NULL;
        }
        radar->memoryUsage += bytes;
        radar->state ^= RKRadarStateRayBufferAllocating;
        radar->state |= RKRadarStateRayBufferInitialized;
    }

    if (flags & RKInitFlagAllocRawIQBuffer) {
        radar->state |= RKRadarStateRawIQBufferAllocating;
        bytes = RKBuffer0SlotCount * sizeof(RKPulse);
        if (posix_memalign((void **)&radar->pulses, RKSIMDAlignSize, bytes)) {
            RKLog("ERROR. Unable to allocate memory for I/Q pulses");
            return NULL;
        }
        memset(radar->pulses, 0, bytes);
        radar->memoryUsage += bytes;
        radar->state ^= RKRadarStateRawIQBufferAllocating;
        radar->state |= RKRadarStateRawIQBufferInitialized;
        for (int i = 0; i < RKBuffer0SlotCount; i++) {
            radar->pulses[i].header.i = i - RKBuffer0SlotCount;
        }
    }

    radar->pulseCompressionEngine = RKPulseCompressionEngineInit();
    RKPulseCompressionEngineSetInputOutputBuffers(radar->pulseCompressionEngine, radar->pulses, &radar->index, RKBuffer0SlotCount);
    radar->state |= RKRadarStatePulseCompressionEngineInitialized;

    radar->socketServer = RKServerInit();
    RKServerSetCommandHandler(radar->socketServer, &socketCommandHandler);
    RKServerSetStreamHandler(radar->socketServer, &socketStreamHandler);
    radar->state |= RKRadarStateSocketServerInitialized;

    RKLog("Radar initialized\n");
    return radar;
}

int RKFree(RKRadar *radar) {
    if (radar->active) {
        RKStop(radar);
    }
    while (radar->state & RKRadarStateRayBufferAllocating) {
        usleep(1000);
    }
    if (radar->state & RKRadarStateRayBufferInitialized) {
        free(radar->rays);
    }
    while (radar->state & RKRadarStateRawIQBufferAllocating) {
        usleep(1000);
    }
    if (radar->state & RKRadarStateRawIQBufferInitialized) {
        free(radar->pulses);
    }
    free(radar);
    return EXIT_SUCCESS;
}

int RKGoLive(RKRadar *radar) {
    RKPulseCompressionEngineStart(radar->pulseCompressionEngine);
    RKServerActivate(radar->socketServer);
    return 0;
}

int RKStop(RKRadar *radar) {
    radar->active = false;
    if (radar->state & RKRadarStatePulseCompressionEngineInitialized) {
        RKPulseCompressionEngineStop(radar->pulseCompressionEngine);
    }
    if (radar->state & RKRadarStateSocketServerInitialized) {
        RKServerStop(radar->socketServer);
        RKServerWait(radar->socketServer);
    }
    return 0;
}

RKPulse *RKGetVacantPulse(RKRadar *radar) {
    if (radar->pulses == NULL) {
        RKLog("Error. Buffer for raw pulses has not been allocated.\n");
        exit(EXIT_FAILURE);
    }
    RKPulse *pulse = &radar->pulses[radar->index];
    pulse->header.s = RKPulseStatusVacant;
    pulse->header.i += RKBuffer0SlotCount;
    radar->index = RKNextBuffer0Slot(radar->index);
    return pulse;
}

void RKSetPulseReady(RKPulse *pulse) {
    pulse->header.s = RKPulseStatusReady;
}

// Function incomplete
int RKSetWaveform(RKRadar *radar, const char *filename, const int group, const int maxDataLength) {
    // Load in the waveform
    // Call a transceiver delegate function to fill in the DAC
    RKComplex filter[] = {{1.0f, 0.0f}};
    return RKPulseCompressionSetFilter(radar->pulseCompressionEngine, filter, 1, 0, maxDataLength, group, 0);
}

int RKSetWaveformToImpulse(RKRadar *radar) {
    return RKPulseCompressionSetFilterToImpulse(radar->pulseCompressionEngine);
}

int RKSetWaveformTo121(RKRadar *radar) {
    return RKPulseCompressionSetFilterTo121(radar->pulseCompressionEngine);
}
