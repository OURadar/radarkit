//
//  RKCommandCenter.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 1/5/17.
//  Copyright © 2017 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit/RKCommandCenter.h>

// Private declarations

int socketCommandHandler(RKOperator *);
int socketStreamHandler(RKOperator *);
int socketInitialHandler(RKOperator *);
int socketTerminateHandler(RKOperator *);

#pragma mark - Helper Functions

static void consolidateStreams(RKCommandCenter *engine) {

    int j, k;
    RKStream consolidatedStreams;
    
    // Consolidate streams
    for (j = 0; j < RKCommandCenterMaxRadars; j++) {
        RKRadar *radar = engine->radars[j];
        if (radar == NULL) {
            continue;
        }
        if (radar->desc.initFlags & RKInitFlagSignalProcessor) {
            continue;
        }
        consolidatedStreams = RKStreamNull;
        for (k = 0; k < RKCommandCenterMaxConnections; k++) {
            RKUser *user = &engine->users[k];
            if (radar == user->radar) {
                consolidatedStreams |= user->streams;
            }
        }
        RKRadarRelayUpdateStreams(radar->radarRelay, consolidatedStreams);
    }
}

#pragma mark - Handlers

int socketCommandHandler(RKOperator *O) {
    RKCommandCenter *engine = O->userResource;
    RKUser *user = &engine->users[O->iid];
    
    int j, k;

    j = snprintf(user->commandResponse, RKMaximumPacketSize - 1, "%s %d radar:", engine->name, engine->radarCount);
    for (k = 0; k < engine->radarCount; k++) {
        RKRadar *radar = engine->radars[k];
        j += snprintf(user->commandResponse + j, RKMaximumPacketSize - j - 1, " %s", radar->desc.name);
    }

    //int ival;
    //float fval1, fval2;
    char sval1[RKMaximumStringLength];
    char sval2[RKMaximumStringLength];
    memset(sval1, 0, sizeof(sval1));
    memset(sval2, 0, sizeof(sval2));
    
    // Delimited reading: each command is separated by a ';'
    // e.g., a radarkit nopasswd;s hz;h hv on;
    
    char *commandString = O->cmd;
    char *commandStringEnd = NULL;

    RKStripTail(commandString);

    RKStream stream;

    while (commandString != NULL) {
        if ((commandStringEnd = strchr(commandString, ';')) != NULL) {
            *commandStringEnd = '\0';
        }
        // Command 'ping' is most frequent, check this first
        if (!strncmp(commandString, "ping", 4)) {
            user->pingCount++;
            if (engine->verbose && user->pingCount % 100 == 0) {
                RKLog("%s %s Ping x %s\n", engine->name, O->name, RKIntegerToCommaStyleString(user->pingCount));
            }
            // There is no need to send a response. The delegate function socketStreamHandler sends a beacon periodically
        } else if (user->radar->desc.initFlags & RKInitFlagSignalProcessor) {
            k = 0;
            while (!(user->radar->state & RKRadarStateLive)) {
                usleep(100000);
                if (++k % 10 == 0 && engine->verbose > 1) {
                    RKLog("%s sleep 1/%.1f s   radar->state = 0x%04x\n", engine->name, (float)k * 0.1f, user->radar->state);
                }
            }
            user->commandCount++;
            RKLog("%s %s Received command '%s%s%s' (%p)\n",
                  engine->name, O->name,
                  rkGlobalParameters.showColor ? RKGreenColor : "",
                  commandString,
                  rkGlobalParameters.showColor ? RKNoColor : "",
                  commandStringEnd);
            // Process the command
            switch (commandString[0]) {
                case 'a':
                    // Authenticate
                    sscanf(commandString + 1, "%s %s", sval1, sval2);
                    RKLog("%s %s Authenticating %s %s ... (%d) (%d)\n", engine->name, O->name, sval1, sval2, strlen(sval1), sizeof(user->login));
                    // Check authentication here. For now, control is always authorized
                    //
                    //
                    user->access |= RKStreamControl;
                    // Update some info
                    strncpy(user->login, sval1, sizeof(user->login) - 1);
                    user->controlFirstUID = (uint32_t)-1;
                    break;

                case 'c':
                    user->textPreferences ^= RKTextPreferencesShowColor;
                    user->streamsInProgress &= ~RKStreamStatusMask;
                    break;
                    
                case 'd':
                    // DSP related
                    switch (commandString[commandString[1] == ' ' ? 2 : 1]) {
                        case 'r':
                            // Suspend all user straems
                            for (k = 0; k < RKCommandCenterMaxConnections; k++) {
                                if (engine->users[k].radar == user->radar && engine->users[k].streams != RKStreamNull) {
                                    pthread_mutex_lock(&engine->users[k].mutex);
                                    engine->users[k].streamsToRestore = engine->users[k].streams;
                                    engine->users[k].streams = RKStreamNull;
                                    engine->users[k].streamsInProgress = RKStreamNull;
                                    pthread_mutex_unlock(&engine->users[k].mutex);
                                }
                            }

                            // Reset the radar engines
                            RKExecuteCommand(user->radar, commandString, user->commandResponse);

                            // Wait until socketStreamHandler to run another iteration
                            uint64_t tic = user->tic;
                            do {
                                usleep(100000);
                            } while (tic == user->tic);

                            RKLog("%s %s Skipping to current ...", engine->name, O->name);
                            RKCommandCenterSkipToCurrent(engine, user->radar);

                            for (k = 0; k < RKCommandCenterMaxConnections; k++) {
                                if (engine->users[k].radar == user->radar && engine->users[k].streamsToRestore != RKStreamNull) {
                                    pthread_mutex_lock(&engine->users[k].mutex);
                                    engine->users[k].streams = engine->users[k].streamsToRestore;
                                    engine->users[k].streamsToRestore = RKStreamNull;
                                    pthread_mutex_unlock(&engine->users[k].mutex);
                                }
                            }
                            break;
                        default:
                            RKLog("commandString = %s\n", commandString);
                            RKExecuteCommand(user->radar, commandString, user->commandResponse);
                            break;
                    }
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;

                case 'i':
                    O->delimTx.type = RKNetworkPacketTypeRadarDescription;
                    O->delimTx.size = (uint32_t)sizeof(RKRadarDesc);
                    RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &user->radar->desc, sizeof(RKRadarDesc), NULL);
                    break;

                case 'q':
                    sprintf(user->commandResponse, "Bye." RKEOL);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    RKOperatorHangUp(O);
                    break;

                case 's':
                    // Stream varrious data
                    stream = RKStreamFromString(commandString + 1);
                    k = user->rayIndex;
                    pthread_mutex_lock(&user->mutex);
                    user->streamsInProgress = RKStreamNull;
                    user->streams = stream;
                    user->rayStatusIndex = RKPreviousModuloS(user->radar->momentEngine->rayStatusBufferIndex, RKBufferSSlotCount);
                    user->scratchSpaceIndex = user->radar->sweepEngine->scratchSpaceIndex;
                    pthread_mutex_unlock(&user->mutex);
                    sprintf(user->commandResponse, "{\"type\": \"init\", \"access\": 0x%lx, \"streams\": 0x%lx, \"indices\": [%d, %d]}" RKEOL,
                            (unsigned long)user->access, (unsigned long)user->streams, k, user->rayIndex);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;
                    
                case 'u':
                    // Turn this user into an active node with user product return
                    RKParseProductDescription(&user->productDescriptions[user->productCount], commandString + 1);
                    RKLog("%s Registering external product '%s' (%s) ...", engine->name,
                          user->productDescriptions[user->productCount].name,
                          user->productDescriptions[user->productCount].symbol);
                    user->productIds[user->productCount] = RKSweepEngineRegisterProduct(user->radar->sweepEngine, user->productDescriptions[user->productCount]);
                    if (user->productIds[user->productCount]) {
                        sprintf(user->commandResponse, "ACK. {\"type\": \"productDescription\", \"symbol\":\"%s\", \"pid\":%d}" RKEOL,
                                user->productDescriptions[user->productCount].symbol, user->productIds[user->productCount]);
                        user->productCount++;
                    } else {
                        sprintf(user->commandResponse, "NAK. Unable to register product." RKEOL);
                    }
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;

                case 'x':
                    user->ascopeMode = RKNextModuloS(user->ascopeMode, 4);
                    sprintf(user->commandResponse, "ACK. AScope mode to %d" RKEOL, user->ascopeMode);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;
                    
                default:
                    RKExecuteCommand(user->radar, commandString, user->commandResponse);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;
            }
        } else if (user->radar->desc.initFlags & RKInitFlagRelay) {
            switch (commandString[0]) {
                case 'a':
                    RKLog("%s %s Queue command '%s' to relay.\n", engine->name, O->name, commandString);
                    RKRadarRelayExec(user->radar->radarRelay, commandString, user->commandResponse);
                    O->delimTx.type = RKNetworkPacketTypeControls;
                    O->delimTx.size = (uint32_t)strlen(user->commandResponse);
                    RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->commandResponse, O->delimTx.size, NULL);

                case 'r':
                    // Change radar
                    sscanf("%s", commandString + 1, sval1);
                    RKLog(">%s %s Selected radar %s\n", engine->name, O->name, sval1);
                    snprintf(user->commandResponse, RKMaximumPacketSize - 1, "ACK. %s selected." RKEOL, sval1);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;
                    
                case 's':
                    // Stream varrious data
                    user->streams = RKStreamFromString(commandString + 1);
                    
                    consolidateStreams(engine);

                    k = user->rayIndex;
                    pthread_mutex_lock(&user->mutex);
                    user->streamsInProgress = RKStreamNull;
                    pthread_mutex_unlock(&user->mutex);
                    RKLog(">%s %s Reset progress.\n", engine->name, O->name);
                    sprintf(user->commandResponse, "{\"type\": \"init\", \"access\": 0x%lx, \"streams\": 0x%lx, \"indices\":[%d,%d]}" RKEOL,
                            (unsigned long)user->access, (unsigned long)user->streams, k, user->rayIndex);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;
                    
                default:
                    // Just forward to the right radar
                    RKLog("%s %s Queue command '%s' to relay.\n", engine->name, O->name, commandString);
                    RKRadarRelayExec(user->radar->radarRelay, commandString, user->commandResponse);
                    RKOperatorSendCommandResponse(O, user->commandResponse);
                    break;
            }
        } else {
            RKLog("%s The radar is neither a DSP system nor a relay.\n", engine->name);
        }
        // Get to the next command
        if (commandStringEnd != NULL) {
            RKLog("Next command ...\n");
            commandString = commandStringEnd + 1;
            // Strip out some space after ';'
            while ((*commandString == '\r' || *commandString == '\n' || *commandString == ' ') && commandString < O->cmd + strlen(O->cmd) - 1) {
                commandString++;
            }
            if (*commandString == '\0') {
                commandString = NULL;
            }
        } else {
            commandString = NULL;
        }
    } // while (commandString != NULL) ...
    
    return 0;
}

