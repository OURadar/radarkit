//
//  RKPulseRingFilter.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 11/11/17.
//  Copyright (c) 2015-2018 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit/RKPulseRingFilter.h>

#pragma mark - Helper Functions

static void RKPulseRingFilterUpdateStatusString(RKPulseRingFilterEngine *engine) {
    int i, c;
    char *string = engine->statusBuffer[engine->statusBufferIndex];

    // Always terminate the end of string buffer
    string[RKMaximumStringLength - 1] = '\0';
    string[RKMaximumStringLength - 2] = '#';
    
    // Use RKStatusBarWidth characters to draw a bar
    i = *engine->pulseIndex * RKStatusBarWidth / engine->radarDescription->pulseBufferDepth;
    memset(string, '.', RKStatusBarWidth);
    string[i] = 'R';

    // Engine lag
    i = RKStatusBarWidth + snprintf(string + RKStatusBarWidth, RKMaximumStringLength - RKStatusBarWidth, " | %s%02.0f%s |",
                                    rkGlobalParameters.showColor ? RKColorLag(engine->lag) : "",
                                    99.49f * engine->lag,
                                    rkGlobalParameters.showColor ? RKNoColor : "");
    
    RKPulseRingFilterWorker *worker;

    // Lag from each core
    for (c = 0; c < engine->coreCount; c++) {
        worker = &engine->workers[c];
        i += snprintf(string + i, RKMaximumStringLength - i, " %s%02.0f%s",
                      rkGlobalParameters.showColor ? RKColorLag(worker->lag) : "",
                      99.49f * worker->lag,
                      rkGlobalParameters.showColor ? RKNoColor : "");
    }
    // Put a separator
    i += snprintf(string + i, RKMaximumStringLength - i, " |");
    // Duty cycle of each core
    for (c = 0; c < engine->coreCount && i < RKMaximumStringLength - 13; c++) {
        worker = &engine->workers[c];
        i += snprintf(string + i, RKMaximumStringLength - i, " %s%02.0f%s",
                      rkGlobalParameters.showColor ? RKColorDutyCycle(worker->dutyCycle) : "",
                      99.49f * worker->dutyCycle,
                      rkGlobalParameters.showColor ? RKNoColor : "");
    }
    // Almost full count
    i += snprintf(string + i, RKMaximumStringLength - i, " [%d]", engine->almostFull);
    if (i > RKMaximumStringLength - 13) {
        memset(string + i, '#', RKMaximumStringLength - i - 1);
    }
    engine->statusBufferIndex = RKNextModuloS(engine->statusBufferIndex, RKBufferSSlotCount);
}

#pragma mark - Delegate Workers

