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
	CABufferList.cpp

=============================================================================*/

#include "CABufferList.h"
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreServices/CoreServices.h>
#else
	#include <Endian.h>
#endif

void		CABufferList::AllocateBuffers(UInt32 nBytes)
{
	if (nBytes <= GetNumBytes()) return;

	if (mNumberBuffers > 1)
		// align successive buffers for Altivec and to take alternating
		// cache line hits by spacing them by odd multiples of 16
		nBytes = (nBytes + (0x10 - (nBytes & 0xF))) | 0x10;
	UInt32 memorySize = nBytes * mNumberBuffers;
	Byte *newMemory = new Byte[memorySize], *p = newMemory;
	memset(newMemory, 0, memorySize);	// get page faults now, not later

	AudioBuffer *buf = mBuffers;
	for (UInt32 i = mNumberBuffers; i--; ++buf) {
		if (buf->mData != NULL && buf->mDataByteSize > 0)
			// preserve existing buffer contents
			memcpy(p, buf->mData, buf->mDataByteSize);
		buf->mDataByteSize = nBytes;
		buf->mData = p;
		p += nBytes;
	}
	Byte *oldMemory = mBufferMemory;
	mBufferMemory = newMemory;
	delete[] oldMemory;
}

void		CABufferList::AllocateBuffersAndCopyFrom(UInt32 nBytes, CABufferList *inSrcList, CABufferList *inSetPtrList)
{
	if (mNumberBuffers != inSrcList->mNumberBuffers) return;
	if (mNumberBuffers != inSetPtrList->mNumberBuffers) return;
	if (nBytes <= GetNumBytes()) {
		CopyAllFrom(inSrcList, inSetPtrList);
		return;
	}
	inSetPtrList->VerifyNotTrashingOwnedBuffer();
	UInt32 fromByteSize = inSrcList->GetNumBytes();

	if (mNumberBuffers > 1)
		// align successive buffers for Altivec and to take alternating
		// cache line hits by spacing them by odd multiples of 16
		nBytes = (nBytes + (0x10 - (nBytes & 0xF))) | 0x10;
	UInt32 memorySize = nBytes * mNumberBuffers;
	Byte *newMemory = new Byte[memorySize], *p = newMemory;
	memset(newMemory, 0, memorySize);	// make buffer "hot"

	AudioBuffer *buf = mBuffers;
	AudioBuffer *ptrBuf = inSetPtrList->mBuffers;
	AudioBuffer *srcBuf = inSrcList->mBuffers;
	for (UInt32 i = mNumberBuffers; i--; ++buf, ++ptrBuf, ++srcBuf) {
		if (srcBuf->mData != NULL && srcBuf->mDataByteSize > 0)
			// preserve existing buffer contents
			memmove(p, srcBuf->mData, srcBuf->mDataByteSize);
		buf->mDataByteSize = nBytes;
		buf->mData = p;
		ptrBuf->mDataByteSize = srcBuf->mDataByteSize;
		ptrBuf->mData = p;
		p += nBytes;
	}
	Byte *oldMemory = mBufferMemory;
	mBufferMemory = newMemory;
	if (inSrcList != inSetPtrList)
		inSrcList->BytesConsumed(fromByteSize);
	delete[] oldMemory;
}

void		CABufferList::DeallocateBuffers()
{
	AudioBuffer *buf = mBuffers;
	for (UInt32 i = mNumberBuffers; i--; ++buf) {
		buf->mData = NULL;
		buf->mDataByteSize = 0;
	}
	if (mBufferMemory != NULL) {
		delete[] mBufferMemory;
		mBufferMemory = NULL;
	}

}

extern "C" void CAShowAudioBufferList(const AudioBufferList *abl, int framesToPrint, int wordSize)
{
	printf("AudioBufferList @ %p:\n", abl);
	const AudioBuffer *buf = abl->mBuffers;
	for (UInt32 i = 0; i < abl->mNumberBuffers; ++i, ++buf) {
		printf("  [%2ld]: %2ldch, %5ld bytes @ %8p",
			i, buf->mNumberChannels, buf->mDataByteSize, buf->mData);
		if (framesToPrint) {
			printf(":");
			Byte *p = (Byte *)buf->mData;
			for (int j = framesToPrint * buf->mNumberChannels; --j >= 0; )
				switch (wordSize) {
				case 0:
					printf(" %6.3f", *(Float32 *)p);
					p += sizeof(Float32);
					break;
				case 1:
				case -1:
					printf(" %02X", *p);
					p += 1;
					break;
				case 2:
					printf(" %04X", EndianU16_BtoN(*(UInt16 *)p));
					p += 2;
					break;
				case 3:
					printf(" %06X", (p[0] << 16) | (p[1] << 8) | p[2]);
					p += 3;
					break;
				case 4:
					printf(" %08lX", EndianU32_BtoN(*(UInt32 *)p));
					p += 4;
					break;
				case -2:
					printf(" %04X", EndianU16_LtoN(*(UInt16 *)p));
					p += 2;
					break;
				case -3:
					printf(" %06X", (p[2] << 16) | (p[1] << 8) | p[0]);
					p += 3;
					break;
				case -4:
					printf(" %08lX", EndianU32_LtoN(*(UInt32 *)p));
					p += 4;
					break;
				}
		}
		printf("\n");
	}
}