int socketStreamHandler(RKOperator *O) {
    RKCommandCenter *engine = O->userResource;
    RKUser *user = &engine->users[O->iid];
    
    int i, j, k, s;
    char *c;
    static struct timeval t0;

    int gid;
    ssize_t size;
    uint32_t endIndex;

    gettimeofday(&t0, NULL);
    const double time = (double)t0.tv_sec + 1.0e-6 * (double)t0.tv_usec;

    RKPulse *pulse;
    RKPulseHeader pulseHeader;

    RKRay *ray;
    RKRayHeader rayHeader;
    
    RKSweep *sweep;
    RKSweepHeader sweepHeader;
    
    RKProduct *product;

    uint8_t *u8Data = NULL;
    float *f32Data = NULL;

    RKInt16C *c16DataH = NULL;
    RKInt16C *c16DataV = NULL;
    RKInt16C *userDataH = NULL;
    RKInt16C *userDataV = NULL;

    RKIdentifier identifier;
    RKProductId productId;

    struct timeval timevalOrigin, timevalTx, timevalRx;
    double deltaTx, deltaRx;

    if (engine->radarCount < 1) {
        return 0;
    }

    if (user->radar == NULL) {
        RKLog("User %s has no associated radar.\n", user->login);
        return 0;
    }

    pthread_mutex_lock(&user->mutex);

    if (!(user->radar->state & RKRadarStateLive)) {
        pthread_mutex_unlock(&user->mutex);
        return 0;
    }

    if (user->radar->desc.initFlags & RKInitFlagSignalProcessor && time - user->timeLastOut >= 0.05) {
        // Signal processor only - showing the latest summary text view
        k = user->streams & user->access & RKStreamStatusMask;
        if ((user->streamsInProgress & RKStreamStatusMask) != k && k != RKStreamStatusBuffers) {
            user->streamsInProgress |= k;
        }
        if (k == RKStreamStatusPositions) {
            // Stream "0" - Positions
            k = snprintf(user->string, RKMaximumStringLength - 1, "%s" RKEOL,
                         RKPositionEnginePositionString(user->radar->positionEngine));
            O->delimTx.type = RKNetworkPacketTypePlainText;
            O->delimTx.size = k + 1;
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            user->timeLastOut = time;
        } else if (k == RKStreamStatusPulses) {
            // Stream "1" - Pulses
            k = snprintf(user->string, RKMaximumStringLength - 1, "%s" RKEOL,
                         RKPulseCompressionEnginePulseString(user->radar->pulseCompressionEngine));
            O->delimTx.type = RKNetworkPacketTypePlainText;
            O->delimTx.size = k + 1;
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            user->timeLastOut = time;
        } else if (k == RKStreamStatusRays) {
            // Stream "2" - Rays (no skipping)
            j = 0;
            k = 0;
            endIndex = RKPreviousModuloS(user->radar->momentEngine->rayStatusBufferIndex, RKBufferSSlotCount);
            while (user->rayStatusIndex != endIndex && k < RKMaximumStringLength - 200) {
                c = user->radar->momentEngine->rayStatusBuffer[user->rayStatusIndex];
                k += sprintf(user->string + k, "%s\n", c);
                user->rayStatusIndex = RKNextModuloS(user->rayStatusIndex, RKBufferSSlotCount);
                j++;
            }
            if (j) {
                // Take out the last '\n', replace it with somethign else + EOL
                snprintf(user->string + k - 1, RKMaximumStringLength - k - 1, "" RKEOL);
                O->delimTx.type = RKNetworkPacketTypePlainText;
                O->delimTx.size = k + 1;
                RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
                user->timeLastOut = time;
            }
        } else if (k == RKStreamStatusIngest) {
            // Stream "3" - Overall status
            k = snprintf(user->string, RKMaximumStringLength - 1, "%s | %s | %s | %s | %s |" RKEOL,
                         RKPulseCompressionEngineStatusString(user->radar->pulseCompressionEngine),
                         RKPulseRingFilterEngineStatusString(user->radar->pulseRingFilterEngine),
                         RKPositionEngineStatusString(user->radar->positionEngine),
                         RKMomentEngineStatusString(user->radar->momentEngine),
                         RKRawDataRecorderStatusString(user->radar->rawDataRecorder));
            O->delimTx.type = RKNetworkPacketTypePlainText;
            O->delimTx.size = k + 1;
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            user->timeLastOut = time;
        } else if (k == RKStreamStatusEngines) {
            if (user->textPreferences & RKTextPreferencesShowColor) {
                k = snprintf(user->string, RKMaximumStringLength - 1, "%s%04x/%04d  %s%04x/%05d  %s%04x/%04d  %s%04x/%02d  %s%04x  %s%04x " RKEOL,
                             user->radar->positionEngine->name,
                             user->radar->positionEngine->state & 0xFFFF,
                             user->radar->positionIndex,
                             user->radar->pulseCompressionEngine->name,
                             user->radar->pulseCompressionEngine->state & 0xFFFF,
                             user->radar->pulseIndex,
                             user->radar->momentEngine->name,
                             user->radar->momentEngine->state & 0xFFFF,
                             user->radar->rayIndex,
                             user->radar->healthEngine->name,
                             user->radar->healthEngine->state & 0xFFFF,
                             user->radar->healthIndex,
                             user->radar->sweepEngine->name,
                             user->radar->sweepEngine->state & 0xFFFF,
                             user->radar->rawDataRecorder->name,
                             user->radar->rawDataRecorder->state & 0xFFFF);
            } else {
                k = snprintf(user->string, RKMaximumStringLength - 1, "Pos:%04x/%04d  Pul:%04x/%05d  Mom:%04x/%04d  Hea:%04x/%02d  Swe:%04x  Rec:%04x " RKEOL,
                             user->radar->positionEngine->state & 0xFFFF,
                             user->radar->positionIndex,
                             user->radar->pulseCompressionEngine->state & 0xFFFF,
                             user->radar->pulseIndex,
                             user->radar->momentEngine->state & 0xFFFF,
                             user->radar->rayIndex,
                             user->radar->healthEngine->state & 0xFFFF,
                             user->radar->healthIndex,
                             user->radar->sweepEngine->state & 0xFFFF,
                             user->radar->rawDataRecorder->state & 0xFFFF);
            }
            O->delimTx.type = RKNetworkPacketTypePlainText;
            O->delimTx.size = k + 1;
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            user->timeLastOut = time;
        } else if (k == RKStreamStatusBuffers) {
            if ((user->streamsInProgress & RKStreamStatusMask) != k) {
                user->streamsInProgress |= k;
                k = RKBufferOverview(user->radar, user->string,
                                     RKOverviewFlagDrawBackground | (user->textPreferences & RKTextPreferencesShowColor ? RKOverviewFlagShowColor : RKOverviewFlagNone));
            } else {
                k = RKBufferOverview(user->radar, user->string, user->textPreferences & RKTextPreferencesShowColor ? RKOverviewFlagShowColor : RKOverviewFlagNone);
            }
            O->delimTx.type = RKNetworkPacketTypePlainText;
            O->delimTx.size = k + 1;
            // Special case to avoid character 007, which is a beep.
            if ((O->delimTx.size & 0xFF) == 0x07) {
                O->delimTx.size++;
            }
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            user->timeLastOut = time;
        }

        // Send another set of controls if the radar controls have changed.
        if (user->controlFirstUID != user->radar->controls[0].uid && user->access & RKStreamControl) {
            user->controlFirstUID = user->radar->controls[0].uid;
            RKLog("%s %s Sending new controls.\n", engine->name, O->name);
            j = sprintf(user->string, "{\"Radars\": [");
            for (k = 0; k < engine->radarCount; k++) {
                RKRadar *radar = engine->radars[k];
                j += sprintf(user->string + j, "\"%s\", ", radar->desc.name);
            }
            if (k > 0) {
                j += sprintf(user->string + j - 2, "], ") - 2;
            } else {
                j += sprintf(user->string + j, "], ");
            }
            // Should only send the controls if the user has been authenticated
            RKMakeJSONStringFromControls(user->scratch, user->radar->controls, user->radar->controlCount);
            j += sprintf(user->string + j, "\"Controls\": ["
                        "{\"Label\": \"Go\", \"Command\": \"y\"}, "
                        "{\"Label\": \"Stop\", \"Command\": \"z\"}, "
                        "%s"
                        "]}" RKEOL, user->scratch);
            O->delimTx.type = RKNetworkPacketTypeControls;
            O->delimTx.size = j;
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            user->timeLastOut = time;
        }
    }

    // For contiguous streaming:
    // If we just started a connection, grab the payload that is either:
    // 1) Up to latest available:
    //      i) For a health, it is radar->healthIndex - 1
    //     ii) For a ray, it is radar->rayIndex - (number of workers)
    //    iii) For a pulse, it is radar->pulseIndex - (number of workers)
    //     iv) For a sweep, it is radar->sweepEngine->scratchSpaceIndex
    // 2) The latest slot it will be stored. It is crucial to ensure that:
    //      i) For a health, it is RKStatusReady
    //     ii) For a ray, it has RKRayStatusReady set
    //    iii) For a pulse, it has RKPulseStatusReadyForMoments set
    //     iv) For a sweep, scratchSpaceIndex has increased
    // 3) Once the first payload is sent, the stream is consider in progress (streamsInProgress)
    // 4) If (2) can't be met within X secs, in progress flag is not set so (2) will be checked
    //    again in the next iteraction.

    // Processor Status
    if (user->streams & user->access & RKStreamStatusProcessorStatus) {
        endIndex = RKPreviousModuloS(user->radar->statusIndex, user->radar->desc.statusBufferDepth);
        if (!(user->streamsInProgress & RKStreamStatusProcessorStatus)) {
            user->streamsInProgress |= RKStreamStatusProcessorStatus;
            if (engine->verbose) {
                RKLog("%s %s Streaming RKStatus -> %d (0x%x %s).\n", engine->name, O->name, endIndex,
                      user->radar->status[endIndex].flag,
                      user->radar->status[endIndex].flag == RKStatusFlagVacant ? "vacant" : "ready");
            }
            user->statusIndex = endIndex;
        }
        s = 0;
        while (user->radar->status[user->statusIndex].flag != RKStatusFlagReady && engine->server->state == RKServerStateActive && s++ < 20) {
            if (s % 10 == 0 && engine->verbose > 1) {
                RKLog("%s %s sleep 0/%.1f s  RKStatus\n", engine->name, O->name, s * 0.1f);
            }
            usleep(50000);
        }
        if (user->radar->status[user->statusIndex].flag == RKStatusFlagReady && engine->server->state == RKServerStateActive) {
            while (user->statusIndex != endIndex) {
                O->delimTx.type = RKNetworkPacketTypeProcessorStatus;
                O->delimTx.size = (uint32_t)sizeof(RKStatus);
                RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &user->radar->status[user->statusIndex], sizeof(RKStatus), NULL);
                user->statusIndex = RKNextModuloS(user->statusIndex, user->radar->desc.statusBufferDepth);
            }
        } else {
            RKLog("%s %s No Status / Deactivated.   statusIndex = %d\n", engine->name, O->name, user->statusIndex);
        }
    }
    
    // Health Status
    if (user->streams & user->access & RKStreamHealthInJSON) {
        endIndex = RKPreviousModuloS(user->radar->healthIndex, user->radar->desc.healthBufferDepth);
        if (!(user->streamsInProgress & RKStreamHealthInJSON)) {
            user->streamsInProgress |= RKStreamHealthInJSON;
            if (engine->verbose) {
                RKLog("%s %s Streaming RKHealth -> %d (0x%x %s).\n", engine->name, O->name, endIndex,
                      user->radar->healths[endIndex].flag,
                      user->radar->healths[endIndex].flag == RKStatusFlagVacant ? "vacant" : "ready");
            }
            user->healthIndex = endIndex;
        }
        s = 0;
        while (user->radar->healths[user->healthIndex].flag != RKHealthFlagReady && engine->server->state == RKServerStateActive && s++ < 20) {
            if (s % 10 == 0 && engine->verbose > 1) {
                RKLog("%s %s sleep 0/%.1f s  RKHealth\n", engine->name, O->name, s * 0.1f);
            }
            usleep(50000);
        }
        if (user->radar->healths[user->healthIndex].flag == RKHealthFlagReady && engine->server->state == RKServerStateActive) {
            j = 0;
            k = 0;
            while (user->healthIndex != endIndex && k < RKMaximumStringLength - 200) {
                c = user->radar->healths[user->healthIndex].string;
                k += sprintf(user->string + k, "%s\n", c);
                user->healthIndex = RKNextModuloS(user->healthIndex, user->radar->desc.healthBufferDepth);
                j++;
            }
            if (j) {
                // Take out the last '\n', replace it with somethign else + EOL
                snprintf(user->string + k - 1, RKMaximumStringLength - k - 1, "" RKEOL);
                O->delimTx.type = RKNetworkPacketTypeHealth;
                O->delimTx.size = k;
                RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), user->string, O->delimTx.size, NULL);
            }
        } else {
            RKLog("%s %s No Health / Deactivated.   healthIndex = %d / %d\n", engine->name, O->name, user->healthIndex, user->radar->healthIndex);
        }
    }

    // Product or display streams - no skipping
    if (user->streams & user->access & RKStreamProductZVWDPRKS) {
        // Product streams - assume no display as display data can be derived later
        if (user->radar->desc.initFlags & RKInitFlagSignalProcessor) {
            endIndex = RKPreviousNModuloS(user->radar->rayIndex, 2 * user->radar->momentEngine->coreCount, user->radar->desc.rayBufferDepth);
        } else {
            endIndex = RKPreviousModuloS(user->radar->rayIndex, user->radar->desc.rayBufferDepth);
        }
        ray = RKGetRay(user->radar->rays, endIndex);

        if (!(user->streamsInProgress & RKStreamProductZVWDPRKS)) {
            user->streamsInProgress |= (user->streams & RKStreamProductZVWDPRKS);
            user->rayIndex = endIndex;
            s = 0;
            while (!(ray->header.s & RKRayStatusReady) && engine->server->state == RKServerStateActive && s++ < 20) {
                if (s % 10 == 0 && engine->verbose > 1) {
                    RKLog("%s %s sleep 0/%.1f s  RKRay\n", engine->name, O->name, s * 0.1f);
                }
                usleep(50000);
            }
            if (engine->verbose) {
                RKLog("%s %s Streaming RKRay products -> %d (%s).\n", engine->name, O->name, endIndex,
                      ray->header.s & RKRayStatusReady ? "ready" : "not ready");
            }
        }

        if (ray->header.s & RKRayStatusReady && engine->server->state == RKServerStateActive) {
            while (user->rayIndex != endIndex) {
                ray = RKGetRay(user->radar->rays, user->rayIndex);
                // Duplicate and send the header with only selected products
                memcpy(&rayHeader, &ray->header, sizeof(RKRayHeader));
                // Gather the products to be sent
                rayHeader.baseMomentList = RKBaseMomentListNone;
                if (user->streams & RKStreamProductZ) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductZ;
                }
                if (user->streams & RKStreamProductV) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductV;
                }
                if (user->streams & RKStreamProductW) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductW;
                }
                if (user->streams & RKStreamProductD) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductD;
                }
                if (user->streams & RKStreamProductP) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductP;
                }
                if (user->streams & RKStreamProductR) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductR;
                }
                if (user->streams & RKStreamProductK) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductK;
                }
                if (user->streams & RKStreamProductSh) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductSh;
                }
                if (user->streams & RKStreamProductSv) {
                    rayHeader.baseMomentList |= RKBaseMomentListProductSv;
                }
                uint32_t productList = rayHeader.baseMomentList & RKBaseMomentListProductZVWDPRK;
                uint32_t productCount = __builtin_popcount(productList);
                //RKLog("ProductCount = %d / %x\n", productCount, productList);
                
                rayHeader.gateCount /= user->rayDownSamplingRatio;
                rayHeader.gateSizeMeters *= (float)user->rayDownSamplingRatio;
                
                O->delimTx.type = RKNetworkPacketTypeRayDisplay;
                O->delimTx.size = (uint32_t)(sizeof(RKRayHeader) + productCount * rayHeader.gateCount * sizeof(float));
                RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &rayHeader, sizeof(RKRayHeader), NULL);
                
                for (j = 0; j < productCount; j++) {
                    if (productList & RKBaseMomentListProductZ) {
                        productList ^= RKBaseMomentListProductZ;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexZ);
                    } else if (productList & RKBaseMomentListProductV) {
                        productList ^= RKBaseMomentListProductV;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexV);
                    } else if (productList & RKBaseMomentListProductW) {
                        productList ^= RKBaseMomentListProductW;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexW);
                    } else if (productList & RKBaseMomentListProductD) {
                        productList ^= RKBaseMomentListProductD;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexD);
                    } else if (productList & RKBaseMomentListProductP) {
                        productList ^= RKBaseMomentListProductP;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexP);
                    } else if (productList & RKBaseMomentListProductR) {
                        productList ^= RKBaseMomentListProductR;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexR);
                    } else if (productList & RKBaseMomentListProductK) {
                        productList ^= RKBaseMomentListProductK;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexK);
                    } else if (productList & RKBaseMomentListProductSh) {
                        productList ^= RKBaseMomentListProductSh;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexSh);
                    } else if (productList & RKBaseMomentListProductSv) {
                        productList ^= RKBaseMomentListProductSv;
                        f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexSv);
                    } else {
                        f32Data = NULL;
                    }
                    if (f32Data) {
                        float *lowRateData = (float *)user->string;
                        for (i = 0, k = 0; i < rayHeader.gateCount; i++, k += user->rayDownSamplingRatio) {
                            lowRateData[i] = f32Data[k];
                        }
                        RKOperatorSendPackets(O, lowRateData, rayHeader.gateCount * sizeof(float), NULL);
                    }
                }
                ray->header.s |= RKRayStatusStreamed;
                user->rayIndex = RKNextModuloS(user->rayIndex, user->radar->desc.rayBufferDepth);
            }
        } else {
            if ((int)ray->header.i > 0) {
                RKLog("%s %s No product ray / deactivated.  streamsInProgress = 0x%08x\n",
                      engine->name, O->name, user->streamsInProgress);
            }
        }
    } else if (user->streams & user->access & RKStreamDisplayZVWDPRKS) {
        // Display streams - no skipping
        if (user->radar->desc.initFlags & RKInitFlagSignalProcessor) {
            endIndex = RKPreviousNModuloS(user->radar->rayIndex, 2 * user->radar->momentEngine->coreCount, user->radar->desc.rayBufferDepth);
        } else {
            endIndex = RKPreviousModuloS(user->radar->rayIndex, user->radar->desc.rayBufferDepth);
        }
        if (endIndex >= user->radar->desc.rayBufferDepth) {
            RKLog("%s Error. endIndex = %s > %s\n", engine->name, RKIntegerToCommaStyleString(endIndex), RKIntegerToCommaStyleString(user->radar->desc.rayBufferDepth));
            RKLog("%s user->radar->rayIndex = %s / user->radar->desc.rayBufferDepth = %s", engine->name,
                  RKIntegerToCommaStyleString(user->radar->rayIndex),
                  RKIntegerToCommaStyleString(user->radar->desc.rayBufferDepth));
            endIndex = 0;
        }
        ray = RKGetRay(user->radar->rays, endIndex);

        if (!(user->streamsInProgress & RKStreamDisplayZVWDPRKS)) {
            user->streamsInProgress |= (user->streams & RKStreamDisplayZVWDPRKS);
            user->rayIndex = endIndex;
            s = 0;
            while (!(ray->header.s & RKRayStatusReady) && engine->server->state == RKServerStateActive && s++ < 20) {
                if (s % 10 == 0 && engine->verbose) {
                    RKLog("%s %s sleep 0/%.1f s  RKRay\n", engine->name, O->name, s * 0.1f);
                }
                usleep(50000);
            }
            if (engine->verbose) {
                RKLog("%s %s Streaming RKRay displays -> %d (0x%02x %s).\n", engine->name, O->name, endIndex,
                      ray->header.s, ray->header.s & RKRayStatusReady ? "ready" : "not ready");
            }
        }

        if (ray->header.s & RKRayStatusReady && engine->server->state == RKServerStateActive) {
            while (user->rayIndex != endIndex) {
                ray = RKGetRay(user->radar->rays, user->rayIndex);
                // Duplicate and send the header with only selected products
                memcpy(&rayHeader, &ray->header, sizeof(RKRayHeader));
                // Gather the products to be sent
                rayHeader.baseMomentList = RKBaseMomentListNone;
                if (user->streams & RKStreamDisplayZ) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayZ;
                }
                if (user->streams & RKStreamDisplayV) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayV;
                }
                if (user->streams & RKStreamDisplayW) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayW;
                }
                if (user->streams & RKStreamDisplayD) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayD;
                }
                if (user->streams & RKStreamDisplayP) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayP;
                }
                if (user->streams & RKStreamDisplayR) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayR;
                }
                if (user->streams & RKStreamDisplayK) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplayK;
                }
                if (user->streams & RKStreamDisplaySh) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplaySh;
                }
                if (user->streams & RKStreamDisplaySv) {
                    rayHeader.baseMomentList |= RKBaseMomentListDisplaySv;
                }
                uint32_t displayList = rayHeader.baseMomentList & RKBaseMomentListDisplayZVWDPRKS;
                uint32_t displayCount = __builtin_popcount(displayList);
                //RKLog("displayCount = %d / %x\n", productCount, productList);

                rayHeader.gateCount /= user->rayDownSamplingRatio;
                rayHeader.gateSizeMeters *= (float)user->rayDownSamplingRatio;

                O->delimTx.type = RKNetworkPacketTypeRayDisplay;
                O->delimTx.size = (uint32_t)(sizeof(RKRayHeader) + displayCount * rayHeader.gateCount * sizeof(uint8_t));
                RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &rayHeader, sizeof(RKRayHeader), NULL);

                for (j = 0; j < displayCount; j++) {
                    if (displayList & RKBaseMomentListDisplayZ) {
                        displayList ^= RKBaseMomentListDisplayZ;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexZ);
                    } else if (displayList & RKBaseMomentListDisplayV) {
                        displayList ^= RKBaseMomentListDisplayV;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexV);
                    } else if (displayList & RKBaseMomentListDisplayW) {
                        displayList ^= RKBaseMomentListDisplayW;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexW);
                    } else if (displayList & RKBaseMomentListDisplayD) {
                        displayList ^= RKBaseMomentListDisplayD;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexD);
                    } else if (displayList & RKBaseMomentListDisplayP) {
                        displayList ^= RKBaseMomentListDisplayP;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexP);
                    } else if (displayList & RKBaseMomentListDisplayR) {
                        displayList ^= RKBaseMomentListDisplayR;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexR);
                    } else if (displayList & RKBaseMomentListDisplayK) {
                        displayList ^= RKBaseMomentListDisplayK;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexK);
                    } else if (displayList & RKBaseMomentListDisplaySh) {
                        displayList ^= RKBaseMomentListDisplaySh;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexSh);
                    } else if (displayList & RKBaseMomentListDisplaySv) {
                        displayList ^= RKBaseMomentListDisplaySv;
                        u8Data = RKGetUInt8DataFromRay(ray, RKBaseMomentIndexSv);
                    } else {
                        u8Data = NULL;
                    }
                    if (u8Data) {
                        uint8_t *lowRateData = (uint8_t *)user->string;
                        for (i = 0, k = 0; i < rayHeader.gateCount; i++, k += user->rayDownSamplingRatio) {
                            lowRateData[i] = u8Data[k];
                        }
                        RKOperatorSendPackets(O, lowRateData, rayHeader.gateCount * sizeof(uint8_t), NULL);
                    }
                }
                ray->header.s |= RKRayStatusStreamed;
                user->rayIndex = RKNextModuloS(user->rayIndex, user->radar->desc.rayBufferDepth);
            } // while (user->rayIndex != endIndex) ...
        } else {
            if ((int)ray->header.i > 0) {
                RKLog("%s %s No display ray / deactivated.  user->rayIndex = %d   endIndex = %d  %lld\n",
                      engine->name, O->name, user->rayIndex, endIndex, ray->header.i);
            }
        } // if (ray->header.s & RKRayStatusReady && engine->server->state == RKServerStateActive) ...
    } // else if if (user->streams & user->access & RKStreamDisplayZVWDPRKS) ...

    // Sweep
    if (user->streams & user->access & RKStreamSweepZVWDPRKS) {
        // Sweep streams - no skipping
        if (user->scratchSpaceIndex != user->radar->sweepEngine->scratchSpaceIndex) {
            if (user->radar->sweepEngine->verbose > 1) {
                RKLog("%s RKSweepCollect()   anchorsIndex = %d / %d\n", engine->name, user->scratchSpaceIndex, user->radar->sweepEngine->scratchSpaceIndex);
            }
            sweep = RKSweepCollect(user->radar->sweepEngine, user->scratchSpaceIndex);
            if (sweep) {
                // Make a local copy of the sweepHeader and mutate it for this client while keeping the original intact
                memcpy(&sweepHeader, &sweep->header, sizeof(RKSweepHeader));

                if (engine->verbose > 1) {
                    RKLog("%s %s New sweep available   C%02d   S%lu.  <--  %x / %x\n",
                          engine->name, O->name,
                          sweep->rays[0]->header.configIndex, sweep->header.config.i, sweep->header.baseMomentList, sweepHeader.baseMomentList);
                }

                // Store a copy of the original list of available products
                uint32_t productList = sweepHeader.baseMomentList;

                // Mutate sweep so that header indicates the sweep to be transmitted
                i = 0;
                user->scratch[0] = '\0';
                sweepHeader.baseMomentList = RKBaseMomentListNone;
                if ((user->streams & RKStreamSweepZ) && (productList & RKBaseMomentListProductZ)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductZ;
                    i += sprintf(user->scratch + i, " Z,");
                }
                if ((user->streams & RKStreamSweepV) && (productList & RKBaseMomentListProductV)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductV;
                    i += sprintf(user->scratch + i, " V,");
                }
                if ((user->streams & RKStreamSweepW) && (productList & RKBaseMomentListProductW)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductW;
                    i += sprintf(user->scratch + i, " W,");
                }
                if ((user->streams & RKStreamSweepD) && (productList & RKBaseMomentListProductD)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductD;
                    i += sprintf(user->scratch + i, " D,");
                }
                if ((user->streams & RKStreamSweepP) && (productList & RKBaseMomentListProductP)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductP;
                    i += sprintf(user->scratch + i, " P,");
                }
                if ((user->streams & RKStreamSweepR) && (productList & RKBaseMomentListProductR)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductR;
                    i += sprintf(user->scratch + i, " R,");
                }
                if ((user->streams & RKStreamSweepK) && (productList & RKBaseMomentListProductK)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductK;
                    i += sprintf(user->scratch + i, " K,");
                }
                if ((user->streams & RKStreamSweepSh) && (productList & RKBaseMomentListProductSh)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductSh;
                    i += sprintf(user->scratch + i, " Sh,");
                }
                if ((user->streams & RKStreamSweepSv) && (productList & RKBaseMomentListProductSv)) {
                    sweepHeader.baseMomentList |= RKBaseMomentListProductSv;
                    i += sprintf(user->scratch + i, " Sv,");
                }
                // Remove the last ','
                if (i > 1) {
                    user->scratch[i - 1] = '\0';
                }

                const uint32_t baseMomentCount = __builtin_popcount(sweepHeader.baseMomentList);

                if (baseMomentCount) {
                    size = 0;
                    gettimeofday(&timevalOrigin, NULL);

                    O->delimTx.type = RKNetworkPacketTypeSweepHeader;
                    O->delimTx.size = (uint32_t)sizeof(RKSweepHeader);
                    size += RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &sweepHeader, sizeof(RKSweepHeader), NULL);

                    O->delimTx.type = RKNetworkPacketTypeSweepRay;
                    O->delimTx.size = (uint32_t)(sizeof(RKRayHeader) + baseMomentCount * sweepHeader.gateCount * sizeof(RKFloat));

                    for (k = 0; k < sweepHeader.rayCount; k++) {
                        ray = sweep->rays[k];
                        memcpy(&rayHeader, &ray->header, sizeof(RKRayHeader));
                        rayHeader.baseMomentList = sweepHeader.baseMomentList;
                        size += RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &rayHeader, sizeof(RKRayHeader), NULL);
                        productList = sweepHeader.baseMomentList;
                        if (engine->verbose > 1 && (k < 3 || k == sweepHeader.rayCount - 1)) {
                            RKLog(">%s %s k = %d   moments = %s   (%x)\n", engine->name, O->name, k, user->scratch + 1, productList);
                        }
                        for (j = 0; j < baseMomentCount; j++) {
                            if (productList & RKBaseMomentListProductZ) {
                                productList ^= RKBaseMomentListProductZ;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexZ);
                            } else if (productList & RKBaseMomentListProductV) {
                                productList ^= RKBaseMomentListProductV;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexV);
                            } else if (productList & RKBaseMomentListProductW) {
                                productList ^= RKBaseMomentListProductW;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexW);
                            } else if (productList & RKBaseMomentListProductD) {
                                productList ^= RKBaseMomentListProductD;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexD);
                            } else if (productList & RKBaseMomentListProductP) {
                                productList ^= RKBaseMomentListProductP;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexP);
                            } else if (productList & RKBaseMomentListProductR) {
                                productList ^= RKBaseMomentListProductR;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexR);
                            } else if (productList & RKBaseMomentListProductK) {
                                productList ^= RKBaseMomentListProductK;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexK);
                            } else if (productList & RKBaseMomentListProductSh) {
                                productList ^= RKBaseMomentListProductSh;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexSh);
                            } else if (productList & RKBaseMomentListProductSv) {
                                productList ^= RKBaseMomentListProductSv;
                                f32Data = RKGetFloatDataFromRay(ray, RKBaseMomentIndexSv);
                            } else {
                                f32Data = NULL;
                            }
                            if (f32Data) {
                                size += RKOperatorSendPackets(O, f32Data, sweep->header.gateCount * sizeof(float), NULL);
                                user->timeLastOut = time;
                            }
                        }
                    }

                    gettimeofday(&timevalTx, NULL);

                    // Offset scratch by one to get rid of the very first space character
                    RKLog("%s %s Sweep @ %s sent (%s)\n", engine->name, O->name,
                          RKVariableInString("configId", &sweepHeader.config.i, RKValueTypeUInt64) , user->scratch + 1);
                    if (engine->verbose > 1) {
                        RKLog(">%s %s user->streams = 0x%lx / 0x%lx\n", engine->name, O->name, user->streams, RKStreamSweepZVWDPRKS);
                        RKLog(">%s %s Sent a sweep of size %s B (%d moments)\n", engine->name, O->name, RKIntegerToCommaStyleString(size), baseMomentCount);
                    }

                    for (k = 0; k < user->productCount; k++) {
                        RKLog(">%s %s Expecting return for productId = %d ...\n", engine->name, O->name, user->productIds[k]);
                        size = RKServerReceiveUserPayload(O, user->string, RKNetworkMessageFormatHeaderDefinedSize);
                        if (size < 0) {
                            RKLog("%s %s Error. Failed receiving user product header ...\n", engine->name, O->name);
                            continue;
                        }
                        productId = RKProductIdFromString(RKGetValueOfKey(user->string, "productId"));
                        identifier = RKIdentifierFromString(RKGetValueOfKey(user->string, "configId"));
                        if (user->productIds[k] != productId) {
                            RKLog("%s %s Warning. Inconsistent productId = %d (expected) != %d (reported)\n", engine->name, O->name, user->productIds[k], productId);
                        } else if (sweep->header.config.i != identifier) {
                            RKLog("%s %s Warning. Inconsistent configId = %lu (expected) != %lu (reported)\n", engine->name, O->name, sweep->header.config.i, identifier);
                        } else {
                            product = RKSweepEngineGetVacantProduct(user->radar->sweepEngine, sweep, productId);
                            if (product) {
                                // Transfer important meta data and prepare the necessary buffer size
                                RKProductInitFromSweep(product, sweep);
                                size = RKServerReceiveUserPayload(O, product->data, RKNetworkMessageFormatHeaderDefinedSize);
                                if (user->radar->sweepEngine->verbose > 1) {
                                    RKLog("%s %s %s (%d) -> %u %zu\n",
                                          engine->name, O->name, user->string, strlen(user->string), productId, identifier);
                                }
                                RKShowArray(product->data, product->desc.symbol, product->header.gateCount, product->header.rayCount);
                                RKSweepEngineSetProductComplete(user->radar->sweepEngine, sweep, product);
                            } else {
                                // Still need to consume this packet so we dump it to a scratch space
                                RKLog("Warning. Unable to retrieve storage for incoming sweep.\n");
                                size = RKServerReceiveUserPayload(O, user->scratch, RKNetworkMessageFormatHeaderDefinedSize);
                            }
                            if (size < 0) {
                                RKLog("%s %s Error. Failed receiving user product data ...\n", engine->name, O->name);
                                continue;
                            }
                        } // else (user->productIds[k] == productId && sweep->header.config.i == identifier)
                    } // for (k = 0; k < user->productCount; k++) ...

                    gettimeofday(&timevalRx, NULL);

                    deltaTx = 1.0e3 * RKTimevalDiff(timevalTx, timevalOrigin);
                    deltaRx = 1.0e3 * RKTimevalDiff(timevalRx, timevalTx);
                    RKLog("%s %s Round trip finished   %s ms   %s ms\n", engine->name, O->name,
                          RKVariableInString("tx", &deltaTx, RKValueTypeDouble),
                          RKVariableInString("rx", &deltaRx, RKValueTypeDouble));

                } // if (baseMomentCount) ...
                RKSweepFree(sweep);
            } else if (engine->verbose > 1) {
                RKLog("%s %s Empty sweep   anchorIndex = %d.\n", engine->name, O->name, user->scratchSpaceIndex);
            } // if (sweep) ...
            // This scratchSpaceIndex is consumed, moving to the next one
            user->scratchSpaceIndex = RKNextModuloS(user->scratchSpaceIndex, RKSweepScratchSpaceDepth);
        } // if (user->scratchSpaceIndex != user->radar->sweepEngine->scratchSpaceIndex) ...
    } // if (user->streams & user->access & RKStreamSweepZVWDPRKS) ...

    // IQ
    if (user->streams & user->access & RKStreamProductIQ) {
        // If I/Q data is sent, there is no need to send another subset of it.
        endIndex = RKPreviousModuloS(user->radar->pulseIndex, user->radar->desc.pulseBufferDepth);
        while (user->pulseIndex != endIndex) {
            user->pulseIndex = RKNextModuloS(user->pulseIndex, user->radar->desc.pulseBufferDepth);
        }
        user->timeLastDisplayIQOut = time;
    } else if (user->streams & user->access & RKStreamDisplayIQ && time - user->timeLastDisplayIQOut >= 0.05) {
        if (user->radar->desc.initFlags & RKInitFlagSignalProcessor) {
            endIndex = RKPreviousNModuloS(user->radar->pulseIndex, 2 * user->radar->pulseCompressionEngine->coreCount, user->radar->desc.pulseBufferDepth);
        } else {
            endIndex = RKPreviousModuloS(user->radar->pulseIndex, user->radar->desc.pulseBufferDepth);
        }
        pulse = RKGetPulse(user->radar->pulses, endIndex);
        s = 0;
        while (!(pulse->header.s & RKPulseStatusProcessed) && s++ < 100) {
            usleep(1000);
        }

        if (!(user->streamsInProgress & RKPulseStatusProcessed)) {
            user->pulseIndex = endIndex;
            s = 0;
            while (!(pulse->header.s & RKPulseStatusHasIQData) && engine->server->state == RKServerStateActive && s++ < 20) {
                if (s % 10 == 0) {
                    RKLog("%s %s sleep 0/%.1f s  RKPulse\n", engine->name, O->name, s * 0.1f);
                }
                usleep(100000);
            }
        }

        if (pulse->header.s & RKPulseStatusProcessed && engine->server->state == RKServerStateActive) {
            user->streamsInProgress |= RKStreamDisplayIQ;
            memcpy(&pulseHeader, &pulse->header, sizeof(RKPulseHeader));
            c16DataH = RKGetInt16CDataFromPulse(pulse, 0);
            c16DataV = RKGetInt16CDataFromPulse(pulse, 1);
            userDataH = user->samples[0];
            userDataV = user->samples[1];
            RKComplex *yH;
            RKComplex *yV;
            float scale = 1.0f;

            // Default stride: k = 1
            k = 1;
            switch (user->ascopeMode) {
                case 3:
                    // Show the waveform that was used through the forward sampling path
                    pulseHeader.gateCount = 2000;
                    if (!(user->radar->desc.initFlags & RKInitFlagSignalProcessor)) {
                        break;
                    }
                    i = 0;
                    gid = pulse->header.i % user->radar->pulseCompressionEngine->filterGroupCount;
                    for (k = 0; k < MIN(400, user->radar->pulseCompressionEngine->filterAnchors[gid][0].length); k++) {
                        *userDataH++ = *c16DataH++;
                        *userDataV++ = *c16DataV++;
                        i++;
                    }
                    for (; k < MIN(410, user->radar->pulseCompressionEngine->filterAnchors[gid][0].length + 3); k++) {
                        userDataH->i   = 0;
                        userDataH++->q = 0;
                        userDataV->i   = 0;
                        userDataV++->q = 0;
                        i++;
                    }
                    scale = 10000.0f * sqrtf(user->radar->pulseCompressionEngine->filterAnchors[gid][0].length);
                    // Show the filter that was used as matched filter
                    yH = user->radar->pulseCompressionEngine->filters[gid][0];
                    yV = user->radar->pulseCompressionEngine->filters[gid][0];
                    for (k = 0; k < MIN(400, user->radar->pulseCompressionEngine->filterAnchors[gid][0].length); k++) {
                        userDataH->i   = (int16_t)(scale * yH->i);
                        userDataH++->q = (int16_t)(scale * yH++->q);
                        userDataV->i   = (int16_t)(scale * yV->i);
                        userDataV++->q = (int16_t)(scale * yV++->q);
                        i++;
                    }
                    for (; k < MIN(410, user->radar->pulseCompressionEngine->filterAnchors[gid][0].length + 3); k++) {
                        userDataH->i   = 0;
                        userDataH++->q = 0;
                        userDataV->i   = 0;
                        userDataV++->q = 0;
                        i++;
                    }
                    // Compute an appropriate normalization factor so that 16-bit view on the scope is okay
                    scale = 1.0f / sqrtf((float)user->radar->pulseCompressionEngine->filterAnchors[gid][0].length);
                    // The third part of is the processed data
                    yH = RKGetComplexDataFromPulse(pulse, 0);
                    yV = RKGetComplexDataFromPulse(pulse, 1);
                    for (; i < pulseHeader.gateCount; i++) {
                        userDataH->i   = (int16_t)(scale * yH->i);
                        userDataH++->q = (int16_t)(scale * yH++->q);
                        userDataV->i   = (int16_t)(scale * yV->i);
                        userDataV++->q = (int16_t)(scale * yV++->q);
                    }
                    break;

                case 2:
                    // Down-sampled output data
                    k = user->pulseDownSamplingRatio;

                    pulseHeader.gateCount /= k;
                    pulseHeader.gateSizeMeters *= (float)k;

                    scale = 1.0f;
                    yH = RKGetComplexDataFromPulse(pulse, 0);
                    yV = RKGetComplexDataFromPulse(pulse, 1);
                    for (i = 0; i < pulseHeader.gateCount; i++) {
                        userDataH->i   = (int16_t)(scale * yH->i);
                        userDataH++->q = (int16_t)(scale * yH->q);
                        yH += k;
                        userDataV->i   = (int16_t)(scale * yV->i);
                        userDataV++->q = (int16_t)(scale * yV->q);
                        yV += k;
                    }
                    break;

                case 1:
                    // Down-sampled raw input data
                    k = user->pulseDownSamplingRatio;

                default:
                    // Raw input data
                    // Note that at this point, the gateCount in header is describing the RKComplex data (compressed), not the RKIQZ raw data.
                    pulseHeader.gateCount = MIN(pulseHeader.gateCount * user->radar->desc.pulseToRayRatio / k, 2000);
                    pulseHeader.gateSizeMeters *= (float)k / user->radar->desc.pulseToRayRatio;
                    for (i = 0; i < pulseHeader.gateCount; i++) {
                        *userDataH++ = *c16DataH;
                        *userDataV++ = *c16DataV;
                        c16DataH += k;
                        c16DataV += k;
                    }
                    break;
            }
            
            size = pulseHeader.gateCount * sizeof(RKInt16C);
            
            O->delimTx.type = RKNetworkPacketTypePulseData;
            O->delimTx.size = (uint32_t)(sizeof(RKPulseHeader) + 2 * size);
            RKOperatorSendPackets(O, &O->delimTx, sizeof(RKNetDelimiter), &pulseHeader, sizeof(RKPulseHeader), user->samples[0], size, user->samples[1], size, NULL);
            
            user->timeLastDisplayIQOut = time;
            user->timeLastOut = time;
        } else {
            if ((int)pulse->header.i > 0) {
                RKLog("%s %s No IQ / Deactivated.  streamsInProgress = 0x%08x  header.s = %x\n",
                       engine->name, O->name, user->streamsInProgress, pulse->header.s);
            }
        }
    }

    user->tic++;

    pthread_mutex_unlock(&user->mutex);

    // Re-evaluate td = time - user->timeLastOut; send a beacon if nothing has been sent for a while
    if (time - user->timeLastOut >= 1.0) {
        if (O->beacon.type != RKNetworkPacketTypeBeacon) {
            RKLog("Beacon has been changed %d\n", O->beacon.type);
        }
        if (engine->verbose > 1) {
            RKLog("%s %s Beacon\n", engine->name, O->name);
        }
        size = RKOperatorSendBeacon(O);
        user->timeLastOut = time;
        if (size < 0) {
            RKLog("Beacon failed (r = %d).\n", size);
            RKOperatorHangUp(O);
        }
    }
    
    return 0;
}

