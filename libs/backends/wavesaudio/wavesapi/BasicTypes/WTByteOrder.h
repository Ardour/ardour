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

#if !defined(__WTByteOrder_h__)
#define __WTByteOrder_h__

/* Copy to include
#include "BasicTypes/WTByteOrder.h"
*/

#include "WavesPublicAPI/wstdint.h"
#include "BasicTypes/WUDefines.h"

// Stuff concerning little/big endian and the conversion between them.
// most of the code here was copied from NetShell with some modifications
// Written by Udi on Nov-2005
// Adjusted to Cross platform by Shai Mar-2006

// Macros to determine endian.  __BIG_ENDIAN__ & __LITTLE_ENDIAN__ should come from the compiler.
// We try to set the macro _BYTEORDER_BIG_ENDIAN to 1 if big-endian or to 0 if little-endian.

// if the compiler properly has set either __BIG_ENDIAN__ or __LITTLE_ENDIAN__
#if defined(__BIG_ENDIAN__) || defined(__LITTLE_ENDIAN__)
#if defined(__BIG_ENDIAN__) && defined(__LITTLE_ENDIAN__) //if both defined, check them as booleans
#if __BIG_ENDIAN__ && !__LITTLE_ENDIAN__
#define _BYTEORDER_BIG_ENDIAN 1
#elif !__BIG_ENDIAN__ && __LITTLE_ENDIAN__
#define _BYTEORDER_BIG_ENDIAN 0
#else
#error I am confused. Is this big-endian or little-endian?
#endif  // stupid compiler defines both __LITTLE_ENDIAN__ and __BIG_ENDIAN__
#elif defined(__BIG_ENDIAN__)
#define _BYTEORDER_BIG_ENDIAN 1
#else
#define _BYTEORDER_BIG_ENDIAN 0
#endif // big/little switch
#else // if the compiler proparly has NOT set either __BIG_ENDIAN__ or __LITTLE_ENDIAN__
// http://msdn.microsoft.com/en-us/library/b0084kay.aspx for all preprocessor defs. _M_X64: 64 bit. _M_IA64: Itanium 64bit 
#if defined(__i386__) || defined(__i386) || defined(_M_IX86) || defined(__INTEL__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_IA64)
#define _BYTEORDER_BIG_ENDIAN 0
#elif defined(_M_PPC) || defined(__POWERPC__ ) || defined(__ppc__)
#define _BYTEORDER_BIG_ENDIAN 1
#else
#error Cannot detect compiler byte-order. Please add a test for your compiler appropriate symbol to this header file.
#endif // symbol search
#endif // standard preprocessor symbol found

// code to determine which assembly code we can use
#if defined(_MSC_VER) && defined(_M_IX86)
#define _BYTEORDER_ASM_MSVC_I386  1  // Windows
#elif defined(__GNUC__) && defined(__i386__)
#define _BYTEORDER_ASM_GNUC_I386  1  // Linux, or MacOS with MacIntel on Xcode
#define _BYTEORDER_ASM_NONE		  1	 // Currently we have no assebley for GNU i386, so use the C version
#elif defined(__GNUC__) && defined(__POWERPC__)
#define _BYTEORDER_ASM_GNUC_PPC   1  // MacOS with PPC on Xcode
#define _BYTEORDER_ASM_NONE		  1	 // Currently we have no assebley for GNU PPC, so use the C version
#else
#define _BYTEORDER_ASM_NONE       1  // don't know the compiler and processor, use C implementation
#endif

namespace wvNS {
	
namespace wvBO // namespace Waves::ByteOrder
{
    typedef int		byte_order_type;   // we use int rather than enum because some compilers cannot resolve enum constants at compile-time. There are only two options anyway :-)
    static const	byte_order_type byte_order_little_endian = 0;
    static const	byte_order_type byte_order_big_endian    = 1;


    // We try to use this static const rather than preprocessor symbols in our code wherever possible.
#if _BYTEORDER_BIG_ENDIAN == 1
    static const	byte_order_type compiler_byte_order = byte_order_big_endian;
#else 
    static const	byte_order_type compiler_byte_order = byte_order_little_endian;
#endif


    //---------------------------------------------------------------------------------
    // swap functions - best if implemented in inline assembly code
    // The following are very slow swappers when compiled, do not use in loops
#if _BYTEORDER_ASM_MSVC_I386

    // assembly implementation for Intel386 on Visual Studio
    inline uint16_t swap16(uint16_t x)
    {
        __asm MOV AX,x;
        __asm XCHG AL,AH;
        __asm MOV x,AX;
        return x;
    }

