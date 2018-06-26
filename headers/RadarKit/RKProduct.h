//
//  RKProduct.h
//  RadarKit
//
//  Created by Boon Leng Cheong on 6/23/18.
//  Copyright © Boon Leng Cheong. All rights reserved.
//

#ifndef __RadarKit_Product__
#define __RadarKit_Product__

#include <RadarKit/RKFoundation.h>

size_t  RKProductBufferAlloc(RKProduct **, const int);

int RKProductInitFromSweep(RKProduct *, const RKSweep *);

void RKProductFree(RKProduct *);

#endif