int socketInitialHandler(RKOperator *O) {
    RKCommandCenter *engine = O->userResource;
    RKUser *user = &engine->users[O->iid];
    
    if (engine->radarCount == 0) {
        RKLog("%s No radar yet.\n", engine->name);
        return RKResultNoRadar;
    }
    
    memset(user, 0, sizeof(RKUser));
    user->access = RKStreamStatusAll;
    user->access |= RKStreamDisplayZVWDPRKS;
    user->access |= RKStreamProductZVWDPRKS;
    user->access |= RKStreamSweepZVWDPRKS;
    user->access |= RKStreamDisplayIQ | RKStreamProductIQ;
    user->textPreferences = RKTextPreferencesShowColor;
    user->radar = engine->radars[0];
    if (user->radar->desc.initFlags & RKInitFlagSignalProcessor) {
        user->rayDownSamplingRatio = (uint16_t)MAX(user->radar->desc.pulseCapacity / user->radar->desc.pulseToRayRatio / 1000, 1);
    } else {
        user->rayDownSamplingRatio = 1;
    }
    user->pulseDownSamplingRatio = (uint16_t)MAX(user->radar->desc.pulseCapacity / 1000, 1);
    user->ascopeMode = 0;
    pthread_mutex_init(&user->mutex, NULL);
    RKLog("%s %s Pul x %d   Ray x %d ...\n", engine->name, O->name, user->pulseDownSamplingRatio, user->rayDownSamplingRatio);

    snprintf(user->login, 63, "radarop");
    user->serverOperator = O;

    return RKResultSuccess;
}

