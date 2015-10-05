/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	CAXException.h

=============================================================================*/

#ifndef __CAXException_h__
#define __CAXException_h__

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreServices/CoreServices.h>
#else
	#include <ConditionalMacros.h>
	#include <CoreServices.h>
#endif
#include "CADebugMacros.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// An extended exception class that includes the name of the failed operation
class CAXException {
public:
	CAXException(const char *operation, OSStatus err) :
		mError(err)
		{
			if (operation == NULL)
				mOperation[0] = '\0';
			else if (strlen(operation) >= sizeof(mOperation)) {
				memcpy(mOperation, operation, sizeof(mOperation) - 1);
				mOperation[sizeof(mOperation) - 1] = '\0';
			} else
				strcpy(mOperation, operation);
		}

	char *FormatError(char *str) const
	{
		return FormatError(str, mError);
	}

	char				mOperation[256];
	const OSStatus		mError;

	// -------------------------------------------------

	typedef void (*WarningHandler)(const char *msg, OSStatus err);

	/*static void Throw(const char *operation, OSStatus err)
	{
		throw CAXException(operation, err);
	}*/

	static char *FormatError(char *str, OSStatus error)
	{
		// see if it appears to be a 4-char-code
		*(UInt32 *)(str + 1) = EndianU32_NtoB(error);
		if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
			str[0] = str[5] = '\'';
			str[6] = '\0';
		} else
			// no, format it as an integer
			sprintf(str, "%ld", error);
		return str;
	}

	static void Warning(const char *s, OSStatus error)
	{
		if (sWarningHandler)
			(*sWarningHandler)(s, error);
	}

	static void SetWarningHandler(WarningHandler f) { sWarningHandler = f; }
private:
	static WarningHandler	sWarningHandler;
};

#if	DEBUG || CoreAudio_Debug
	#define XThrowIfError(error, operation) \
		do {																	\
			OSStatus __err = error;												\
			if (__err) {															\
				char __buf[12];													\
				DebugMessageN2("error %s: %4s\n", CAXException::FormatError(__buf, __err), operation);\
				STOP;															\
				throw CAXException(operation, __err);		\
			}																	\
		} while (0)

	#define XThrowIf(condition, error, operation) \
		do {																	\
			if (condition) {													\
				OSStatus __err = error;											\
				char __buf[12];													\
				DebugMessageN2("error %s: %4s\n", CAXException::FormatError(__buf, __err), operation);\
				STOP;															\
				throw CAXException(operation, __err);		\
			}																	\
		} while (0)

#else
	#define XThrowIfError(error, operation) \
		do {																	\
			OSStatus __err = error;												\
			if (__err) {															\
				throw CAXException(operation, __err);		\
			}																	\
		} while (0)

	#define XThrowIf(condition, error, operation) \
		do {																	\
			if (condition) {													\
				OSStatus __err = error;											\
				throw CAXException(operation, __err);		\
			}																	\
		} while (0)

#endif

#define XThrow(error, operation) XThrowIf(true, error, operation)
#define XThrowIfErr(error) XThrowIfError(error, #error)

#endif // __CAXException_h__
