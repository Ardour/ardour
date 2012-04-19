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
	CAComponent.cpp
 
=============================================================================*/

#include "CAComponent.h"
#include "CAComponentDescription.h"
#include "CACFDictionary.h"
#include <stdlib.h>

CAComponent::CAComponent (const ComponentDescription& inDesc, CAComponent* next)
	: mManuName(0), mAUName(0), mCompName(0), mCompInfo (0)
{
	mComp = FindNextComponent ((next ? next->Comp() : NULL), const_cast<ComponentDescription*>(&inDesc));
	if (mComp)
		GetComponentInfo (Comp(), &mDesc, NULL, NULL, NULL);
	else
		memcpy (&mDesc, &inDesc, sizeof(ComponentDescription));
}

CAComponent::CAComponent (const Component& comp) 
	: mComp (comp),
	  mManuName(0), 
	  mAUName(0), 
	  mCompName(0), 
	  mCompInfo (0) 
{
	GetComponentInfo (Comp(), &mDesc, NULL, NULL, NULL);
}

CAComponent::CAComponent (const ComponentInstance& compInst) 
	: mComp (Component(compInst)), 
	  mManuName(0), 
	  mAUName(0), 
	  mCompName(0), 
	  mCompInfo (0) 
{ 
	GetComponentInfo (Comp(), &mDesc, NULL, NULL, NULL);
}

CAComponent::CAComponent (OSType inType, OSType inSubtype, OSType inManu)
	: mDesc (inType, inSubtype, inManu),
	  mManuName(0), mAUName(0), mCompName(0), mCompInfo (0)
{
	mComp = FindNextComponent (NULL, &mDesc);
	GetComponentInfo (Comp(), &mDesc, NULL, NULL, NULL);
}

CAComponent::~CAComponent ()
{
	Clear();
}

OSStatus		CAComponent::GetResourceVersion (UInt32 &outVersion) const
{
	bool versionFound = false;
	ResFileRefNum componentResFileID = kResFileNotOpened;
	OSStatus result;
	short thngResourceCount;
	
	short curRes = CurResFile();
	require_noerr (result = OpenAComponentResFile( mComp, &componentResFileID), home);
	require_noerr (result = componentResFileID <= 0, home);
	
	UseResFile(componentResFileID);

	thngResourceCount = Count1Resources(kComponentResourceType);
	
	require_noerr (result = ResError(), home);
			// only go on if we successfully found at least 1 thng resource
	require_noerr (thngResourceCount <= 0 ? -1 : 0, home);

	// loop through all of the Component thng resources trying to 
	// find one that matches this Component description
	for (short i = 0; i < thngResourceCount && (!versionFound); i++)
	{
		// try to get a handle to this code resource
		Handle thngResourceHandle = Get1IndResource(kComponentResourceType, i+1);
		if (thngResourceHandle != NULL && ((*thngResourceHandle) != NULL))
		{
			if (UInt32(GetHandleSize(thngResourceHandle)) >= sizeof(ExtComponentResource))
			{
				ExtComponentResource * componentThng = (ExtComponentResource*) (*thngResourceHandle);

				// check to see if this is the thng resource for the particular Component that we are looking at
				// (there often is more than one Component described in the resource)
				if ((componentThng->cd.componentType == mDesc.Type()) 
						&& (componentThng->cd.componentSubType == mDesc.SubType()) 
						&& (componentThng->cd.componentManufacturer == mDesc.Manu()))
				{
					outVersion = componentThng->componentVersion;
					versionFound = true;
				}
			}
			ReleaseResource(thngResourceHandle);
		}
	}

	if (!versionFound)
		result = resNotFound;
		
	UseResFile(curRes);	// revert
	
	if ( componentResFileID != kResFileNotOpened )
		CloseComponentResFile(componentResFileID);
		
home:
	return result;
}