    inline uint32_t swap32(uint32_t x)
    {
        __asm MOV EAX,x;
        __asm BSWAP EAX;
        __asm MOV x,EAX;
        return x;
    }
    inline uint64_t swap64(uint64_t x)  // TODO: To be replaced
    {   
        return 
            ((x>>7*8)&0xFF)<<0*8 | ((x>>6*8)&0xFF)<<1*8 | ((x>>5*8)&0xFF)<<2*8 | ((x>>4*8)&0xFF)<<3*8 |
            ((x>>3*8)&0xFF)<<4*8 | ((x>>2*8)&0xFF)<<5*8 | ((x>>1*8)&0xFF)<<6*8 | ((x>>0*8)&0xFF)<<7*8 ;
    }

    /* the ASM code for swap64 does not compile
    inline uint64_t swap64(uint64_t x)
    {
    __asm MOV EBX, OFFSET x;
    __asm MOV EAX, [EBX];
    __asm MOV EDX, [EBX+4];
    __asm BSWAP EAX;
    __asm BSWAP EDX;
    __asm MOV [EBX],EDX;
    __asm MOV [EBX+4],EAX;
    return x;
    }
    */
#endif // _BYTEORDER_ASM_MSVC_I386

#if _BYTEORDER_ASM_GNUC_I386
    // assembly implementation for Intel386 on GCC (Linux)
    // TODO
#endif // _BYTEORDER_ASM_GNUC_I386

#if _BYTEORDER_ASM_GNUC_PPC
    // assembly implementation for PowerPC on GCC (XCode)
    // TODO
#endif // _BYTEORDER_ASM_GNUC_PPC

#if _BYTEORDER_ASM_NONE 
    inline uint16_t swap16(uint16_t x) { return (x>>8) | ((x&0xFF)<<8); }
    inline uint32_t swap32(uint32_t x) { return (x&0xFF)<<24 | (x&0xFF00)<<8 | (x&0xFF0000)>>8 | (x&0xFF000000)>>24; }
    inline uint64_t swap64(uint64_t x)
    {   
        return 
            ((x>>7*8)&0xFF)<<0*8 | ((x>>6*8)&0xFF)<<1*8 | ((x>>5*8)&0xFF)<<2*8 | ((x>>4*8)&0xFF)<<3*8 |
            ((x>>3*8)&0xFF)<<4*8 | ((x>>2*8)&0xFF)<<5*8 | ((x>>1*8)&0xFF)<<6*8 | ((x>>0*8)&0xFF)<<7*8 ;
    }
#endif // _BYTEORDER_ASM_NONE




    //---------------------------------------------------------------------------------

    // order conversion functions
    //    may want to overload for float and double as well. 
    //    overload for signed ints is ambiguous and should be done only if no other choice exists.
    // - - - - - - - - - - - - - - - - - - - -
    inline uint16_t compiler_to_big_16(uint16_t x)
    {
        return compiler_byte_order==byte_order_big_endian ? x : swap16(x);
    }
    inline uint16_t big_to_compiler_16(uint16_t x)
    {
        return compiler_byte_order==byte_order_big_endian ? x : swap16(x);
    }
    inline uint16_t compiler_to_little_16(uint16_t x)
    {
        return compiler_byte_order==byte_order_little_endian ? x : swap16(x);
    }
    inline uint16_t little_to_compiler_16(uint16_t x)
    {
        return compiler_byte_order==byte_order_little_endian ? x : swap16(x);
    }
    // - - - - - - - - - - - - - - - - - - - -
    inline uint32_t compiler_to_big_32(uint32_t x)
    {
        return compiler_byte_order==byte_order_big_endian ? x : swap32(x);
    }
    inline uint32_t big_to_compiler_32(uint32_t x)
    {
        return compiler_byte_order==byte_order_big_endian ? x : swap32(x);
    }
    inline uint32_t compiler_to_little_32(uint32_t x)
    {
        return compiler_byte_order==byte_order_little_endian ? x : swap32(x);
    }
    inline uint32_t little_to_compiler_32(uint32_t x)
    {
        return compiler_byte_order==byte_order_little_endian ? x : swap32(x);
    }
    // - - - - - - - - - - - - - - - - - - - -
    inline uint64_t compiler_to_big_64(uint64_t x)
    {
        return compiler_byte_order==byte_order_big_endian ? x : swap64(x);
    }
    inline uint64_t big_to_compiler_64(uint64_t x)
    {
        return compiler_byte_order==byte_order_big_endian ? x : swap64(x);
    }
    inline uint64_t compiler_to_little_64(uint64_t x)
    {
        return compiler_byte_order==byte_order_little_endian ? x : swap64(x);
    }
    inline uint64_t little_to_compiler_64(uint64_t x)
    {
        return compiler_byte_order==byte_order_little_endian ? x : swap64(x);
    }

} // namespace wvBO

} // namespace wvNS {

#endif // #if !defined(__WTByteOrder_h__)

