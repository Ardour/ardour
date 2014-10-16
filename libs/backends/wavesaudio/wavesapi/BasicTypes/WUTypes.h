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

#ifndef __WUTypes_h__
	#define __WUTypes_h__

/* Copy to include:
#include "BasicTypes/WUTypes.h"
*/

#include "WavesPublicAPI/WTErr.h"
#include "WavesPublicAPI/wstdint.h"
#include "BasicTypes/WUDefines.h"
#include "BasicTypes/WCFourCC.h"	// declares WTFourCharCode & WCFourCC
#include "BasicTypes/WUComPtr.h"	// Communication Ptr for x64 compatibility
#include "WCFixedString.h"
#include <ctime>
#include <vector>
/********************************************************************************
    Atoms
*********************************************************************************/

#define WTSInt64    "WTSInt64 is obsolete, please use int64_t instead"; 
#define WTUInt64    "WTUInt64 is obsolete, please use uint64_t instead"; 
#define WTSInt32    "WTSInt32 is obsolete, please use int32_t instead"; 
#define WTUInt32    "WTUInt32 is obsolete, please use uint32_t instead"; 
#define WTSInt16    "WTSInt16 is obsolete, please use int16_t instead"; 
#define WTUInt16    "WTUInt16 is obsolete, please use uint16_t instead"; 
#define WTSInt8     "WTSInt8 is obsolete, please use int8_t instead"; 
#define WTUInt8     "WTUInt8 is obsolete, please use uint8_t instead"; 
#define WTFloat32   "WTFloat32 is obsolete, please use float instead"; 
#define WTByte      "WTByte is obsolete, please use uint8_t instead"; 

/********************************************************************************
    Consts
*********************************************************************************/
//#define PI 3.1415926535897 // ... Was moved to WUMathConsts.h under the name kPI
const uint32_t kDefaultCircleSlices = 100;


/********************************************************************************
    Utilities
*********************************************************************************/

// SCOPED_ENUM is a macro that defines an enum inside a class with a given name, thus declaring the enum values
// inside a named scope. This allows declaring:
//      SCOPED_ENUM(SomeType)
//      {
//          Val1,
//          Val2,
//          Val3
//      }
//      SCOPED_ENUM_END
// And then you can reference SomeType::Val1, SomeType::Val2, SomeType::Val3 for the various values, unlike
// a regular enum on which Val1, Val2 and Val3 would become global names.
// Additionally, you get SomeType::Type to specify the type of the whole enum in case you want to transfer it to
// a function.
// Don't forget to close the enum with SCOPED_ENUM_END, otherwise you'll get bogus compilation errors.
// This requirement can probably be removed some day, but it will make the SCOPED_ENUM macro much less readable...
#define SCOPED_ENUM(name) \
class name \
{ \
public: enum Type

#define SCOPED_ENUM_END ;};


//********************************************************************************
//    Files

//! file (and resource container) opening permissions			
// Note: When opening with eFMWriteOnly on existing file, writing to the file will append, not overwrite, Shai, 9/8/2007.
enum 	WEPermitions{ eFMReadOnly, eFMWriteOnly, eFMReadWrite};

// File cursor positions
enum	WEPositionMode{eFMFileBegin, eFMFileCurrent, eFMFileEnd};

// File creation types
enum 	WECreateFlags {
	eFMCreateFile_DontOverrideIfAlreadyExists,	// Create a new file , If the file exists leaves the existing data intact
	eFMCreateFile_FailIfAlreadyExists,			// Attempt to create a new file, if file already exists - fail.
	eFMCreateFile_OverrideIfAlreadyExists	    // Create a new file , If the file exists, overwrite the file and clear the existing data
};


enum WEFoldersDomain{
	eSystemDomain,
	eLocalDomain,
	eUserDomain,

	eNumberOfFoldersDomains
};
enum WEArchBits{
    e32Bits,
    e64Bits,
    eNumberOfArchBits
};

enum WESystemFolders{
	eSystemFolder,
	eDesktopFolder,
	ePreferencesFolder,
	eWavesPreferencesFolder, //deprecated use eWavesPreferencesFolder2
	eTemporaryFolder,
	eTrashFolder,
	eCurrentFolder,
	eRootFolder,
	eLibrariesFolder,
	eAudioComponentsFolder, // MacOS only 
	eCacheFolder,
	eWavesCacheFolder,
	eAppDataFolder,
	eWavesAppDataFolder,
	eSharedUserDataFolder,
	eWavesSharedUserDataFolder,
	eWavesScanViewFolder,

	eWavesPreferencesFolder2, // Mac: "/Users/username/Library/Preferences/Waves Audio"
                              // Win: "C:\Users\username\AppData\Roaming\Waves Audio\Preferences"
		