static void *ringFilterCore(void *_in) {
    RKPulseRingFilterWorker *me = (RKPulseRingFilterWorker *)_in;
    RKPulseRingFilterEngine *engine = me->parentEngine;

    int g, i, k, p;
    struct timeval t0, t1, t2;

    const int c = me->id;
    const int ci = engine->radarDescription->initFlags & RKInitFlagManuallyAssignCPU ? engine->coreOrigin + c : -1;

    // Find the semaphore
    sem_t *sem = sem_open(me->semaphoreName, O_RDWR);
    if (sem == SEM_FAILED) {
        RKLog("Error. Unable to retrieve semaphore %d\n", c);
        return (void *)RKResultFailedToRetrieveSemaphore;
    };

    // Initiate a variable to store my name
    RKName name;
    if (rkGlobalParameters.showColor) {
        pthread_mutex_lock(&engine->mutex);
        k = snprintf(name, RKNameLength - 1, "%s", rkGlobalParameters.showColor ? RKGetColor() : "");
        pthread_mutex_unlock(&engine->mutex);
    } else {
        k = 0;
    }
    if (engine->coreCount > 9) {
        k += sprintf(name + k, "C%02d", c);
    } else {
        k += sprintf(name + k, "C%d", c);
    }
    if (rkGlobalParameters.showColor) {
        sprintf(name + k, RKNoColor);
    }

#if defined(_GNU_SOURCE)
    
    if (engine->radarDescription->initFlags & RKInitFlagManuallyAssignCPU) {
        // Set my CPU core
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(ci, &cpuset);
        sched_setaffinity(0, sizeof(cpuset), &cpuset);
        pthread_setaffinity_np(me->tid, sizeof(cpu_set_t), &cpuset);
    }
    
#endif

    RKPulse *pulse;
    size_t mem = 0;
    
    if (me->dataPath.origin % RKSIMDAlignSize > 0 || me->dataPath.length % RKSIMDAlignSize > 0) {
        RKLog("%s %s Error. Each filter line path must align to SIMD requirements.\n", engine->name, name);
        return NULL;
    }
    // Allocate local resources, use k to keep track of the total allocation
    // Each block is depth x pols (2) x gates (me->dataPath.length)
    RKIQZ x;
    RKIQZ y;
    RKIQZ b;
    RKIQZ a;
    const int depth = 8;
    size_t bytes = depth * me->dataPath.length * sizeof(RKFloat);
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&x.i, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&x.q, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&y.i, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&y.q, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&b.i, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&b.q, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&a.i, RKSIMDAlignSize, bytes));
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&a.q, RKSIMDAlignSize, bytes));
    mem += 8 * bytes;
    memset(x.i, 0, bytes);
    memset(x.q, 0, bytes);
    memset(y.i, 0, bytes);
    memset(y.q, 0, bytes);
    memset(b.i, 0, bytes);
    memset(b.q, 0, bytes);
    memset(a.i, 0, bytes);
    memset(a.q, 0, bytes);
    
    // Duplicate the filter coefficients to vectors
    i = 0;
    for (k = 0; k < engine->filter.bLength; k++) {
        RKFloat *bi = &b.i[k * me->dataPath.length];
        RKFloat *bq = &b.q[k * me->dataPath.length];
        RKFloat *ai = &a.i[k * me->dataPath.length];
        RKFloat *aq = &a.q[k * me->dataPath.length];
        for (g = 0; g < me->dataPath.length; g++) {
            *bi++ = engine->filter.B[k].i;
            *bq++ = engine->filter.B[k].q;
            *ai++ = engine->filter.A[k].i;
            *aq++ = engine->filter.A[k].q;
        }
    }

    double *busyPeriods, *fullPeriods;
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&busyPeriods, RKSIMDAlignSize, RKWorkerDutyCycleBufferDepth * sizeof(double)))
    POSIX_MEMALIGN_CHECK(posix_memalign((void **)&fullPeriods, RKSIMDAlignSize, RKWorkerDutyCycleBufferDepth * sizeof(double)))
    if (busyPeriods == NULL || fullPeriods == NULL) {
        RKLog("Error. Unable to allocate resources for duty cycle calculation\n");
        exit(EXIT_FAILURE);
    }
    mem += 2 * RKWorkerDutyCycleBufferDepth * sizeof(double);
    memset(busyPeriods, 0, RKWorkerDutyCycleBufferDepth * sizeof(double));
    memset(fullPeriods, 0, RKWorkerDutyCycleBufferDepth * sizeof(double));
    double allBusyPeriods = 0.0, allFullPeriods = 0.0;

    // Initialize some end-of-loop variables
    gettimeofday(&t0, NULL);
    gettimeofday(&t2, NULL);

    // The last index of the pulse buffer
    uint32_t i0 = engine->radarDescription->pulseBufferDepth - 1;

    // The latest index in the dutyCycle buffer
    int d0 = 0;

    // Log my initial state
    pthread_mutex_lock(&engine->mutex);
    engine->memoryUsage += mem;
    
    RKLog(">%s %s Started.   mem = %s B   origin = %s   length = %s   ci = %d\n",
          engine->name, name,
          RKUIntegerToCommaStyleString(mem),
          RKIntegerToCommaStyleString(me->dataPath.origin),
          RKIntegerToCommaStyleString(me->dataPath.length),
          ci);
    
    if (engine->verbose > 2) {
        RKShowArray(b.i, "Bi", me->dataPath.length, depth);
        printf("\n");
        RKShowArray(b.q, "Bq", me->dataPath.length, depth);
        printf("\n");
        RKShowArray(a.i, "Ai", me->dataPath.length, depth);
        printf("\n");
        RKShowArray(a.q, "Aq", me->dataPath.length, depth);
        printf("\n");
    }

    pthread_mutex_unlock(&engine->mutex);

    // Increase the tic once to indicate this processing core is created.
    me->tic++;
    
    //
    // free   busy       free   busy
    // .......|||||||||||.......|||||||||
    // t2 --- t1 --- t0/t2 --- t1 --- t0
    //        [ t0 - t1 ]
    // [    t0 - t2     ]
    //
    uint64_t tic = me->tic;

    k = 0;    // pulse index
    
    while (engine->state & RKEngineStateWantActive) {
        if (engine->useSemaphore) {
            #ifdef DEBUG_IQ
            RKLog(">%s sem_wait()\n", coreName);
            #endif
            if (sem_wait(sem)) {
                RKLog("%s %s Error. Failed in sem_wait(). errno = %d\n", engine->name, name, errno);
            }
        } else {
            while (tic == me->tic && engine->state & RKEngineStateWantActive) {
                usleep(1000);
            }
            tic = me->tic;
        }
        if (!(engine->state & RKEngineStateWantActive)) {
            break;
        }
        
        // Something happened
        gettimeofday(&t1, NULL);
        
        // Start of getting busy
        i0 = RKNextModuloS(i0, engine->radarDescription->pulseBufferDepth);

        pulse = RKGetPulse(engine->pulseBuffer, i0);
		if (!(pulse->header.s & RKPulseStatusRingInspected)) {
			fprintf(stderr, "This should not happen.   i0 = %d\n", i0);
		}

        // Now we do the work
        // Should only focus on the tasked range bins
        //
		if (engine->workerTaskDone[i0 * engine->coreCount + c] != false) {
			fprintf(stderr, "Already done?   i0 = %d\n", i0);
		}

        if (c == 0) {
            for (p = 0; p < 1; p++) {
                // Store x[n] at index k
                RKIQZ Z = RKGetSplitComplexDataFromPulse(pulse, 0);
                memcpy(x.i, Z.i + me->dataPath.origin, me->dataPath.length * sizeof(RKFloat));
                memcpy(x.q, Z.q + me->dataPath.origin, me->dataPath.length * sizeof(RKFloat));
                RKLog("%s %s  %d -> [%02u] = %.2f %.2f   %.2f %.2f  %.2f %.2f ... (%d)\n", engine->name, name, i0, me->dataPath.origin,
                      x.i[0], x.q[0], x.i[1], x.q[1], x.i[2], x.q[2], me->dataPath.length);
//                RKShowArray(Z.i, "Zi", 8, 1);
//                RKShowArray(Z.q, "Zq", 8, 1);
//                RKLog("%s %s  %d -> [%02u] = %.1f %.1f   %.1f %.1f   %.1f %.1f ... (%d)\n", engine->name, name, i0, me->linePath.origin,
//                      Z.i[0], Z.q[0],
//                      Z.i[1], Z.q[1],
//                      Z.i[2], Z.q[2], me->linePath.length);
            }
        }
        k = RKNextModuloS(k, depth);
        
        // The task for this core is now done at this point
        engine->workerTaskDone[i0 * engine->coreCount + c] = true;

        #ifdef DEBUG_IQ
        RKLog(">%s i0 = %d  stat = %d\n", coreName, i0, input->header.s);
        #endif

        // Record down the latest processed pulse index
        me->pid = i0;
        me->lag = fmodf((float)(*engine->pulseIndex + engine->radarDescription->pulseBufferDepth - me->pid) / engine->radarDescription->pulseBufferDepth, 1.0f);

        // Done processing, get the time
        gettimeofday(&t0, NULL);
        
        // Drop the oldest reading, replace it, and add to the calculation
        allBusyPeriods -= busyPeriods[d0];
        allFullPeriods -= fullPeriods[d0];
        busyPeriods[d0] = RKTimevalDiff(t0, t1);
        fullPeriods[d0] = RKTimevalDiff(t0, t2);
        allBusyPeriods += busyPeriods[d0];
        allFullPeriods += fullPeriods[d0];
        d0 = RKNextModuloS(d0, RKWorkerDutyCycleBufferDepth);
        me->dutyCycle = allBusyPeriods / allFullPeriods;

        t2 = t0;
    }

    // Clean up
    if (engine->verbose > 1) {
        RKLog("%s %s Freeing reources ...\n", engine->name, name);
    }
    
    free(x.i);
    free(x.q);
    free(y.i);
    free(y.q);
    free(b.i);
    free(b.q);
    free(a.i);
    free(a.q);
    free(busyPeriods);
    free(fullPeriods);

    RKLog(">%s %s Stopped.\n", engine->name, name);

    return NULL;
}

