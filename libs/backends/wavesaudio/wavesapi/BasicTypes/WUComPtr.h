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

#ifndef __WUComPtr_h__
#define __WUComPtr_h__
	
/* Copy to include
#include "BasicTypes/WUComPtr.h"
*/

#include "WavesPublicAPI/wstdint.h"

typedef int32_t wvComPtr[2]; 

// ConvertDPtr has the exact format of a vfp callback function, but it is a local function, native only.
// It converts a pointer in either 32 bits or 64 bits to a place-holder of 64 bits in coefs/states/external memory.
// pData is expected to point to a pre-allocate space enough for storing a pointer (posibly two single-precision coefs).
// Since pointers are not transferable between hardwares, at preset time no need for a shell callback.
// We keep this as a cALGORITHM for compatibility with the rest of the convert functions
//================================================================================
inline uint32_t vfpConvertDPtr(const void* InPointer, void* pData)
//================================================================================
{	
    uint64_t *pL = (uint64_t *)pData;
    *pL = (uint64_t)InPointer;
    return (uint32_t)sizeof(uint64_t);
}


/*
{
	// data in that struct must be the same type of the Coefs/States type!
	int32_t LSW; // Least significant word
	int32_t MSW; // Most  significant word
};

inline wvComPtr PackToComPtr(const intptr_t in_PtrToPack)
// take ptr that hosted in intptr_t type
// and pack it to wvComPtr container type (MSW and LSW of 32bit each)
{
	wvComPtr retVal;
	int64_t t_PtrToPack = static_cast<int64_t>(in_PtrToPack);
	// This packing is xPlatform coding for x32 and x64
	// #ifdef for x64 - intptr_t is 64 bit
	retVal.LSW = static_cast<int32_t>(t_PtrToPack & intptr_t(0xFFFFFFFF));
	retVal.MSW = (static_cast<int32_t>(t_PtrToPack>>32));

	// #ifdef for x32 - intptr_t is 32 bit
//	retVal.LSW = int32_t(in_PtrToPack);
//	retVal.MSW = 0;
	
	return retVal;
}

inline intptr_t UnpackComPtr( const wvComPtr in_ComPtrToUnpack)
// take wvComPtr with MSW and LSW of 32bit each
// and unpack it to intptr_t type
{
	intptr_t retVal;
	
	// This unpacking is xPlatform coding for x32 and x64
	// #ifdef for x64 - intptr_t is 64 bit so use intptr_t instead of int64_t
	int64_t PtrAt64 = static_cast<int64_t>(in_ComPtrToUnpack.MSW);
	PtrAt64 <<= 32;
	PtrAt64	|= static_cast<int64_t>(in_ComPtrToUnpack.LSW);
	retVal = static_cast<intptr_t>(PtrAt64);


	// #ifdef for x32 - intptr_t is 32 bit
//	retVal = static_cast<intptr_t>(retVal.LSW);

	return retVal;
}


//////////////////////////////////////////////////////////////////////////
inline uint32_t  ComPtr_to_DSP( const intptr_t PtrToConvert, char* pDataStruct )
{

	*(reinterpret_cast<wvComPtr *>(pDataStruct)) = PackToComPtr(PtrToConvert);

	return uint32_t(sizeof(wvComPtr));
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
inline uint32_t  DSP_to_ComPtr( const char* pDataStruct, intptr_t *ThePtr)
// pDataStruct is pointing to wvComPtr in the Coefs/States
// the function reconstruct the pointer into ThePtr 
{

	*ThePtr = UnpackComPtr(*(reinterpret_cast<const wvComPtr *>(pDataStruct)));

	return uint32_t(sizeof(wvComPtr));
}
//////////////////////////////////////////////////////////////////////////
*/

#endif //#if !defined(__WUComPtr_h__)



