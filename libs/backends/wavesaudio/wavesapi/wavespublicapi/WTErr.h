/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

///////////////////////////////////////////////////////////////////////////////////////////////////////
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