static void *pulseRingWatcher(void *_in) {
    RKPulseRingFilterEngine *engine = (RKPulseRingFilterEngine *)_in;
    
    int c, i, j, k, s;
	struct timeval t0, t1;
	float lag;

	sem_t *sem[engine->coreCount];
    
    unsigned int skipCounter = 0;

    bool allDone;
    bool *workerTaskDone;
    
    if (engine->coreCount == 0) {
        RKLog("Error. No processing core?\n");
        return NULL;
    }
    
    RKPulse *pulse;
    RKPulse *pulseToSkip;

	// Filter status of each worker: the beginning of the buffer is a pulse, it has the capacity info
    engine->workerTaskDone = (bool *)malloc(engine->radarDescription->pulseBufferDepth * engine->coreCount * sizeof(bool));
    memset(engine->workerTaskDone, 0, engine->radarDescription->pulseBufferDepth * engine->coreCount * sizeof(bool));
    
	// Update the engine state
    engine->state |= RKEngineStateWantActive;
    engine->state ^= RKEngineStateActivating;

    // Spin off N workers to process I/Q pulses
    memset(sem, 0, engine->coreCount * sizeof(sem_t *));
    uint32_t paddedGateCount = ((int)ceilf((float)engine->gateCount / RKSIMDAlignSize) * RKSIMDAlignSize);
    uint32_t length = paddedGateCount / engine->coreCount;
    uint32_t origin = 0;
    for (c = 0; c < engine->coreCount; c++) {
        RKPulseRingFilterWorker *worker = &engine->workers[c];
        snprintf(worker->semaphoreName, 32, "rk-cf-%03d", c);
        sem[c] = sem_open(worker->semaphoreName, O_CREAT | O_EXCL, 0600, 0);
        if (sem[c] == SEM_FAILED) {
            if (engine->verbose > 1) {
                RKLog(">%s Info. Semaphore %s exists. Try to remove and recreate.\n", engine->name, worker->semaphoreName);
            }
            if (sem_unlink(worker->semaphoreName)) {
                RKLog(">%s Error. Unable to unlink semaphore %s.\n", engine->name, worker->semaphoreName);
            }
            // 2nd trial
            sem[c] = sem_open(worker->semaphoreName, O_CREAT | O_EXCL, 0600, 0);
            if (sem[c] == SEM_FAILED) {
                RKLog(">%s Error. Unable to remove then create semaphore %s\n", engine->name, worker->semaphoreName);
                return (void *)RKResultFailedToInitiateSemaphore;
            } else if (engine->verbose > 1) {
                RKLog(">%s Info. Semaphore %s removed and recreated.\n", engine->name, worker->semaphoreName);
            }
        }
        worker->id = c;
        worker->sem = sem[c];
        worker->parentEngine = engine;
        worker->dataPath.origin = origin;
        if (c == engine->coreCount - 1) {
            worker->dataPath.length = paddedGateCount - origin;
        } else {
            worker->dataPath.length = length;
        }
        origin += length;
        if (engine->verbose > 1) {
            RKLog(">%s %s @ %p\n", engine->name, worker->semaphoreName, worker->sem);
        }
        if (pthread_create(&worker->tid, NULL, ringFilterCore, worker) != 0) {
            RKLog(">%s Error. Failed to start a ring core.\n", engine->name);
            return (void *)RKResultFailedToStartRingCore;
        }
    }

    // Wait for the workers to increase the tic count once
    engine->state |= RKEngineStateSleep0;
    for (c = 0; c < engine->coreCount; c++) {
        while (engine->workers[c].tic == 0) {
            usleep(1000);
        }
    }
    engine->state ^= RKEngineStateSleep0;
    engine->state |= RKEngineStateActive;

    RKLog("%s Started.   mem = %s B   pulseIndex = %d\n", engine->name, RKUIntegerToCommaStyleString(engine->memoryUsage), *engine->pulseIndex);

	// Increase the tic once to indicate the engine is ready
	engine->tic = 1;

    gettimeofday(&t1, NULL); t1.tv_sec -= 1;

    // Here comes the busy loop
    // i  anonymous
	// c  core index
    j = 0;   // filtered pulse index
    k = 0;   // pulse index
    while (engine->state & RKEngineStateWantActive) {
        // The pulse
        pulse = RKGetPulse(engine->pulseBuffer, k);

        // Wait until the engine index move to the next one for storage, which is also the time pulse has data.
        engine->state |= RKEngineStateSleep1;
        s = 0;
        while (k == *engine->pulseIndex && engine->state & RKEngineStateWantActive) {
            usleep(200);
            if (++s % 1000 == 0 && engine->verbose > 1) {
                RKLog("%s sleep 1/%.1f s   k = %d   pulseIndex = %d   header.s = 0x%02x\n",
                      engine->name, (float)s * 0.0002f, k , *engine->pulseIndex, pulse->header.s);
            }
        }
        engine->state ^= RKEngineStateSleep1;
        engine->state |= RKEngineStateSleep2;
        // Wait until the pulse has has been processed (compressed or skipped) so that this engine won't compete with the pulse compression engine to set the status.
        s = 0;
        while (!(pulse->header.s & RKPulseStatusProcessed) && engine->state & RKEngineStateWantActive) {
            usleep(200);
            if (++s % 1000 == 0 && engine->verbose > 1) {
                RKLog("%s sleep 2/%.1f s   k = %d   pulseIndex = %d   header.s = 0x%02x\n",
                      engine->name, (float)s * 0.0002f, k , *engine->pulseIndex, pulse->header.s);
            }
        }
        engine->state ^= RKEngineStateSleep2;
        
        if (!(engine->state & RKEngineStateWantActive)) {
            break;
        }

        // Lag of the engine
        engine->lag = fmodf(((float)*engine->pulseIndex + engine->radarDescription->pulseBufferDepth - k) / engine->radarDescription->pulseBufferDepth, 1.0f);

        // Assess the lag of the workers
        lag = engine->workers[0].lag;
        for (i = 1; i < engine->coreCount; i++) {
            lag = MAX(lag, engine->workers[i].lag);
        }
        if (skipCounter == 0 && lag > 0.9f) {
            engine->almostFull++;
            skipCounter = engine->radarDescription->pulseBufferDepth / 10;
            RKLog("%s Warning. Projected an I/Q Buffer overflow.\n", engine->name);
            i = *engine->pulseIndex;
            do {
                i = RKPreviousModuloS(i, engine->radarDescription->pulseBufferDepth);
                // Have some way to skip processing
                pulseToSkip = RKGetPulse(engine->pulseBuffer, i);
            } while (!(pulseToSkip->header.s & RKPulseStatusRingFiltered));
        } else if (skipCounter > 0) {
            // Skip processing if the buffer is getting full (avoid hitting SEM_VALUE_MAX)
            // Have some way to record skipping
            if (--skipCounter == 0) {
                RKLog(">%s Info. Skipped a chunk.\n", engine->name);
                for (i = 0; i < engine->coreCount; i++) {
                    engine->workers[i].lag = 0.0f;
                }
            }
        }
        
        // The pulse is considered "inspected" whether it will be skipped / filtered by the designated worker
        pulse->header.s |= RKPulseStatusRingInspected;
        
		#ifdef SHOW_RING_FILTER_DOUBLE_BUFFERING
		for (c = 0; c < engine->coreCount; c++) {
			*workerTaskDone++ = false;
		}
		for (c = 0; c < engine->coreCount; c++) {
			printf("c=%d:", c);
			for (i = 0; i < engine->radarDescription->pulseBufferDepth; i++) {
				printf(" %d", engine->workerTaskDone[i * engine->coreCount + c]);
			}
			printf("\n");
		}
		printf("===\n");
        RKLog("%s k = %d   pulseIndex = %u / %zu\n", engine->name, k, *engine->pulseIndex, pulse->header.i);
		#endif

		// Now we set this pulse to be "not done" and post
		workerTaskDone = engine->workerTaskDone + k * engine->coreCount;
		for (c = 0; c < engine->coreCount; c++) {
			*workerTaskDone++ = false;
			if (engine->useSemaphore) {
				if (sem_post(sem[c])) {
					RKLog("%s Error. Failed in sem_post(), errno = %d\n", engine->name, errno);
				}
			} else {
				engine->workers[c].tic++;
			}
		}

		// Now we check how many pulses are done
        allDone = true;
        while (j != k && allDone) {
            // Decide whether the pulse has been processed by FIR/IIR filter
            workerTaskDone = engine->workerTaskDone + j * engine->coreCount;
            for (c = 0; i < engine->coreCount; c++) {
                allDone &= *workerTaskDone++;
            }
            if (allDone) {
                pulse = RKGetPulse(engine->pulseBuffer, j);
                pulse->header.s |= RKPulseStatusRingFiltered | RKPulseStatusRingProcessed;
                j = RKNextModuloS(j, engine->radarDescription->pulseBufferDepth);
            }
        }
        
        // Log a message if it has been a while
        gettimeofday(&t0, NULL);
        if (RKTimevalDiff(t0, t1) > 0.05) {
            t1 = t0;
            RKPulseRingFilterUpdateStatusString(engine);
        }

		engine->tic++;

        // Update k to catch up for the next watch
        k = RKNextModuloS(k, engine->radarDescription->pulseBufferDepth);
    }

    // Wait for workers to return
    for (c = 0; c < engine->coreCount; c++) {
        RKPulseRingFilterWorker *worker = &engine->workers[c];
        if (engine->useSemaphore) {
            sem_post(worker->sem);
        }
        pthread_join(worker->tid, NULL);
        sem_unlink(worker->semaphoreName);
    }
    
    // Clean up
    free(engine->workerTaskDone);
    
    engine->state ^= RKEngineStateActive;
    return NULL;
}

