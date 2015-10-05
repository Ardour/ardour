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
	CACFDictionary.cpp
	CAAudioEngine

=============================================================================*/

//=============================================================================
//	Includes
//=============================================================================

//	Self Include
#include "CACFDictionary.h"

//	PublicUtility Includes
#include "CACFString.h"
#include "CACFNumber.h"

//=============================================================================
//	CACFDictionary
//=============================================================================

bool	CACFDictionary::HasKey(const CFStringRef inKey) const
{
	return CFDictionaryContainsKey(mCFDictionary, inKey) != 0;
}

UInt32	CACFDictionary::Size () const
{
	return CFDictionaryGetCount(mCFDictionary);
}

void	CACFDictionary::GetKeys (const void **keys) const
{
	CFDictionaryGetKeysAndValues(mCFDictionary, keys, NULL);
}

bool	CACFDictionary::GetBool(const CFStringRef inKey, bool& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFBooleanGetTypeID()))
		{
			outValue = CFBooleanGetValue(static_cast<CFBooleanRef>(theValue));
			theAnswer = true;
		}
		else if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			SInt32 theNumericValue = 0;
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberSInt32Type, &theNumericValue);
			outValue = theNumericValue != 0;
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetSInt32(const CFStringRef inKey, SInt32& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberSInt32Type, &outValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetUInt32(const CFStringRef inKey, UInt32& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberSInt32Type, &outValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetSInt64(const CFStringRef inKey, SInt64& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberSInt64Type, &outValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetUInt64(const CFStringRef inKey, UInt64& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberSInt64Type, &outValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetFloat32(const CFStringRef inKey, Float32& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberFloat32Type, &outValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetFloat64(const CFStringRef inKey, Float64& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFNumberGetTypeID()))
		{
			CFNumberGetValue(static_cast<CFNumberRef>(theValue), kCFNumberFloat64Type, &outValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetString(const CFStringRef inKey, CFStringRef& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFStringGetTypeID()))
		{
			outValue = static_cast<CFStringRef>(theValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetArray(const CFStringRef inKey, CFArrayRef& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFArrayGetTypeID()))
		{
			outValue = static_cast<CFArrayRef>(theValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetDictionary(const CFStringRef inKey, CFDictionaryRef& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFDictionaryGetTypeID()))
		{
			outValue = static_cast<CFDictionaryRef>(theValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetData(const CFStringRef inKey, CFDataRef& outValue) const
{
	bool theAnswer = false;

	CFTypeRef theValue = NULL;
	if(GetCFType(inKey, theValue))
	{
		if((theValue != NULL) && (CFGetTypeID(theValue) == CFDataGetTypeID()))
		{
			outValue = static_cast<CFDataRef>(theValue);
			theAnswer = true;
		}
	}

	return theAnswer;
}

bool	CACFDictionary::GetCFType(const CFStringRef inKey, CFTypeRef& outValue) const
{
	bool theAnswer = false;

	if(mCFDictionary != NULL)
	{
		outValue = CFDictionaryGetValue(mCFDictionary, inKey);
		theAnswer = (outValue != NULL);
	}

	return theAnswer;
}

bool	CACFDictionary::GetCFTypeWithCStringKey(const char* inKey, CFTypeRef& outValue) const
{
	bool theAnswer = false;

	if(mCFDictionary != NULL)
	{
		CACFString theKey(inKey);
		if(theKey.IsValid())
		{
			theAnswer = GetCFType(theKey.GetCFString(), outValue);
		}
	}

	return theAnswer;
}

bool	CACFDictionary::AddSInt32(const CFStringRef inKey, SInt32 inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFNumber theValue(inValue);
		theAnswer = AddCFType(inKey, theValue.GetCFNumber());
	}

	return theAnswer;
}

bool	CACFDictionary::AddUInt32(const CFStringRef inKey, UInt32 inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFNumber theValue(inValue);
		theAnswer = AddCFType(inKey, theValue.GetCFNumber());
	}

	return theAnswer;
}

bool	CACFDictionary::AddSInt64(const CFStringRef inKey, SInt64 inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFNumber theValue(inValue);
		theAnswer = AddCFType(inKey, theValue.GetCFNumber());
	}

	return theAnswer;
}

bool	CACFDictionary::AddUInt64(const CFStringRef inKey, UInt64 inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFNumber theValue(inValue);
		theAnswer = AddCFType(inKey, theValue.GetCFNumber());
	}

	return theAnswer;
}

bool	CACFDictionary::AddFloat32(const CFStringRef inKey, Float32 inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFNumber theValue(inValue);
		theAnswer = AddCFType(inKey, theValue.GetCFNumber());
	}

	return theAnswer;
}

bool	CACFDictionary::AddFloat64(const CFStringRef inKey, Float64 inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFNumber theValue(inValue);
		theAnswer = AddCFType(inKey, theValue.GetCFNumber());
	}

	return theAnswer;
}

bool	CACFDictionary::AddNumber(const CFStringRef inKey, const CFNumberRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		theAnswer = AddCFType(inKey, inValue);
	}

	return theAnswer;
}

bool	CACFDictionary::AddString(const CFStringRef inKey, const CFStringRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		theAnswer = AddCFType(inKey, inValue);
	}

	return theAnswer;
}

bool	CACFDictionary::AddArray(const CFStringRef inKey, const CFArrayRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		theAnswer = AddCFType(inKey, inValue);
	}

	return theAnswer;
}

bool	CACFDictionary::AddDictionary(const CFStringRef inKey, const CFDictionaryRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		theAnswer = AddCFType(inKey, inValue);
	}

	return theAnswer;
}

bool	CACFDictionary::AddData(const CFStringRef inKey, const CFDataRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		theAnswer = AddCFType(inKey, inValue);
	}

	return theAnswer;
}

bool	CACFDictionary::AddCFType(const CFStringRef inKey, const CFTypeRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CFDictionarySetValue(mCFDictionary, inKey, inValue);
		theAnswer = true;
	}

	return theAnswer;
}

bool	CACFDictionary::AddCFTypeWithCStringKey(const char* inKey, const CFTypeRef inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFString theKey(inKey);
		if(theKey.IsValid())
		{
			theAnswer = AddCFType(theKey.GetCFString(), inValue);
		}
	}

	return theAnswer;
}

bool	CACFDictionary::AddCString(const CFStringRef inKey, const char* inValue)
{
	bool theAnswer = false;

	if(mMutable && (mCFDictionary != NULL))
	{
		CACFString theValue(inValue);
		if(theValue.IsValid())
		{
			theAnswer = AddCFType(inKey, theValue.GetCFString());
		}
	}

	return theAnswer;
}
