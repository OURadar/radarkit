//
//  RKPedestalPedzy.c
//  RadarKit
//
//  Created by Boon Leng Cheong on 1/4/17.
//  Copyright © 2017 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit/RKPedestalPedzy.h>

#pragma mark - Internal Functions

// Internal Implementations

int RKPedestalPedzyRead(RKClient *client) {
    RKRadar *radar = client->userResource;

    RKPosition *position = (RKPosition *)client->userPayload;

    if (radar->desc.initFlags & RKInitFlagVeryVerbose) {
        RKLog("Position %08x EL %.2f  AZ %.2f\n", position->flag, position->elevationDegrees, position->azimuthDegrees);
    }
    RKPosition *newPosition = RKGetVacantPosition(radar);
    memcpy(newPosition, position, sizeof(RKPosition));
    RKSetPositionReady(radar, newPosition);

    return 0;
}

#pragma mark - Protocol Implementations

// Implementations

RKPedestal RKPedestalPedzyInit(RKRadar *radar, void *input) {
    RKPedestalPedzy *me = (RKPedestalPedzy *)malloc(sizeof(RKPedestalPedzy));
    if (me == NULL) {
        RKLog("Error. Unable to allocated RKPedestalPedzy.\n");
        return NULL;
    }
    memset(me, 0, sizeof(RKPedestalPedzy));
    
    RKClientDesc desc;
    memset(&desc, 0, sizeof(RKClientDesc));
    sprintf(desc.name, "%s<PedzyRelay>%s",
            rkGlobalParameters.showColor ? RKGetBackgroundColor() : "",
            rkGlobalParameters.showColor ? RKNoColor : "");
    strncpy(desc.hostname, (char *)input, RKMaximumStringLength - 1);
    char *colon = strstr(desc.hostname, ":");
    if (colon != NULL) {
        *colon = '\0';
        sscanf(colon + 1, "%d", &desc.port);
    } else {
        desc.port = 9000;
    }
    desc.type = RKNetworkSocketTypeTCP;
    desc.format = RKNetworkMessageFormatConstantSize;
    desc.blocking = true;
    desc.reconnect = true;
    desc.blockLength = sizeof(RKPosition);
    desc.timeoutSeconds = RKNetworkTimeoutSeconds;
    desc.verbose = 1;
    
    me->client = RKClientInitWithDesc(desc);

    RKClientSetUserResource(me->client, radar);
    RKClientSetReceiveHandler(me->client, &RKPedestalPedzyRead);
    RKClientStart(me->client);

    return (RKPedestal)me;
}

int RKPedestalPedzyExec(RKPedestal input, const char *command) {
    RKPedestalPedzy *me = (RKPedestalPedzy *)input;
    RKClient *client = me->client;
    if (client->verbose > 1) {
        RKLog("%s received '%s'", client->name, command);
    }
    if (!strcmp(command, "disconnect")) {
        RKClientStop(client);
    } else {
        RKNetworkSendPackets(client->sd, command, strlen(command), NULL);
    }
    return RKResultNoError;
}

int RKPedestalPedzyFree(RKPedestal input) {
    RKPedestalPedzy *me = (RKPedestalPedzy *)input;
    RKClientFree(me->client);
    free(me);
    return RKResultNoError;
}