#pragma mark - Life Cycle

RKPulseRingFilterEngine *RKPulseRingFilterEngineInit(void) {
    RKPulseRingFilterEngine *engine = (RKPulseRingFilterEngine *)malloc(sizeof(RKPulseRingFilterEngine));
    if (engine == NULL) {
        RKLog("Error. Unable to allocate a pulse ring filter engine.\n");
        return NULL;
    }
    memset(engine, 0, sizeof(RKPulseRingFilterEngine));
    sprintf(engine->name, "%s<PulseRingFilter>%s",
            rkGlobalParameters.showColor ? RKGetBackgroundColorOfIndex(RKEngineColorPulseRingFilterEngine) : "",
            rkGlobalParameters.showColor ? RKNoColor : "");
    engine->state = RKEngineStateAllocated;
    engine->useSemaphore = true;
    engine->filter.bLength = 2;
    engine->filter.aLength = 2;
    engine->filter.B[0].i = 0.5f;
    engine->filter.B[1].i = 0.5f;
    engine->filter.A[0].i = 0.25f;
    engine->filter.A[1].i = 0.25f;
    engine->gateCount = 100;
    engine->memoryUsage = sizeof(RKPulseRingFilterEngine);
    pthread_mutex_init(&engine->mutex, NULL);
    return engine;
}