void			CAComponent::Clear ()
{
	if (mManuName) { CFRelease (mManuName); mManuName = 0; }
	if (mAUName) { CFRelease (mAUName);  mAUName = 0; }
	if (mCompName) { CFRelease (mCompName); mCompName = 0; }
	if (mCompInfo) { CFRelease (mCompInfo); mCompInfo = 0; }
}

CAComponent&	CAComponent::operator= (const CAComponent& y)
{
	Clear();

	mComp = y.mComp;
	mDesc = y.mDesc;

	if (y.mManuName) { mManuName = y.mManuName; CFRetain (mManuName); }
	if (y.mAUName) { mAUName = y.mAUName; CFRetain (mAUName); }
	if (y.mCompName) { mCompName = y.mCompName; CFRetain (mCompName); } 
	if (y.mCompInfo) { mCompInfo = y.mCompInfo; CFRetain (mCompInfo); }

	return *this;
}

void 		CAComponent::SetCompNames () const
{
	if (!mCompName) {
		Handle h1 = NewHandle(4);
		CAComponentDescription desc;
		OSStatus err = GetComponentInfo (Comp(), &desc, h1, 0, 0);
		
		if (err) { DisposeHandle(h1); return; }
		
		HLock(h1);
		char* ptr1 = *h1;
		// Get the manufacturer's name... look for the ':' character convention
		int len = *ptr1++;
		char* displayStr = 0;

		const_cast<CAComponent*>(this)->mCompName = CFStringCreateWithPascalString(NULL, (const unsigned char*)*h1, kCFStringEncodingMacRoman);
				
		for (int i = 0; i < len; ++i) {
			if (ptr1[i] == ':') { // found the name
				ptr1[i] = 0;
				displayStr = ptr1;
				break;
			}
		}
		
		if (displayStr)
		{
			const_cast<CAComponent*>(this)->mManuName = CFStringCreateWithCString(NULL, displayStr, kCFStringEncodingMacRoman);
										
			//move displayStr ptr past the manu, to the name
			// we move the characters down a index, because the handle doesn't have any room
			// at the end for the \0
			int i = strlen(displayStr), j = 0;
			while (displayStr[++i] == ' ' && i < len)
					;
			while (i < len)
				displayStr[j++] = displayStr[i++];
			displayStr[j] = 0;

			const_cast<CAComponent*>(this)->mAUName = CFStringCreateWithCString(NULL, displayStr, kCFStringEncodingMacRoman);
		} 
		
		DisposeHandle (h1);
	}
}

void	CAComponent::SetCompInfo () const
{
	if (!mCompInfo) {
		Handle h1 = NewHandle(4);
		CAComponentDescription desc;
		OSStatus err = GetComponentInfo (Comp(), &desc, 0, h1, 0);
		if (err) return;
		HLock (h1);
		const_cast<CAComponent*>(this)->mCompInfo = CFStringCreateWithPascalString(NULL, (const unsigned char*)*h1, kCFStringEncodingMacRoman);

		DisposeHandle (h1);
	}
}

void	_ShowCF (FILE* file, CFStringRef str)
{
	if (CFGetTypeID(str) != CFStringGetTypeID()) {
		CFShow(str);
		return;
	}

	UInt32 len = CFStringGetLength(str);
	char* chars = (char*)malloc (len * 2); // give us plenty of room for unichar chars
	if (CFStringGetCString (str, chars, len * 2, kCFStringEncodingUTF8))
		fprintf (file, "%s", chars);
	else
		CFShow (str);

	free (chars);
}

void	CAComponent::Print(FILE* file) const
{
        fprintf (file, "CAComponent: %p", (void*) (Comp()));
	if (mManuName) {
		fprintf (file, ", Manu:"); _ShowCF (file, mManuName);
		if (mAUName) fprintf (file, ", Name:"); _ShowCF (file, mAUName);
	}
	fprintf (file, ", "); 
	Desc ().Print(file);
}
