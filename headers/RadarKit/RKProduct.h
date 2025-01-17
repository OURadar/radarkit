//
//  RKProduct.h
//  RadarKit
//
//  Created by Boonleng Cheong on 6/23/18.
//  Copyright © Boonleng Cheong. All rights reserved.
//

#ifndef __RadarKit_Product__
#define __RadarKit_Product__

#include <RadarKit/RKFoundation.h>

size_t RKProductBufferAlloc(RKProduct **, const uint32_t depth, const uint32_t rayCount, const uint32_t gateCount);
size_t RKProductBufferExtend(RKProduct **, const uint32_t depth, const uint32_t extension);
void RKProductBufferFree(RKProduct *, const uint32_t depth);

int RKProductInitFromSweep(RKProduct *, const RKSweep *);
void RKProductFree(RKProduct *);

#endif
