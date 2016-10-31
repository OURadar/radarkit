//
//  RKPulseCompression.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 3/18/15.
//  Copyright (c) 2015 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit/RKPulseCompression.h>

// Internal functions

int workerThreadId(RKPulseCompressionEngine *engine);
void RKPulseCompressionShowBuffer(fftwf_complex *in, const int n);
void *pulseCompressionCore(void *in);

#pragma mark -

// Implementations

int workerThreadId(RKPulseCompressionEngine *engine) {
    int i;
    pthread_t id = pthread_self();
    for (i = 0; i < engine->coreCount; i++) {
        if (pthread_equal(id, engine->workers[i].tid) == 0) {
            return i;
        }
    }
    return -1;
}

void RKPulseCompressionShowBuffer(fftwf_complex *in, const int n) {
    for (int k = 0; k < n; k++) {
        printf("    %6.2fd %s %6.2fdi\n", in[k][0], in[k][1] < 0 ? "-" : "+", fabsf(in[k][1]));
    }
}

void *pulseCompressionCore(void *_in) {
    RKPulseCompressionWorker *me = (RKPulseCompressionWorker *)_in;
    RKPulseCompressionEngine *engine = me->parentEngine;

    int i, j, k, p;
    struct timeval t0, t1, t2;

    i = workerThreadId(engine);
    if (i < 0) {
        i = me->id;
        RKLog("Warning. Unable to find my thread ID. Assume %d\n", me->id);
    } else if (engine->verbose > 1) {
        RKLog("Info. Thread ID %d = %d okay.\n", me->id, i);
    }
    const int c = me->id;

    const int multiplyMethod = 1;

    // Find the semaphore
    sem_t *sem = sem_open(me->semaphoreName, O_RDWR);
    if (sem == SEM_FAILED) {
        RKLog("Error. Unable to retrieve semaphore %d\n", c);
        return (void *)RKResultFailedToRetrieveSemaphore;
    };
    // Set my CPU core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(c, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    pthread_setaffinity_np(me->tid, sizeof(cpu_set_t), &cpuset);

    // Allocate local resources, use k to keep track of the total allocation
    // Avoid fftwf_malloc() here so that if a non-avx-enabled libfftw is compatible
    fftwf_complex *in, *out;
    posix_memalign((void **)&in, RKSIMDAlignSize, RKGateCount * sizeof(fftwf_complex));
    posix_memalign((void **)&out, RKSIMDAlignSize, RKGateCount * sizeof(fftwf_complex));
    if (in == NULL || out == NULL) {
        RKLog("Error. Unable to allocate resources for FFTW.\n");
        return (void *)RKResultFailedToAllocateFFTSpace;
    }
    k = 2 * RKGateCount * sizeof(fftwf_complex);
    RKIQZ *zi, *zo;
    posix_memalign((void **)&zi, RKSIMDAlignSize, sizeof(RKIQZ));
    posix_memalign((void **)&zo, RKSIMDAlignSize, sizeof(RKIQZ));
    if (zi == NULL || zo == NULL) {
        RKLog("Error. Unable to allocate resources for FFTW.\n");
        return (void *)RKResultFailedToAllocateFFTSpace;
    }
    k += 2 * sizeof(RKIQZ);
    double *busyPeriods, *fullPeriods;
    posix_memalign((void **)&busyPeriods, RKSIMDAlignSize, RKWorkerDutyCycleBufferSize * sizeof(double));
    posix_memalign((void **)&fullPeriods, RKSIMDAlignSize, RKWorkerDutyCycleBufferSize * sizeof(double));
    if (busyPeriods == NULL || fullPeriods == NULL) {
        RKLog("Error. Unable to allocate resources for duty cycle calculation\n");
        return (void *)RKResultFailedToAllocateDutyCycleBuffer;
    }
    k += 2 * RKWorkerDutyCycleBufferSize * sizeof(double);
    memset(busyPeriods, 0, RKWorkerDutyCycleBufferSize * sizeof(double));
    memset(fullPeriods, 0, RKWorkerDutyCycleBufferSize * sizeof(double));
    double allBusyPeriods = 0.0, allFullPeriods = 0.0;

    // Initiate a variable to store my name
    char name[20];
    if (rkGlobalParameters.showColor) {
        i = sprintf(name, "\033[3%dm", c % 7 + 1);
    }
    if (engine->coreCount > 9) {
        i += sprintf(name + i, "Core %02d", c);
    } else {
        i += sprintf(name + i, "Core %d", c);
    }
    if (rkGlobalParameters.showColor) {
        sprintf(name + i, "\033[0m");
    }

    // Initialize some end-of-loop variables
    gettimeofday(&t0, NULL);
    gettimeofday(&t2, NULL);
    
    // The last index of the pulse buffer
    uint32_t i0 = RKBuffer0SlotCount - engine->coreCount + c;

    // The latest index in the dutyCycle buffer
    int d0 = 0;

    // DFT plan size and plan index in the parent engine
    int planSize = -1, planIndex;

    // Log my initial state
    if (engine->verbose) {
        pthread_mutex_lock(&engine->coreMutex);
        RKLog(">%s started.  i0 = %d   mem = %s  tic = %d\n", name, i0, RKIntegerToCommaStyleString(k), me->tic);
        pthread_mutex_unlock(&engine->coreMutex);
    }

    // Increase the tic once to indicate this processing core is created.
    me->tic++;

    //
    // free   busy       free   busy
    // .......|||||||||||.......|||||||||
    // t2 --- t1 --- t0/t2 --- t1 --- t0
    //        [ t0 - t1 ]
    // [    t0 - t2     ]
    //

    uint32_t tic = me->tic;

    while (engine->state == RKPulseCompressionEngineStateActive) {
        if (engine->useSemaphore) {
            #ifdef DEBUG_IQ
            RKLog(">%s sem_wait()\n", coreName);
            #endif
            sem_wait(sem);
        } else {
            while (tic == me->tic && engine->state == RKPulseCompressionEngineStateActive) {
                usleep(1000);
            }
            tic = me->tic;
        }
        if (engine->state != RKPulseCompressionEngineStateActive) {
            break;
        }

        // Something happened
        gettimeofday(&t1, NULL);

        // Start of getting busy
        i0 = RKNextNModuloS(i0, engine->coreCount, engine->size);
        me->lag = fmodf((float)(*engine->index - me->pid + engine->size) / engine->size, 1.0f);

        RKPulse *pulse = &engine->pulses[i0];

        #ifdef DEBUG_IQ
        RKLog(">%s i0 = %d  stat = %d\n", coreName, i0, input->header.s);
        #endif

        // Filter group id
        const int gid = engine->filterGid[i0];

        // Do some work with this pulse
        // DFT of the raw data is stored in *in
        // DFT of the filter is stored in *out
        // Their product is stored in *out using in-place multiplication: out[i] = out[i] * in[i]
        // Then, the inverse DFT is performed to get out back to time domain, which is the compressed pulse

        // Process each polarization separately and indepently
        for (p = 0; p < 2; p++) {
            // Go through all the filters in this fitler group
            for (j = 0; j < engine->filterCounts[gid]; j++) {
                // Get the plan index and size from parent engine
                planIndex = engine->planIndices[i0][j];
                planSize = engine->planSizes[planIndex];

                // Copy and convert the samples
                for (k = 0, i = engine->anchors[gid][j].origin;
                     k < planSize && i < pulse->header.gateCount;
                     k++, i++) {
                    in[k][0] = (RKFloat)pulse->X[p][i].i;
                    in[k][1] = (RKFloat)pulse->X[p][i].q;
                }
                // Zero pad the input; a filter is always zero-padded in the setter function.
                memset(in[k], 0, (planSize - k) * sizeof(fftwf_complex));

                fftwf_execute_dft(engine->planForwardInPlace[planIndex], in, in);

                //printf("dft(in) =\n"); RKPulseCompressionShowBuffer(in, 8);

                fftwf_execute_dft(engine->planForwardOutPlace[planIndex], (fftwf_complex *)engine->filters[gid][j], out);

                //printf("dft(filt) =\n"); RKPulseCompressionShowBuffer(in, 8);

                if (multiplyMethod == 1) {
                    // In-place SIMD multiplication using the interleaved format
                    RKSIMD_iymul((RKComplex *)in, (RKComplex *)out, planSize);
                } else if (multiplyMethod == 2) {
                    // Deinterleave the RKComplex data into RKIQZ format, multiply using SIMD, then interleave the result back to RKComplex format
                    RKSIMD_Complex2IQZ((RKComplex *)in, zi, planSize);
                    RKSIMD_Complex2IQZ((RKComplex *)out, zo, planSize);
                    RKSIMD_izmul(zi, zo, planSize, true);
                    RKSIMD_IQZ2Complex(zo, (RKComplex *)out, planSize);
                } else {
                    // Regular multiplication with compiler optimization -Os
                    RKSIMD_iymul_reg((RKComplex *)in, (RKComplex *)out, planSize);
                }

                //printf("in * out =\n"); RKPulseCompressionShowBuffer(out, 8);

                fftwf_execute_dft(engine->planBackwardInPlace[planIndex], out, out);

                //printf("idft(out) =\n"); RKPulseCompressionShowBuffer(out, 8);

                // Scaling due to a net gain of planSize from forward + backward DFT
                RKSIMD_iyscl((RKComplex *)out, 1.0f / planSize, planSize);
                //printf("idft(out) =\n"); RKPulseCompressionShowBuffer(out, 8);

                for (k = 0, i = engine->anchors[gid][j].length - 1;
                     k < MIN(pulse->header.gateCount - engine->anchors[gid][j].length, engine->anchors[gid][j].maxDataLength);
                     k++, i++) {
                    pulse->Y[p][k].i = out[i][0];
                    pulse->Y[p][k].q = out[i][1];
                }

                // Copy over the parameters used
                pulse->parameters.planIndices[p][j] = planIndex;
                pulse->parameters.planSizes[p][j] = planSize;
            } // filterCount
            pulse->parameters.filterCounts[p] = j;
        } // p - polarization

        pulse->header.s |= RKPulseStatusCompressed;
        me->pid = i0;

        // Done processing, get the time
        gettimeofday(&t0, NULL);

        // Drop the oldest reading, replace it, and add to the calculation
        allBusyPeriods -= busyPeriods[d0];
        allFullPeriods -= fullPeriods[d0];
        busyPeriods[d0] = RKTimevalDiff(t0, t1);
        fullPeriods[d0] = RKTimevalDiff(t0, t2);
        allBusyPeriods += busyPeriods[d0];
        allFullPeriods += fullPeriods[d0];
        d0 = RKNextModuloS(d0, RKWorkerDutyCycleBufferSize);
        me->dutyCycle = allBusyPeriods / allFullPeriods;

        t2 = t0;
    }

    // Clean up
    free(zi);
    free(zo);
    free(in);
    free(out);
    free(busyPeriods);
    free(fullPeriods);

    RKLog(">%s ended.\n", name);
    
    return NULL;
}

