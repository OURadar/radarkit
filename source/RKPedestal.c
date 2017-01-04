//
//  RKPedestal.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 1/3/17.
//  Copyright © 2017 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit/RKPedestal.h>

// Internal functions

void *pulseTagger(void *in);

// Implementations

#pragma mark -
#pragma mark Helper Functions

#pragma mark -
#pragma mark Threads

void *pulseTagger(void *in) {
    RKPedestalEngine *engine = (RKPedestalEngine *)in;

    uint32_t index = *engine->pulseIndex;

    RKLog("<pulseTagger> started.   mem = %s B   engine->index = %d\n", RKIntegerToCommaStyleString(engine->memoryUsage), index);

//    // Search until a time I need
//
//    // Wait until the latest position arrives
//    // find the latest position, tag the pulse with the appropriate position
//    // linearly interpolate between the best two readings
//    // at some point, implement something sophisticated like Klaman filter
//
    engine->state = RKPedestalEngineStateActive;

    // Set the pulse to have position
    while (engine->state == RKPedestalEngineStateActive) {
        while (index == *engine->pulseIndex) {
            usleep(10000);
        }
        // Get the latest pulse
        RKPulse *pulse = RKGetPulse(engine->pulseBuffer, index);
        while ((pulse->header.s & RKPulseStatusHasIQData) == 0) {
            usleep(1000);
        }

        printf("Tagging pulse %d / %d ...\n", pulse->header.i, index);
        index = RKNextModuloS(index, engine->pulseBufferSize);
    }

    return (void *)NULL;
}

#pragma mark -
#pragma mark Life Cycle

RKPedestalEngine *RKPedestalEngineInit() {
    RKPedestalEngine *engine = (RKPedestalEngine *)malloc(sizeof(RKPedestalEngine));
    memset(engine, 0, sizeof(RKPedestalEngine));
    engine->memoryUsage = sizeof(RKPedestalEngine);
    return engine;
}

void RKPedestalEngineFree(RKPedestalEngine *engine) {
    free(engine);
}

#pragma mark -
#pragma mark Properties

void RKPedestalEngineSetInputOutputBuffers(RKPedestalEngine *engine,
                                           RKBuffer buffer, uint32_t *index, const uint32_t size) {
    engine->pulseBuffer = buffer;
    engine->pulseIndex = index;
    engine->pulseBufferSize = size;
}

void RKPedestalEngineSetHardwareInit(RKPedestalEngine *engine, RKPedestal hardwareInit(void *), void *hardwareInitInput) {
    engine->hardwareInit = hardwareInit;
    engine->hardwareInitInput = hardwareInitInput;
}

void RKPedestalEngineSetHardwareExec(RKPedestalEngine *engine, int hardwareExec(RKPedestal, const char *)) {
    engine->hardwareExec = hardwareExec;
}

//void RKPedestalEngineSetHardwareRead(RKPedestalEngine *engine, int hardwareRead(RKPedestal, RKPosition *)) {
//    engine->hardwareRead = hardwareRead;
//}

void RKPedestalEngineSetHardwareFree(RKPedestalEngine *engine, int hardwareFree(RKPedestal)) {
    engine->hardwareFree = hardwareFree;
}

#pragma mark -
#pragma mark Interactions

int RKPedestalEngineStart(RKPedestalEngine *engine) {
    RKLog("Starting <pulseTagger> ...\n");
    if (pthread_create(&engine->threadId, NULL, pulseTagger, engine)) {
        RKLog("Error. Unable to start pedestal engine.\n");
        return RKResultFailedToStartPedestalWorker;
    }
    return RKResultNoError;
}

int RKPedestalEngineStop(RKPedestalEngine *engine) {
    RKLog("Stopping <pedestalWorker>\n");
    return RKResultNoError;
}