int socketTerminateHandler(RKOperator *O) {
    int k;
    RKCommandCenter *engine = O->userResource;
    RKUser *user = &engine->users[O->iid];
    user->access = RKStreamNull;
    user->streams = RKStreamNull;
    RKLog(">%s %s Stream reset.\n", engine->name, O->name);
    for (k = 0; k < user->productCount; k++) {
        if (user->productIds[k]) {
            RKSweepEngineUnregisterProduct(user->radar->sweepEngine, user->productIds[k]);
            user->productIds[k] = 0;
        }
    }
    pthread_mutex_destroy(&user->mutex);
    user->radar = NULL;
    consolidateStreams(engine);
    return RKResultSuccess;
}

#pragma mark - Life Cycle

RKCommandCenter *RKCommandCenterInit(void) {
    RKCommandCenter *engine = (RKCommandCenter *)malloc(sizeof(RKCommandCenter));
    if (engine == NULL) {
        RKLog("Error. Unable to allocate local command center.\n");
        return NULL;
    }
    memset(engine, 0, sizeof(RKCommandCenter));
    sprintf(engine->name, "%s<OperationCenter>%s",
            rkGlobalParameters.showColor ? RKGetBackgroundColorOfIndex(RKEngineColorCommandCenter) : "",
            rkGlobalParameters.showColor ? RKNoColor : "");
    engine->verbose = 3;
    engine->memoryUsage = sizeof(RKCommandCenter);
    engine->server = RKServerInit();
    RKServerSetName(engine->server, engine->name);
    RKServerSetWelcomeHandler(engine->server, &socketInitialHandler);
    RKServerSetCommandHandler(engine->server, &socketCommandHandler);
    RKServerSetStreamHandler(engine->server, &socketStreamHandler);
    RKServerSetTerminateHandler(engine->server, &socketTerminateHandler);
    RKServerSetSharedResource(engine->server, engine);
    return engine;
}