void RKPulseRingFilterEngineFree(RKPulseRingFilterEngine *engine) {
    if (engine->state & RKEngineStateWantActive) {
        RKPulseRingFilterEngineStop(engine);
    }
    pthread_mutex_destroy(&engine->mutex);
    free(engine);
}

#pragma mark - Properties

void RKPulseRingFilterEngineSetVerbose(RKPulseRingFilterEngine *engine, const int verb) {
    engine->verbose = verb;
}

void RKPulseRingFilterEngineSetInputOutputBuffers(RKPulseRingFilterEngine *engine, const RKRadarDesc *desc,
                                                  RKConfig *configBuffer, uint32_t *configIndex,
                                                  RKBuffer pulseBuffer,   uint32_t *pulseIndex) {
    engine->radarDescription  = (RKRadarDesc *)desc;
    engine->configBuffer      = configBuffer;
    engine->configIndex       = configIndex;
    engine->pulseBuffer       = pulseBuffer;
    engine->pulseIndex        = pulseIndex;

    engine->state |= RKEngineStateProperlyWired;
}

void RKPulseRingFilterEngineSetCoreCount(RKPulseRingFilterEngine *engine, const uint8_t count) {
    if (engine->state & RKEngineStateWantActive) {
        RKLog("%s Error. Core count cannot change when the engine is active.\n", engine->name);
        return;
    }
    engine->coreCount = count;
}

