///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2011 Waves Audio Ltd. All rights reserved.
// \file WTErr.h, defines basic error type and "No Error" code
// All users may use their own error codes with this type, as long as eNoErr remains defined here
///////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __WTErr_h__
#define __WTErr_h__

/* Copy to include:
#include "WavesPublicAPI/WTErr.h"
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "WavesPublicAPI/wstdint.h"

typedef int32_t	WTErr; // Waves Type Error
const WTErr eNoErr =  0;


#ifdef __cplusplus
} //extern "C" {
#endif

#endif // __WTErr_h__

