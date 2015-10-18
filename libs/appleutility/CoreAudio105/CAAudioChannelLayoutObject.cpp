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
    CAAudioChannelLayoutObject.cpp

=============================================================================*/

#include "CAAudioChannelLayout.h"
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreServices/CoreServices.h>
	#include <AudioToolbox/AudioFormat.h>
#else
	#include <CoreServices.h>
	#include <AudioFormat.h>
#endif


CAAudioChannelLayout::CAAudioChannelLayout ()
{
	mLayoutHolder = new ACLRefCounter (offsetof(AudioChannelLayout, mChannelDescriptions));
}

//=============================================================================
//	CAAudioChannelLayout::CAAudioChannelLayout
//=============================================================================
CAAudioChannelLayout::CAAudioChannelLayout (UInt32 inNumberChannels, bool inChooseSurround)
{
		// this chooses default layouts based on the number of channels...
	UInt32 theSize = CalculateByteSize (inNumberChannels);

	mLayoutHolder = new ACLRefCounter (theSize);

	AudioChannelLayout* layout = mLayoutHolder->GetLayout();

	layout->mNumberChannelDescriptions = inNumberChannels;

	switch (inNumberChannels)
	{
		case 1:
			layout->mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
			break;
		case 2:
			layout->mChannelLayoutTag = inChooseSurround ? kAudioChannelLayoutTag_Binaural : kAudioChannelLayoutTag_Stereo;
			break;
		case 4:
			layout->mChannelLayoutTag = inChooseSurround ? kAudioChannelLayoutTag_Ambisonic_B_Format : kAudioChannelLayoutTag_AudioUnit_4;
			break;
		case 5:
			layout->mChannelLayoutTag = inChooseSurround ? kAudioChannelLayoutTag_AudioUnit_5_0 : kAudioChannelLayoutTag_AudioUnit_5;
			break;
		case 6:
			layout->mChannelLayoutTag = inChooseSurround ? kAudioChannelLayoutTag_AudioUnit_6_0 : kAudioChannelLayoutTag_AudioUnit_6;
			break;
		case 7:
			layout->mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_7_0;
			break;
		case 8:
			layout->mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_8;
			break;
		default:
			// here we have a "broken" layout, in the sense that we haven't any idea how to lay this out
			// the layout itself is all set to zeros
			// ### no longer true ###
			SetAllToUnknown(*layout, inNumberChannels);
			break;
	}
}

//=============================================================================
//	CAAudioChannelLayout::CAAudioChannelLayout
//=============================================================================
CAAudioChannelLayout::CAAudioChannelLayout (AudioChannelLayoutTag inLayoutTag)
	: mLayoutHolder(NULL)
{
	SetWithTag(inLayoutTag);
}

//=============================================================================
//	CAAudioChannelLayout::CAAudioChannelLayout
//=============================================================================
CAAudioChannelLayout::CAAudioChannelLayout (const CAAudioChannelLayout &c)
	: mLayoutHolder(NULL)
{
	*this = c;
}


//=============================================================================
//	CAAudioChannelLayout::AudioChannelLayout
//=============================================================================
CAAudioChannelLayout::CAAudioChannelLayout (const AudioChannelLayout* inChannelLayout)
	: mLayoutHolder(NULL)
{
	*this = inChannelLayout;
}

//=============================================================================
//	CAAudioChannelLayout::~CAAudioChannelLayout
//=============================================================================
CAAudioChannelLayout::~CAAudioChannelLayout ()
{
	if (mLayoutHolder) {
		mLayoutHolder->release();
		mLayoutHolder = NULL;
	}
}

//=============================================================================
//	CAAudioChannelLayout::CAAudioChannelLayout
//=============================================================================
CAAudioChannelLayout& CAAudioChannelLayout::operator= (const CAAudioChannelLayout &c)
{
	if (mLayoutHolder != c.mLayoutHolder) {
		if (mLayoutHolder)
			mLayoutHolder->release();

		if ((mLayoutHolder = c.mLayoutHolder) != NULL)
			mLayoutHolder->retain();
	}

	return *this;
}

CAAudioChannelLayout&	CAAudioChannelLayout::operator= (const AudioChannelLayout* inChannelLayout)
{
	if (mLayoutHolder)
		mLayoutHolder->release();

	UInt32 theSize = CalculateByteSize (inChannelLayout->mNumberChannelDescriptions);

	mLayoutHolder = new ACLRefCounter (theSize);

	memcpy(mLayoutHolder->mLayout, inChannelLayout, theSize);
	return *this;
}

void	CAAudioChannelLayout::SetWithTag(AudioChannelLayoutTag inTag)
{
	if (mLayoutHolder)
		mLayoutHolder->release();

	mLayoutHolder = new ACLRefCounter(offsetof(AudioChannelLayout, mChannelDescriptions[0]));
	AudioChannelLayout* layout = mLayoutHolder->GetLayout();
	layout->mChannelLayoutTag = inTag;
}

//=============================================================================
//	CAAudioChannelLayout::operator==
//=============================================================================
bool		CAAudioChannelLayout::operator== (const CAAudioChannelLayout &c) const
{
	if (mLayoutHolder == c.mLayoutHolder)
		return true;
	return Layout() == c.Layout();
}

//=============================================================================
//	CAAudioChannelLayout::Print
//=============================================================================
void		CAAudioChannelLayout::Print (FILE* file) const
{
	CAShowAudioChannelLayout (file, &Layout());
}