void RKCommandCenterFree(RKCommandCenter *engine) {
    RKServerFree(engine->server);
    free(engine);
    return;
}

#pragma mark - Properties

void RKCommandCenterSetVerbose(RKCommandCenter *engine, const int verbose) {
    RKServer *server = engine->server;
    server->verbose = verbose;
    engine->verbose = verbose;
}

void RKCommandCenterAddRadar(RKCommandCenter *engine, RKRadar *radar) {
    if (engine->radarCount >= 4) {
        RKLog("%s unable to add another radar.\n", engine->name);
    }
    engine->radars[engine->radarCount++] = radar;
}

void RKCommandCenterRemoveRadar(RKCommandCenter *engine, RKRadar *radar) {
    int i, j, k;
    if (engine->suspendHandler) {
        RKLog("Wait for command center.\n");
        int s = 0;
        while (++s < 10 && engine->suspendHandler) {
            usleep(100000);
        }
        if (s == 10) {
            RKLog("Should not happen.");
            exit(EXIT_FAILURE);
        }
    }
    engine->suspendHandler = true;
    for (i = 0; i < engine->radarCount; i++) {
        if (engine->radars[i] == radar) {
            RKLog("%s Removing '%s' ...\n", engine->name, radar->desc.name);
            while (i < engine->radarCount - 1) {
                engine->radars[i] = engine->radars[i + 1];
            }
            engine->radarCount--;
        }
    }
    for (i = 0; i < engine->server->maxClient; i++) {
        if (engine->server->busy[i] && engine->users[i].serverOperator->state == RKOperatorStateActive && engine->users[i].radar == radar) {
            RKLog("%s Removing '%s' from user %s %s ...\n", engine->name, radar->desc.name, engine->users[i].serverOperator->name, engine->users[i].login);
            engine->users[i].radar = NULL;
        }
    }
    if (engine->radarCount) {
        j = 0;
        char string[RKMaximumStringLength];
        for (k = 0; k < engine->radarCount; k++) {
            RKRadar *radar = engine->radars[k];
            j += snprintf(string + j, RKMaximumStringLength - j - 1, "%d. %s\n", k, radar->desc.name);
        }
        printf("Remaining radars\n================\n%s", string);
    }
    engine->suspendHandler = false;
}