	eNumberOfSystemFolders
};

//********************************************************************************
//    Process

#ifdef __APPLE__
	typedef uint32_t WTProcessID; // actually pid_t which is __darwin_pid_t which is __uint32_t
#endif
#ifdef PLATFORM_WINDOWS
	typedef int		WTProcessID;
#endif
#ifdef __linux__
	typedef uint32_t WTProcessID;
#endif

enum WEManagerInitOptions
{
    eUnknown_ManagerInitOption,
    eMacOS_Carbon_Runtime,
    eMacOS_Cocoa_Runtime,
    eLinuxOS_gtk_Runtime,
    eLinuxOS_X_Runtime,
    eWindowsOS_GoodOld_Runtime,         // good old windows API
    eWindowsOS_DotNET_Runtime,
    eVerticalFliped_Graphics,
    eInit_RM,
    eInit_GMConfig,
    eInit_PVM,
    eInit_UM,
    eInit_BKG
};
#ifdef __APPLE__
    #if __LP64__ || NS_BUILD_32_LIKE_64	// in 64bit (or when NS_BUILD_32_LIKE_64 is specified) we decline Carbon implementation.
        const WEManagerInitOptions eDefaultRuntime = eMacOS_Cocoa_Runtime;
    #else
        const WEManagerInitOptions eDefaultRuntime = eMacOS_Carbon_Runtime;
    #endif
#endif
#ifdef PLATFORM_WINDOWS
    const WEManagerInitOptions eDefaultRuntime = eWindowsOS_GoodOld_Runtime;
#endif
#ifdef __linux__
    const WEManagerInitOptions eDefaultRuntime = eLinuxOS_gtk_Runtime;
#endif


//********************************************************************************
//    Files

const uint32_t kMaxPathLength = 1023;      // maximum length of a path 
const uint32_t kMaxFileNameLength = 255;    // maximum length of a file name including extension
typedef WCFixedString<kMaxPathLength> WTPathString;
typedef WCFixedString<kMaxFileNameLength> WTFileNameString;

typedef uint64_t WTFileSize;
const WTFileSize kIllegalFileSize = (WTFileSize)-1;

typedef off_t WTFileOffset;

typedef std::time_t WTFileTime;
const WTFileTime kIllegalFileTime = (WTFileTime)-1;

typedef struct WTPathType* WTPathRef;				// represents a path, path need not exists
typedef struct WTOpenFileType* WTOpenFileRef;		// represents a real, open file
typedef struct WTNativeDLLRefType* WTNativeDLLRef;	// define WTNativeDLLRef as a unique type CFBundleRef on Mac, HINSTANCE on Windows
const WTNativeDLLRef kIllegalNativeDLLRef = 0;
//********************************************************************************
//    Resources

const size_t kMaxResTypeLength = 31;
typedef WCFixedString31 WTResType;
typedef short	    WTResID;
const   WTResID     kIllegalResID = -1;


typedef struct WTResContainerType*			WTResContainerRef;
typedef struct WTResourceType*					WTResRef;
const WTResContainerRef kIllegalContainerRef = 0;
const WTResRef kIllegalResourceRef = 0;

#ifdef __APPLE__
	typedef struct WTNativeResourceType*	WTNativeResourceRef;	// for use when need to have access to the native resource without going though resource manager caching anf conversion.
    const WTNativeResourceRef		kIllegalNativeResourceRef = 0;
#endif
#ifdef PLATFORM_WINDOWS
	typedef struct WTNativeResourceType*	WTNativeResourceRef; //HGLOBAL  // for use when need to have access to the native resource without going though resource manager caching anf conversion.
    const WTNativeResourceRef		kIllegalNativeResourceRef = 0;
#endif
#ifdef __linux__
typedef void* 					WTNativeResourceRef;   // WTOpenFileRef // for use when need to have access to the native resource without going though resource manager caching anf conversion.
    const WTNativeResourceRef		kIllegalNativeResourceRef = 0;
#endif

//********************************************************************************
//    OpenGL

typedef struct WCOGLContext*			WCOGLContextRef;
typedef struct WCOGLTexture*			WCOGLTextureRef;
typedef struct WSPluginView*            WCPluginViewRef;
typedef struct WSMenu*                  WCMenuRef;
typedef struct WCPluginNativeView*      WCPluginNativeViewRef;

const WCOGLContextRef kIllegalOGLContextRef = 0;
const WCOGLTextureRef kIllegalOGLTextureRef = 0;
const WCPluginViewRef kIllegalPluginViewRef = 0;
const WCMenuRef kIllegalWCMenuRef = 0;

const intptr_t kIllegalTexturesMaster = -1;        


typedef unsigned int WTTextureRef;
const WTTextureRef kIllegalTextureRef = 0;

// type for storing pointer to functions. Used to avoid warning such as "C++ forbids conversion between pointer to function and pointer to object"
typedef void (*DUMMY_FUNC_PTR)(void);

// type for a generic callback function with one parameter
typedef intptr_t (*CALLBACK_1_PARAM_FUNC_PTR)(intptr_t);

//////////////////////////////////////////////////////////////
// Timer
typedef intptr_t WTTimerRef;
const WTTimerRef kIllegalTimerRef = 0;
typedef void (*WTTimerCallback)(intptr_t);

// generic type for OS native pointer
typedef void* WTPtr;

#endif //__WUTypes_h__