void *pulseWatcher(void *_in) {
    RKPulseCompressionEngine *engine = (RKPulseCompressionEngine *)_in;

    int i, j, k;
    uint32_t c;

    sem_t *sem[engine->coreCount];

    bool found;
    int gid;
    int planSize;
    int planIndex = 0;
    int skipCounter = 0;

    // FFTW's memory allocation and plan initialization are not thread safe but others are.
    fftwf_complex *in, *out;
    posix_memalign((void **)&in, RKSIMDAlignSize, RKGateCount * sizeof(fftwf_complex));
    posix_memalign((void **)&out, RKSIMDAlignSize, RKGateCount * sizeof(fftwf_complex));
    k = 2 * RKGateCount * sizeof(fftwf_complex);
    if (engine->verbose) {
        RKLog("pulseWatcher() allocated %s B\n", RKIntegerToCommaStyleString(k));
    }

    planSize = 1 << (int)ceilf(log2f((float)RKGateCount));
    bool exportWisdom = false;
    const char wisdomFile[] = "fft-wisdom";

    if (RKFilenameExists(wisdomFile)) {
        RKLog("Loading DFT wisdom ...\n");
        fftwf_import_wisdom_from_filename(wisdomFile);
    } else {
        RKLog("DFT wisdom file not found.\n");
        exportWisdom = true;
    }

    for (j = 0; j < 3; j++) {
        RKLog("Pre-allocate FFTW resources for plan size %s (%d)\n", RKIntegerToCommaStyleString(planSize), planIndex);
        engine->planForwardInPlace[planIndex] = fftwf_plan_dft_1d(planSize, in, in, FFTW_FORWARD, FFTW_MEASURE);
        engine->planForwardOutPlace[planIndex] = fftwf_plan_dft_1d(planSize, in, out, FFTW_FORWARD, FFTW_MEASURE);
        engine->planBackwardInPlace[planIndex] = fftwf_plan_dft_1d(planSize, out, out, FFTW_BACKWARD, FFTW_MEASURE);
        //fftwf_print_plan(engine->planForwardInPlace[planIndex]);
        engine->planSizes[planIndex++] = planSize;
        engine->planCount++;
        planSize /= 2;
    }

    // Change the state to active so all the processing cores stay in the busy loop
    engine->state = RKPulseCompressionEngineStateActive;
    
    // Spin off N workers to process I/Q pulses
    for (i = 0; i < engine->coreCount; i++) {
        RKPulseCompressionWorker *worker = &engine->workers[i];
        snprintf(worker->semaphoreName, 16, "rk-sem-%03d", i);
        sem[i] = sem_open(worker->semaphoreName, O_CREAT | O_EXCL, 0600, 0);
        if (sem[i] == SEM_FAILED) {
            if (engine->verbose > 1) {
                RKLog("Info. Semaphore %s exists. Try to remove and recreate.\n", worker->semaphoreName);
            }
            if (sem_unlink(worker->semaphoreName)) {
                RKLog("Error. Unable to unlink semaphore %s.\n", worker->semaphoreName);
            }
            // 2nd trial
            sem[i] = sem_open(worker->semaphoreName, O_CREAT | O_EXCL, 0600, 0);
            if (sem[i] == SEM_FAILED) {
                RKLog("Error. Unable to remove then create semaphore %s\n", worker->semaphoreName);
                return (void *)RKResultFailedToInitiateSemaphore;
            } else if (engine->verbose > 1) {
                RKLog("Info. Semaphore %s removed and recreated.\n", worker->semaphoreName);
            }
        }
        worker->id = i;
        worker->parentEngine = engine;
        if (pthread_create(&worker->tid, NULL, pulseCompressionCore, worker) != 0) {
            RKLog("Error. Failed to start a compression core.\n");
            return (void *)RKResultFailedToStartCompressionCore;
        }
    }

    // Wait for the workers to increase the tic count once
    // Using sem_wait here could cause a stolen post within the worker
    // Tested and removed on 9/29/2016
    for (i = 0; i < engine->coreCount; i++) {
        while (engine->workers[i].tic == 0) {
            usleep(1000);
        }
    }

    // Increase the tic once to indicate the watcher is ready
    engine->tic++;

    // Here comes the busy loop
    k = 0;
    c = 0;
    RKLog("pulseWatcher() started.   c = %d   k = %d   engine->index = %d\n", c, k, *engine->index);
    while (engine->state == RKPulseCompressionEngineStateActive) {
        // Wait until the engine index move to the next one for storage
        while (k == *engine->index && engine->state == RKPulseCompressionEngineStateActive) {
            usleep(200);
        }
        while (engine->pulses[k].header.s != RKPulseStatusReady && engine->state == RKPulseCompressionEngineStateActive) {
            usleep(200);
        }
        if (engine->state == RKPulseCompressionEngineStateActive) {
            RKPulse *pulse = &engine->pulses[k];

            // Compute the filter group id to use
            gid = pulse->header.i % engine->filterGroupCount;
            engine->filterGid[k] = gid;

            // Find the right plan; create it if it does not exist
            for (j = 0; j < engine->filterCounts[gid]; j++) {
                planSize = 1 << (int)ceilf(log2f((float)MIN(pulse->header.gateCount, engine->anchors[gid][j].maxDataLength)));

                found = false;
                i = engine->planCount;
                while (i > 0) {
                    i--;
                    if (planSize == engine->planSizes[i]) {
                        planIndex = i;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    RKLog("A new DFT plan of size %d is needed ...  gid = %d   planCount = %d\n", planSize, gid, engine->planCount);
                    if (engine->planCount >= RKPulseCompressionDFTPlanCount) {
                        RKLog("Error. Unable to create another DFT plan.  engine->planCount = %d\n", engine->planCount);
                        exit(EXIT_FAILURE);
                    }
                    planIndex = engine->planCount;
                    engine->planForwardInPlace[planIndex] = fftwf_plan_dft_1d(planSize, in, in, FFTW_FORWARD, FFTW_MEASURE);
                    engine->planForwardOutPlace[planIndex] = fftwf_plan_dft_1d(planSize, in, out, FFTW_FORWARD, FFTW_MEASURE);
                    engine->planBackwardInPlace[planIndex] = fftwf_plan_dft_1d(planSize, out, out, FFTW_BACKWARD, FFTW_MEASURE);
                    engine->planSizes[planIndex] = planSize;
                    engine->planCount++;
                    RKLog("k = %d   j = %d  planIndex = %d\n", k, j, planIndex);
                }
                engine->planIndices[k][j] = planIndex;
            }

            #ifdef DEBUG_IQ
            RKLog("pulseWatcher() posting core-%d for pulse %d gate %d\n", c, k, engine->pulses[k].header.gateCount);
            #endif

            // Assess the buffer fullness
            if (c == 0 && skipCounter == 0 &&  engine->workers[c].lag > 0.9f) {
                engine->almostFull++;
                skipCounter = engine->size;
                RKLog("Warning. Buffer overflow.\n");
            }

            if (skipCounter > 0) {
                skipCounter--;
            } else {
                if (engine->useSemaphore) {
                    sem_post(sem[c]);
                } else {
                    engine->workers[c].tic++;
                }
                c = RKNextModuloS(c, engine->coreCount);
            }
        }
        // Update k to catch up for the next watch
        k = RKNextModuloS(k, engine->size);
    }

    // Wait for workers to return
    for (i = 0; i < engine->coreCount; i++) {
        RKPulseCompressionWorker *worker = &engine->workers[i];
        if (engine->useSemaphore) {
            sem_post(sem[i]);
        }
        pthread_join(worker->tid, NULL);
        sem_unlink(worker->semaphoreName);
    }

    // Export wisdom
    if (exportWisdom) {
        RKLog("Saving DFT wisdom ...\n");
        fftwf_export_wisdom_to_filename(wisdomFile);
    }

    // Destroy all the DFT plans
    for (k = 0; k < engine->planCount; k++) {
        fftwf_destroy_plan(engine->planForwardInPlace[k]);
        fftwf_destroy_plan(engine->planForwardOutPlace[k]);
        fftwf_destroy_plan(engine->planBackwardInPlace[k]);
    }
    free(in);
    free(out);

    return NULL;
}

//
#pragma mark -

RKPulseCompressionEngine *RKPulseCompressionEngineInit(void) {
    RKPulseCompressionEngine *engine = (RKPulseCompressionEngine *)malloc(sizeof(RKPulseCompressionEngine));
    memset(engine, 0, sizeof(RKPulseCompressionEngine));
    engine->state = RKPulseCompressionEngineStateAllocated;
    engine->verbose = 1;
    engine->useSemaphore = true;
    pthread_mutex_init(&engine->coreMutex, NULL);
    return engine;
}

void RKPulseCompressionEngineFree(RKPulseCompressionEngine *engine) {
    if (engine->state == RKPulseCompressionEngineStateActive) {
        RKPulseCompressionEngineStop(engine);
    }
    for (int i = 0; i < engine->filterGroupCount; i++) {
        for (int j = 0; j < engine->filterCounts[i]; j++) {
            if (engine->filters[i][j] != NULL) {
                free(engine->filters[i][j]);
            }
        }
    }
    free(engine->filterGid);
    free(engine->planIndices);
    free(engine);
}

//
// RKPulseCompressionEngineSetInputOutputBuffers
//
// Input:
// engine - the pulse compression engine
// pulses - the raw data buffer
// index - the reference index watch, *index is the latest reading in *pulses
// size - number of slots in *pulses
//
void RKPulseCompressionEngineSetInputOutputBuffers(RKPulseCompressionEngine *engine,
                                                   RKPulse *pulses,
                                                   uint32_t *index,
                                                   const uint32_t size) {
    engine->pulses = pulses;
    engine->index = index;
    engine->size = size;

    if (engine->filterGid != NULL) {
        free(engine->filterGid);
    }
    engine->filterGid = (int *)malloc(size * sizeof(int));
    if (engine->filterGid == NULL) {
        RKLog("Error. Unable to allocate filterGid.\n");
        exit(EXIT_FAILURE);
    }

    if (engine->planIndices != NULL) {
        free(engine->planIndices);
    }
    engine->planIndices = (RKPulseCompressionPlanIndex *)malloc(size * sizeof(RKPulseCompressionPlanIndex));
    if (engine->planIndices == NULL) {
        RKLog("Error. Unable to allocate planIndices.\n");
        exit(EXIT_FAILURE);
    }
}

void RKPulseCompressionEngineSetCoreCount(RKPulseCompressionEngine *engine, const unsigned int count) {
    if (engine->state == RKPulseCompressionEngineStateActive) {
        RKLog("Error. Core count cannot be changed when the engine is active.\n");
        return;
    }
    engine->coreCount = count;
}


int RKPulseCompressionEngineStart(RKPulseCompressionEngine *engine) {
    engine->state = RKPulseCompressionEngineStateActivating;
    if (engine->filterGroupCount == 0) {
        // Set to default impulse as matched filter
        RKPulseCompressionSetFilterToImpulse(engine);
    }
    if (engine->coreCount == 0) {
        engine->coreCount = 8;
    }
    if (engine->workers != NULL) {
        RKLog("Error. engine->workers should be NULL here.\n");
    }
    engine->workers = (RKPulseCompressionWorker *)malloc(engine->coreCount * sizeof(RKPulseCompressionWorker));
    memset(engine->workers, 0, sizeof(RKPulseCompressionWorker));
    if (engine->verbose) {
        RKLog("Starting pulseWatcher() ...\n");
    }
    if (pthread_create(&engine->tidPulseWatcher, NULL, pulseWatcher, engine) != 0) {
        RKLog("Error. Failed to start a pulse watcher.\n");
        return RKResultFailedToStartPulseWatcher;
    }
    while (engine->tic == 0) {
        usleep(1000);
    }

    return 0;
}

int RKPulseCompressionEngineStop(RKPulseCompressionEngine *engine) {
    int k;
    if (engine->state != RKPulseCompressionEngineStateActive) {
        if (engine->verbose > 1) {
            RKLog("Info. Pulse compression engine is being or has been deactivated.\n");
        }
        return 1;
    }
    engine->state = RKPulseCompressionEngineStateDeactivating;
    k = pthread_join(engine->tidPulseWatcher, NULL);
    if (engine->verbose) {
        RKLog("pulseWatcher() ended\n");
    }
    free(engine->workers);
    engine->workers = NULL;
    engine->state = RKPulseCompressionEngineStateNull;
    return k;
}

int RKPulseCompressionSetFilterCountOfGroup(RKPulseCompressionEngine *engine, const int group, const int count) {
    engine->filterCounts[group] = count;
    return 0;
}

int RKPulseCompressionSetFilterGroupCount(RKPulseCompressionEngine *engine, const int groupCount) {
    engine->filterGroupCount = groupCount;
    return 0;
}

int RKPulseCompressionSetFilter(RKPulseCompressionEngine *engine, const RKComplex *filter, const int filterLength, const int origin, const int maxDataLength, const int group, const int index) {
    if (engine->filterGroupCount >= RKMaxMatchedFilterGroupCount) {
        RKLog("Error. Unable to set anymore filters.\n");
        return RKResultFailedToAddFilter;
    }
    if (engine->filters[group][index] != NULL) {
        free(engine->filters[group][index]);
    }
    if (posix_memalign((void **)&engine->filters[group][index], RKSIMDAlignSize, RKGateCount * sizeof(RKComplex))) {
        RKLog("Error. Unable to allocate filter memory.\n");
        return RKResultFailedToAllocateFilter;
    }
    memset(engine->filters[group][index], 0, RKGateCount * sizeof(RKComplex));
    memcpy(engine->filters[group][index], filter, filterLength * sizeof(RKComplex));
    engine->filterGroupCount = MAX(engine->filterGroupCount, group + 1);
    engine->filterCounts[group] = MAX(engine->filterCounts[group], index + 1);
    engine->anchors[group][index].origin = origin;
    engine->anchors[group][index].length = filterLength;
    engine->anchors[group][index].maxDataLength = maxDataLength;
    if (engine->verbose) {
        RKLog("Matched filter set.  group count = %d\n", engine->filterGroupCount);
        for (int i = 0; i < engine->filterGroupCount; i++) {
            RKLog(">Filter count of group[%d] = %d\n", i, engine->filterCounts[i]);
            for (int j = 0; j < engine->filterCounts[i]; j++) {
                RKLog(">   Filter[%d] @ length = %d  origin = %d  maximum data length = %s\n", j, engine->anchors[i][j].length, engine->anchors[i][j].origin, RKIntegerToCommaStyleString(engine->anchors[i][j].maxDataLength));
            }
        }
    }
    return 0;
}

int RKPulseCompressionSetFilterToImpulse(RKPulseCompressionEngine *engine) {
    RKComplex filter[] = {{1.0f, 0.0f}};
    return RKPulseCompressionSetFilter(engine, filter, sizeof(filter) / sizeof(RKComplex), 0, RKGateCount, 0, 0);
}

int RKPulseCompressionSetFilterTo121(RKPulseCompressionEngine *engine) {
    RKComplex filter[] = {{1.0f, 0.0f}, {2.0f, 0.0f}, {1.0f, 0.0f}};
    return RKPulseCompressionSetFilter(engine, filter, sizeof(filter) / sizeof(RKComplex), 0, RKGateCount, 0, 0);
}

void RKPulseCompressionEngineLogStatus(RKPulseCompressionEngine *engine) {
    int i, k;
    bool full = true;
    char string[RKMaximumStringLength];
    i = *engine->index * 10 / engine->size;
    RKPulseCompressionWorker *worker;

    string[RKMaximumStringLength - 1] = '\0';
    string[RKMaximumStringLength - 2] = '#';
    memset(string, '|', i);
    memset(string + i, '.', 10 - i);
    i = 10;
    if (full) {
        i += sprintf(string + i, " :");
    }
    // Lag from each core
    for (k = 0; k < engine->coreCount; k++) {
        worker = &engine->workers[k];
        if (rkGlobalParameters.showColor) {
            if (full) {
                i += snprintf(string + i, RKMaximumStringLength - i, " \033[3%dm%02.0f\033[0m",
                              worker->lag > 0.7 ? 1 : (worker->lag > 0.5 ? 3 : 2),
                              99.0f * worker->lag);
            } else {
                i += snprintf(string + i, RKMaximumStringLength - i, "\033[3%dm%02.0f\033[0m",
                              worker->lag > 0.7 ? 1 : (worker->lag > 0.5 ? 3 : 2),
                              99.0f * worker->lag);
            }
        } else {
            i += snprintf(string + i, RKMaximumStringLength - i, " %2.0f", 99.0f * worker->lag);
        }
    }
    if (full) {
        i += snprintf(string + i, RKMaximumStringLength - i, " |");
    } else {
        i += snprintf(string + i, RKMaximumStringLength - i, "|");
    }
    // Duty cycle of each core
    for (k = 0; k < engine->coreCount && i < RKMaximumStringLength - 13; k++) {
        worker = &engine->workers[k];
        if (rkGlobalParameters.showColor) {
            if (full) {
                i += snprintf(string + i, RKMaximumStringLength - i, " \033[3%dm%2.0f\033[0m",
                              worker->dutyCycle > 0.99 ? 1 : (worker->dutyCycle > 0.95 ? 3 : 2),
                              99.0f * worker->dutyCycle);
            } else {
                i += snprintf(string + i, RKMaximumStringLength - i, "\033[3%dm%2.0f\033[0m",
                              worker->dutyCycle > 0.99 ? 1 : (worker->dutyCycle > 0.95 ? 3 : 2),
                              99.0f * worker->dutyCycle);
            }
        } else {
            i += snprintf(string + i, RKMaximumStringLength - i, " %2.0f", 99.0f * worker->dutyCycle);
        }
    }
    // Semaphore value of each core
//    int v;
//    if (full) {
//        i += snprintf(string + i, RKMaximumStringLength - i, " |");
//    } else {
//        i += snprintf(string + i, RKMaximumStringLength - i, "|");
//    }
//    for (k = 0; k < engine->coreCount && i < RKMaximumStringLength - 13; k++) {
//        worker = &engine->workers[k];
//        sem_t *sem = sem_open(worker->semaphoreName, O_RDONLY);
//        sem_getvalue(sem, &v);
//        i += snprintf(string + i, RKMaximumStringLength - i, " %03d", v);
//    }
    // Almost Full flag
    i += snprintf(string + i, RKMaximumStringLength - i, " [%d]", engine->almostFull);
    if (i > RKMaximumStringLength - 13) {
        memset(string + i, '#', RKMaximumStringLength - i - 1);
    }
    RKLog("%s", string);
}