#pragma mark - Interactions

void RKCommandCenterStart(RKCommandCenter *center) {
    RKLog("%s Starting ...\n", center->name);
    RKServerStart(center->server);
    RKLog("%s Started.   mem = %s B   radarCount = %s\n", center->name, RKUIntegerToCommaStyleString(center->memoryUsage), RKIntegerToCommaStyleString(center->radarCount));
}

void RKCommandCenterStop(RKCommandCenter *center) {
    if (center->verbose > 1) {
        RKLog("%s Stopping ...\n", center->name);
    }
    RKServerStop(center->server);
    RKLog("%s Stopped.\n", center->name);
}

void RKCommandCenterSkipToCurrent(RKCommandCenter *engine, RKRadar *radar) {
    int i, s;
    if (!(radar->desc.initFlags & RKInitFlagSignalProcessor)) {
        RKLog("Radar '%s' is not a Signal Processor.\n", radar->name);
        return;
    }

    s = 0;
    while (radar->pulseCompressionEngine->tic <= (2 * radar->pulseCompressionEngine->coreCount + 1) ||
           radar->rayIndex < 2 * radar->momentEngine->coreCount ||
           radar->healthEngine->tic < 2) {
        if (++s % 10 == 0 && engine->verbose > 1) {
            RKLog("%s Sleep 1/%.1f s    %d / %d / %d\n", engine->name, 0.01f * s,
                  radar->pulseCompressionEngine->tic,
                  radar->rayIndex,
                  radar->healthEngine->tic);
        }
        usleep(10000);
    }

    for (i = 0; i < RKCommandCenterMaxConnections; i++) {
        RKUser *user = &engine->users[i];
        if (user->radar != radar) {
            continue;
        }
        pthread_mutex_lock(&user->mutex);
        user->pulseIndex      = RKPreviousNModuloS(radar->pulseIndex, 2 * radar->pulseCompressionEngine->coreCount, radar->desc.pulseBufferDepth);
        user->rayIndex        = RKPreviousNModuloS(radar->rayIndex, 2 * radar->momentEngine->coreCount, radar->desc.rayBufferDepth);
        user->healthIndex     = RKPreviousModuloS(radar->healthIndex, radar->desc.healthBufferDepth);
        user->rayStatusIndex  = RKPreviousModuloS(user->radar->momentEngine->rayStatusBufferIndex, RKBufferSSlotCount);
        user->scratchSpaceIndex = user->radar->sweepEngine->scratchSpaceIndex;
        pthread_mutex_unlock(&user->mutex);
        if (user->pulseIndex > radar->desc.pulseBufferDepth ||
            user->rayIndex > 2 * radar->momentEngine->coreCount ||
            user->healthIndex > 2) {
            RKLog("%s Warning. pulse @ %s / %s   ray @ %s / %s   health @ %s\n",
                  user->serverOperator->name,
                  RKIntegerToCommaStyleString(user->pulseIndex), RKIntegerToCommaStyleString(radar->desc.pulseBufferDepth),
                  RKIntegerToCommaStyleString(user->rayIndex), RKIntegerToCommaStyleString(2 * radar->momentEngine->coreCount),
                  RKIntegerToCommaStyleString(user->healthIndex));
        }
    }
}