void RKPulseRingFilterEngineSetCoreOrigin(RKPulseRingFilterEngine *engine, const uint8_t origin) {
    if (engine->state & RKEngineStateWantActive) {
        RKLog("%s Error. Core origin cannot change when the engine is active.\n", engine->name);
        return;
    }
    engine->coreOrigin = origin;
}

int RKPulseRingFilterEngineSetFilter(RKPulseRingFilterEngine *engine, RKIIRFilter *filter, const uint32_t gateCount) {
    memcpy(&engine->filter, filter, sizeof(RKIIRFilter));
    engine->gateCount = gateCount;
    return RKResultSuccess;
}

int RKPulseRingFilterEngineStart(RKPulseRingFilterEngine *engine) {
    if (!(engine->state & RKEngineStateProperlyWired)) {
        RKLog("%s Error. Not properly wired.\n", engine->name);
        return RKResultEngineNotWired;
    }
    if (engine->coreCount == 0) {
        engine->coreCount = 2;
    }
    if (engine->coreOrigin == 0) {
        engine->coreOrigin = 4;
    }
    if (engine->workers != NULL) {
        RKLog("%s Error. workers should be NULL here.\n", engine->name);
    }
    engine->workers = (RKPulseRingFilterWorker *)malloc(engine->coreCount * sizeof(RKPulseRingFilterWorker));
    engine->memoryUsage += engine->coreCount * sizeof(RKPulseRingFilterWorker);
    memset(engine->workers, 0, engine->coreCount * sizeof(RKPulseRingFilterWorker));
    RKLog("%s Starting ...\n", engine->name);
    engine->tic = 0;
    engine->state |= RKEngineStateActivating;
    if (pthread_create(&engine->tidPulseWatcher, NULL, pulseRingWatcher, engine) != 0) {
        RKLog("%s Error. Failed to start.\n", engine->name);
        return RKResultFailedToStartRingPulseWatcher;
    }
    while (engine->tic == 0) {
        usleep(10000);
    }
    return RKResultSuccess;
}

int RKPulseRingFilterEngineStop(RKPulseRingFilterEngine *engine) {
    if (engine->state & RKEngineStateDeactivating) {
        if (engine->verbose > 1) {
            RKLog("%s Info. Engine is being or has been deactivated.\n", engine->name);
        }
        return RKResultEngineDeactivatedMultipleTimes;
    }
	if (!(engine->state & RKEngineStateWantActive)) {
		RKLog("%s Not active.\n", engine->name);
		return RKResultEngineDeactivatedMultipleTimes;
	}
    RKLog("%s Stopping ...\n", engine->name);
    engine->state |= RKEngineStateDeactivating;
    engine->state ^= RKEngineStateWantActive;
	if (engine->tidPulseWatcher) {
		pthread_join(engine->tidPulseWatcher, NULL);
		engine->tidPulseWatcher = (pthread_t)0;
		free(engine->workers);
		engine->workers = NULL;
	} else {
		RKLog("%s Invalid thread ID.\n", engine->name);
	}
    engine->state ^= RKEngineStateDeactivating;
    RKLog("%s Stopped.\n", engine->name);
    if (engine->state != (RKEngineStateAllocated | RKEngineStateProperlyWired)) {
        RKLog("%s Inconsistent state 0x%04x\n", engine->name, engine->state);
    }
    return RKResultSuccess;
}

char *RKPulseRingFilterEngineStatusString(RKPulseRingFilterEngine *engine) {
    return engine->statusBuffer[RKPreviousModuloS(engine->statusBufferIndex, RKBufferSSlotCount)];
}

#pragma mark - Interactions

