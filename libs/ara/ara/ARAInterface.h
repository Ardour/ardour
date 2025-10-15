//------------------------------------------------------------------------------
//! \file       ARAInterface.h
//!             definition of the ARA application programming interface
//! \project    ARA API Specification
//! \copyright  Copyright (c) 2012-2025, Celemony Software GmbH, All Rights Reserved.
//!             Developed in cooperation with PreSonus Software Ltd.
//! \license    Licensed under the Apache License, Version 2.0 (the "License");
//!             you may not use this file except in compliance with the License.
//!             You may obtain a copy of the License at
//!
//!               http://www.apache.org/licenses/LICENSE-2.0
//!
//!             Unless required by applicable law or agreed to in writing, software
//!             distributed under the License is distributed on an "AS IS" BASIS,
//!             WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//!             See the License for the specific language governing permissions and
//!             limitations under the License.
//------------------------------------------------------------------------------



/***************************************************************************************************/
// IMPORTANT:
// Please read ARA_API.pdf for general documentation before studying this header!
/***************************************************************************************************/



#ifndef ARAInterface_h
#define ARAInterface_h


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark General configuration
#endif
/***************************************************************************************************/


/***************************************************************************************************/
// C99 standard includes for the basic data types

#include <stddef.h>
#include <stdint.h>


/***************************************************************************************************/
// Auxiliary defines for Doxygen code generation, must evaluate to 0 for actual code compilation

// Enable this when building Doxygen documentation
#if !defined(ARA_DOXYGEN_BUILD)
    #define ARA_DOXYGEN_BUILD 0
#endif


/***************************************************************************************************/
// Various configurations/decorations to ensure binary compatibility across compilers:
// struct packing and alignment, calling conventions, etc.

#if defined(__cplusplus) && !(ARA_DOXYGEN_BUILD)
namespace ARA
{
extern "C"
{
    //! helper define to properly insert ARA namespace into C compatible headers
    #define ARA_NAMESPACE ARA::
#else
    #define ARA_NAMESPACE
#endif


// define CPU architecture
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define ARA_CPU_X86 1
    #define ARA_CPU_ARM 0
#elif defined(__aarch64__) || defined(__arm64__) || defined(__arm64e__) || defined(_M_ARM64)
    #define ARA_CPU_X86 0
    #define ARA_CPU_ARM 1
//#elif defined(__ppc__) || defined(__ppc64__)
//  #define ARA_CPU_PPC 1
#else
    #error "unsupported CPU architecture"
#endif


// To prevent any alignment/padding settings from the surrounding code to modify the ARA data layout,
// we need to explicitly define the layout here. Ideally, we would stick with the C standard
// alignment/padding (struct alignment defined by largest member, each member aligned by its size),
// but at the time when ARA 1 was developed there was no way to achieve this in code.
// As a workaround, ARA started using 1-byte packing. However, this causes some members in some of
// the ARA structs to be not naturally aligned on 64 bit systems. But moreover, this also affects
// the possible alignment of all ARA structs in some compilers. Thus developers that directly use
// the C API must carefully align any struct that they pass across the API boundary in order to avoid
// performance penalties. When using the ARA library C++ dispatch code, the SizedStruct<> template
// which is used as central low-level wrapper for any data crossing the API takes care of this issue.
#if defined(_MSC_VER) || defined(__GNUC__)
    #if ARA_CPU_X86
        // This is for historical reason only - current MSVC defaults are 8 on x86 and 16 on x64.
        #pragma pack(push, 1)
    #elif ARA_CPU_ARM
        // MSVC default for ARM64 is 8, this also fits the standard packing by member size for
        // the vast majority of the structs in the ARA API on 64 bit processors.
        #pragma pack(push, 8)
    #else
        #error "struct packing and alignment not yet defined for this architecture"
    #endif
#else
    #error "struct packing and alignment not yet defined for this compiler"
#endif

// Override any custom calling conventions, enforce the C standard calling convention.
#if defined(_MSC_VER)
    #define ARA_CALL __cdecl
#else
    #define ARA_CALL
#endif


/***************************************************************************************************/
//! @addtogroup API_generations API Generations
// Macros to mark API added or deprecated as the API evolves - see ARAAPIGeneration.
//! @{

// Internal macro to trigger deprecation warnings (not fully supported by all compilers).
#if defined(__cplusplus) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        // C++14 standard
        #define ARA_WARN_DEPRECATED(generation) [[deprecated("deprecated as of kARAAPIGeneration_"#generation)]]
    #endif
#endif
#if !defined(ARA_WARN_DEPRECATED) && defined(__GNUC__)
    // Vendor-specific: gcc & clang.
    #define ARA_WARN_DEPRECATED(generation) __attribute__((deprecated))
#endif
#if !defined(ARA_WARN_DEPRECATED) && defined(_MSC_VER)
    // Vendor-specific: Visual Studio.
    #define ARA_WARN_DEPRECATED(generation) __declspec(deprecated("deprecated as of kARAAPIGeneration_"#generation))
#endif


//! Markup for outdated API that should no longer be used in future development, but can still
//! be supported for backwards compatibility with older plug-ins/hosts if desired. \br
//! By defining ARA_ENABLE_DEPRECATION_WARNINGS as non-zero value it is possible to get deprecation
//! warnings in the most common compilers, for inspecting deprecated API usage in a given project.
//! These warnings are disabled by default in order to not interfere with code that supports older APIs.
#define ARA_DEPRECATED(generation)
#define ARA_DISABLE_DOCUMENTATION_DEPRECATED_WARNINGS_BEGIN
#define ARA_DISABLE_DOCUMENTATION_DEPRECATED_WARNINGS_END

#if defined(ARA_ENABLE_DEPRECATION_WARNINGS) && (ARA_ENABLE_DEPRECATION_WARNINGS)
    #if !defined(ARA_WARN_DEPRECATED)
        #warning ARA_ENABLE_DEPRECATION_WARNINGS is not supported for this compiler - no deprecation warnings will be shown!
    #endif
    #undef ARA_DEPRECATED
    #define ARA_DEPRECATED(generation) ARA_WARN_DEPRECATED(generation)
#else
    #if defined (__clang__)
        #undef ARA_DISABLE_DOCUMENTATION_DEPRECATED_WARNINGS_BEGIN
        #define ARA_DISABLE_DOCUMENTATION_DEPRECATED_WARNINGS_BEGIN \
            _Pragma ("GCC diagnostic push") \
            _Pragma ("GCC diagnostic ignored \"-Wdocumentation-deprecated-sync\"")
        #undef ARA_DISABLE_DOCUMENTATION_WARNINGS_END
        #define ARA_DISABLE_DOCUMENTATION_WARNINGS_END \
            _Pragma ("GCC diagnostic pop")
    #endif
#endif

ARA_DISABLE_DOCUMENTATION_DEPRECATED_WARNINGS_BEGIN


//! Markup for struct elements which were added in later revisions of the API and may be omitted
//! from the struct when dealing with older plug-ins/hosts.
#define ARA_ADDENDUM(generation)

//! Markup for draft API that is still under active development and not yet properly versioned -
//! when using those struct elements, host and plug-in must agree on a specific draft header version! \br
//! All uses of this macro will be replaced by ARA_ADDENDUM() upon final release.
//! To quickly find all places in your project that use draft API, it's possible to temporarily
//! redefine ARA_DRAFT to ARA_WARN_DEPRECATED(...).
#define ARA_DRAFT ARA_ADDENDUM(2_X_Draft)

// use this alternate definition to trigger a "deprecation" warning for every location draft API is used
//#undef ARA_DRAFT
//#define ARA_DRAFT ARA_WARN_DEPRECATED(2_X_Draft)

//! @}


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Basic constants and data types
#endif

//! @defgroup Basic_Types Basic Types
//! Pre-defined types to ensure binary compatibility between plug-in and host.
//! These types must be used when crossing the API boundary, but intermediate types can be used internally.
//! For example, you can use your own struct representing color, but when defining color for ARA operations
//! your internal color struct must be converted to ARAColor.
//! @{
/***************************************************************************************************/


/***************************************************************************************************/
//! @defgroup Fixed-size_integers Fixed-size Integers
//! ARA defines platform-independent signed integers with fixed size of 32 or 64 bits
//! and for a pointer-sized signed integer.
//! @{

//! Byte: 8 bits wide unsigned integer.
typedef uint8_t ARAByte;

//! 32 bits wide signed integer.
typedef int32_t ARAInt32;

//! 64 bits wide signed integer.
typedef int64_t ARAInt64;

//! Pointer-wide size value for ARA structs.
typedef size_t ARASize;

//! @}


/***************************************************************************************************/
//! @defgroup Boolean_values Boolean Values
//! Since Microsoft still doesn't fully support C99 and fails to provide <stdbool.h>,
//! we need to roll our own. On the other hand this ensures a fixed size of 32 bits, too.
//! 32 bits were chosen so that ARABool is consistent with the other enum-like data types such as
//! ARAContentType. Since ARABool is only used in temporary structs that are valid only for the
//! duration of a call and likely passed in a register in most cases, there's no point in trying
//! to optimize for size by using 8 bit boolean types.
//! Note that in order to avoid conversion warnings in Visual Studio, you should not directly cast
//! bool to ARABool or vice versa, but instead use a ternary operator or a comparison like this:
//! \code{.c}
//! araBool = (cppBool) ? kARATrue : kARAFalse;
//! cppBool = (araBool != kARAFalse);
//! \endcode
//! Providing conversion operators for ARABool for C++ that handle this automatically is alas no
//! viable option, because ARABool is only a typedef so this would lead to side-effects for all
//! conversions from the integer type that ARABool is defined upon.
//! @{

//! Platform independent 32-bit boolean value.
typedef ARAInt32 ARABool;

#if defined(__cplusplus)
    //! "true" value for ARABool.
    constexpr ARABool kARATrue { 1 };
    //! "false" value for ARABool.
    constexpr ARABool kARAFalse { 0 };
#else
    #define kARATrue  ((ARABool)1)
    #define kARAFalse ((ARABool)0)
#endif

//! @}


/***************************************************************************************************/
//! @defgroup Enums Enums
//! ARA enums can either be used to represent distinct enumerations, or to
//! declare C-compatible constant integer flags that can be or'd together as bit masks.
//! To ensure binary compatibility between plug-in and host, the underlying type
//! of ARA enums is always ARAInt32.
//! @{

//! Define a 32-bit ARA enum type.
//! The actual enum declaration is encapsulated in a macro to allow for adjusting it between
//! C++, C and Doxygen builds.
#if ARA_DOXYGEN_BUILD
    #define ARA_32_BIT_ENUM(name) \
        enum name : ARAInt32
#elif defined(__cplusplus) && ((__cplusplus >= 201103L) || (defined(_MSC_VER) && (_MSC_VER >= 1900)))
    #define ARA_32_BIT_ENUM(name) \
        ARAInt32 name; \
        enum : ARAInt32
#else
    #define ARA_32_BIT_ENUM(name) \
        ARAInt32 name; \
        enum
#endif

//! @}


/***************************************************************************************************/
//! @defgroup Strings Strings
//! User-readable texts are stored as UTF-8 encoded unicode strings.
//! It's not defined if and how the string is normalized - if either side has requirements regarding
//! normalization, it needs to apply these after reading the string from the other side.
//! Unicode rules apply regarding normalization, comparison etc.
//! Both hosts and plug-ins are required to support at least all ISO/IEC 8859-1 based characters
//! (from U+0020 up to U+007E and from U+00A0 up to U+00FF) in their text display rendering.
//! @{

//! A single character.
typedef char ARAUtf8Char;

//! A string, 0-terminated.
typedef const ARAUtf8Char * ARAUtf8String;

//! @}


/***************************************************************************************************/
//! @defgroup Common_time-related_data_types Common Time-Related Data Types
//! Some basic data types used in several contexts.
//! @{

//! A point in time in seconds.
typedef double ARATimePosition;

//! A duration of time in seconds - the start of the duration is part of the interval, the end is not.
typedef double ARATimeDuration;


//! Integer sample index, always related to a particular sample rate defined by the context it is used in.
typedef ARAInt64 ARASamplePosition;

//! Integer sample count, always related to a particular sample rate defined by the context this is used in.
typedef ARAInt64 ARASampleCount;


//! A position in musical time measured in quarter notes.
typedef double ARAQuarterPosition;

//! A duration in musical time measured in quarter notes - the start of the duration is part of the interval, the end is not.
typedef double ARAQuarterDuration;

//! @}


/***************************************************************************************************/
//! @defgroup Sampled_audio_data Sampled Audio Data
//! The audio samples are encoded using these format descriptions.
//! The data alignment and byte order always matches the machine's native layout.
//! @{

//! Specified in Hz.
typedef double ARASampleRate;

//! Count of discrete channels of an audio signal.
//! The spacial positioning of the channels may be provided via ARAChannelArrangementDataType.
typedef ARAInt32 ARAChannelCount;

//! To avoid defining yet another abstraction of spacial layout information for the individual
//! channels of an audio signal, ARA directly uses the respective companion API's model of
//! spacial arrangement. Since different companion APIs are available, this enum specifies which
//! abstraction is used.
ARA_ADDENDUM(2_0_Final) typedef ARA_32_BIT_ENUM(ARAChannelArrangementDataType)
{
    //! Used to indicate the feature is not supported/used (e.g. mono or stereo).
    kARAChannelArrangementUndefined = 0,
    //! For VST3, the channel arrangement is specified as Steinberg::Vst::SpeakerArrangement.
    kARAChannelArrangementVST3SpeakerArrangement = 1,
    //! For Audio Units, the channel arrangement is specified as the Core Audio
    //! struct AudioChannelLayout. Note that according to Apple's documentation,
    //! "the kAudioChannelLayoutTag_UseChannelBitmap field is NOT used within the context
    //! of the AudioUnit." If possible, kAudioChannelLayoutTag_UseChannelDescriptions
    //! should also be avoided to ease parsing the struct.
    kARAChannelArrangementCoreAudioChannelLayout = 2,
    //! For AAX, the channel arrangement is specified as AAX_EStemFormat.
    kARAChannelArrangementAAXStemFormat = 3,
    //! For CLAP surround, the channel arrangement is specified as a channel map,
    //! i.e. an array of uint8_t with ARAAudioSourceProperties.channelCount entries.
    kARAChannelArrangementCLAPChannelMap = 4,
    //! For CLAP ambisonic, the channel arrangement is specified as clap_ambisonic_info.
    kARAChannelArrangementCLAPAmbisonicInfo = 5
};

//! @}

/***************************************************************************************************/
//! @defgroup Color Color
//! ARA color representation.
//! @{

//! R/G/B color, values range from 0.0f to 1.0f.
//! Does not include transparency because it must not depend on the background its drawn upon
//! in order to be equally represented in both the host and plug-in UI - any transparency on
//! either side must be converted depending on internal drawing before/after the ARA calls.
ARA_ADDENDUM(2_0_Draft) typedef struct ARAColor
{
    float r; //!< red
    float g; //!< green
    float b; //!< blue
} ARAColor;

//! @}

/***************************************************************************************************/
//! @defgroup Object_References Object References
//! ARA uses pointer-sized unique identifiers to reference objects at runtime -
//! typical C++-based implementations will use the this-pointer as ID.
//! C-style code could do the same, or instead choose to use array indices as ID. \br
//! Those objects that are archived by the host can be persistently identified
//! by an ::ARAPersistentID that the host assigns as a property of the object.
//! @{

//! @name Markup Types
//! Type-safe representations of the opaque refs/host refs.
//! The markup types allow for overloaded custom conversion functions if using C++,
//! or for re-defining the markup types to actual implementations in C like this:
//! \code{.c}
//! #define ARAAudioSourceRefMarkupType MyAudioFileClass
//! #define ARAMusicalContextRefMarkupType MyGlobalTracksClass
//! \endcode
//!  ... etc ...
//! @{

//! Plug-in reference markup type identifier. \br\br
//! Examples:                  \br
//! ::ARAMusicalContextRef     \br
//! ::ARARegionSequenceRef     \br
//! ::ARAAudioSourceRef        \br
//! ::ARAAudioModificationRef  \br
//! ::ARAPlaybackRegionRef     \br
//! ::ARAContentReaderRef      \br
//! ::ARADocumentControllerRef \br
//! ::ARAPlaybackRendererRef   \br
//! ::ARAEditorRendererRef     \br
//! ::ARAEditorViewRef         \br
#define ARA_REF(RefType) struct RefType##MarkupType * RefType

//! Host reference markup type identifier. \br\br
//! Examples:                           \br
//! ::ARAMusicalContextHostRef          \br
//! ::ARARegionSequenceHostRef          \br
//! ::ARAAudioSourceHostRef             \br
//! ::ARAAudioModificationHostRef       \br
//! ::ARAPlaybackRegionHostRef          \br
//! ::ARAContentReaderHostRef           \br
//! ::ARAAudioAccessControllerHostRef   \br
//! ::ARAAudioReaderHostRef             \br
//! ::ARAArchivingControllerHostRef     \br
//! ::ARAArchiveReaderHostRef           \br
//! ::ARAArchiveWriterHostRef           \br
//! ::ARAContentAccessControllerHostRef \br
//! ::ARAModelUpdateControllerHostRef   \br
//! ::ARAPlaybackControllerHostRef      \br
#define ARA_HOST_REF(HostRefType) struct HostRefType##MarkupType * HostRefType

//! @}

//! @name Persistent IDs
//! @{

//! Persistent object reference representation.
//! Persistent IDs are used to encode object references between plug-in and host when dealing
//! with persistency. Contrary to the user-readable ARAUtf8String, ARAPersistentIDs are seven-bit
//! US-ASCII-encoded strings, such as "com.manufacturerDomain.someIdentifier", and can thus be
//! directly compared using strcmp() and its siblings. They can be copied using strcpy() and must
//! always be compared by value, not by address.
typedef const char * ARAPersistentID;

//! @}

//! @}

//! @}


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Versioning support
#endif

//! @defgroup API_versions API Versions
//! ARA implements two patterns for its ongoing evolution of the API: incremental, fully-backwards
//! compatible additions by appending features to it versioned structs, and major, potentially
//! incompatible updates through its API generations.
//! @{
/***************************************************************************************************/


/***************************************************************************************************/
//! @defgroup API_generations API Generations
//! While purely additive features can be handled through ARA's versioned structs,
//! ARA API generations allow for non-backwards-compatible, fundamental API changes.
//! For hosts that rely on a certain minimum ARA feature set provided by the plug-ins, it also
//! offers a convenient way to filter incompatible plug-ins.
//! Plug-ins on the other hand can use the API generation chosen by the host to optimize their
//! feature set for the given environment, such as disabling potentially costly fallback code
//! required for older hosts when running in a modern host.
//! @{
typedef ARA_32_BIT_ENUM(ARAAPIGeneration)
{
#if !ARA_CPU_ARM
    //! private API between Studio One and Melodyne
    kARAAPIGeneration_1_0_Draft = 1,
    //! supported by Studio One, Cakewalk/SONAR, Samplitude Pro, Mixcraft, Waveform/Tracktion, Melodyne, VocAlign, AutoTune
    kARAAPIGeneration_1_0_Final = 2,
    //! supported by Studio One, Logic Pro, Cubase/Nuendo, Cakewalk, REAPER, Melodyne, ReVoice Pro, VocAlign, Auto-Align, SpectraLayers
    kARAAPIGeneration_2_0_Draft = 3,
#endif
    //! supported by Pro Tools
    //! also required on ARM platforms - all ARM-compatible ARA vendors are now supporting this
    kARAAPIGeneration_2_0_Final = 4,
    //! used during 2.x development
    kARAAPIGeneration_2_X_Draft = 5,
    //! conforming plug-ins will send proper change notifications when their persistent state changes
    //! via ARAModelUpdateControllerInterface, allowing the host to only save what has actually changed.
    kARAAPIGeneration_2_3_Final = 6
};

//! @}


/***************************************************************************************************/
//! @defgroup Versioned_structs Versioned Structs
//! In the various interface and data structs used in the ARA API, callback pointers or data fields
//! may be added in later revisions of the current API generation. Each of these extensible structs
//! starts with a structSize data field that describes how much data is actually contained in the
//! given instance of the struct, thus allowing to determine which of the additional features are
//! supported by the other side.
//! All struct members that are later additions will be marked with the macro ARA_ADDENDUM.
//! Members that are not marked as addendum must always be present in the struct.
//! Accordingly, the minimum value of structSize is the size of the struct in the first API revision.
//! When creating such a struct in your code, the maximum value is the size of the struct in the
//! current API revision used at compile time. When parsing a struct received from the other side,
//! the value may be even larger since the other side may use an even later API revision.
//! \br
//! Note that when implementing ARA, it is important not to directly use sizeof() when filling in
//! the structSize values. If you later update to newer API headers, the values of sizeof() will
//! change and your code thus will be broken until you've implemented all additions.
//! Instead, use the ARA_IMPLEMENTED_STRUCT_SIZE macro or similar techniques added in the ARA
//! C++ library dispatcher code, see \ref ARA_Library_Utility_SizedStructs "there".
//! @{

//! Macro that calculates the proper value for the structSize field of a versioned struct based
//! on which features are actually implemented by the code that provides the struct.
//! This may be different from sizeof() whenever features are added in the API, but not yet
//! implemented in the current code base. Only after adding that implementation, the \p memberName
//! parameter provided to ARA_IMPLEMENTED_STRUCT_SIZE should be updated accordingly.
//! \br
//! The ARA library C++ dispatchers implement a similar feature via templates,
//! see ARA::SizedStruct<>.
#if defined(__cplusplus)
    #define ARA_IMPLEMENTED_STRUCT_SIZE(StructType, memberName) (offsetof(ARA::StructType, memberName) + sizeof(static_cast<ARA::StructType *> (nullptr)->memberName))
#else
    #define ARA_IMPLEMENTED_STRUCT_SIZE(StructType, memberName) (offsetof(StructType, memberName) + sizeof(((StructType *)NULL)->memberName))
#endif


//! Convenience macro to test if a field is present in a given struct.
//! \br
//! The ARA library C++ dispatchers implement a similar feature via templates,
//! see ARA::SizedStructPtr::implements<>().
#if defined(__cplusplus)
    #define ARA_IMPLEMENTS_FIELD(pointerToStruct, StructType, memberName) \
                                 ((pointerToStruct)->structSize > offsetof(ARA::StructType, memberName))
#else
    #define ARA_IMPLEMENTS_FIELD(pointerToStruct, StructType, memberName) \
                                 ((pointerToStruct)->structSize > offsetof(StructType, memberName))
#endif

//! @}

//! @}


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Debugging support
#endif

//! @defgroup Debugging Debugging
//! ARA strictly separates programming errors from runtime error conditions such as missing files,
//! CPU or I/O overloads etc.
//! Runtime errors occur when accessing external data and resources, which is always done on the host
//! side of the ARA API. Accordingly, the host has the responsibility to detect any such errors and
//! to properly communicate the issue to the user. Since ARA leverages existing technologies, host
//! implementations usually already feature proper code for this.
//! With the error reporting done on the host side, plug-ins do not need to know any details about
//! runtime errors - a simple bool to indicate success or failure is sufficient for implementing
//! normal operation or graceful error recovery. Thus, ARA does not need to define error codes for
//! communicating error details across the API.
//! As an example, consider audio data being read from a server across the network - if the connection
//! breaks, the host will recognize the issue and bring up an according user notification. If the
//! plug-in requests the now inaccessible audio data, the host simply flags that an error occurred and
//! the plug-in can either retry later or use silence as fallback data.
//! \br
//! A different kind of errors are programming errors. If either side fails to properly follow the
//! API contract, undefined behavior can occur. Tracking down such bugs from one side only can be
//! difficult and very time consuming, thus ARA strives to aid developers in this process by defining
//! a global assert function that both sides call whenever detecting programming errors related to
//! the ARA API.
//! When debugging (or when running unit tests), either side can provide the code for the assert
//! function, so that no matter what side you're debugging from you can always inject your custom
//! assert handling in order to be able to set proper breakpoints etc.
//! The assert function is only a debug facility: it will usually be disabled on end user systems,
//! and it must never be used for flow control in a shipping product. Instead, each side should
//! implement graceful fallback behavior after asserting the programming error, e.g. by defining a
//! special value for invalid object refs (NULL or -1, depending on the implementation) which will
//! be returned as a placeholder whenever object creation fails due to a programming error on the
//! other side and then filtering this value accordingly whenever objects are referenced.
//! @{
/***************************************************************************************************/

//! Hint about the nature of the programming error.
typedef ARA_32_BIT_ENUM(ARAAssertCategory)
{
    //! Not covered by any of the following codes.
    kARAAssertUnspecified = 0,

    //! Indicate that the caller passed invalid arguments.
    kARAAssertInvalidArgument = -1,

    //! Indicate that the call is invalid in the current state.
    //! e.g. if a document modification is being made without guarding it properly with
    //! ARADocumentControllerInterface::beginEditing() and ARADocumentControllerInterface::endEditing()
    kARAAssertInvalidState  = -2,

    //! Indicate that the call cannot be made on the current thread.
    kARAAssertInvalidThread = -3
};

//! Global assert function pointer.
//! The assert categories passed to the global assert function are useful both for guiding developers
//! when debugging and for automatic assert evaluation when building unit tests. \br
//! The diagnosis text is intended solely to aid the developer debugging an issue "from the
//! other side"; they must not be presented to the user (or even parsed for flow control).
//! If applicable (i.e. if the category is kARAAssertInvalidArgument), the diagnosis should contain
//! a hint about what problematicArgument actually points to - for example if a struct is too small,
//! you'd pass the pointer to the struct along with a diagnosis message a la:
//! "someExampleInterfacePointer->structSize < kExampleStructMinSize".
//! Creating such appropriate texts automatically can be easily accomplished by custom assert macros. \br
//! Finally, problematicArgument should point to the argument that contains the invalid data, so that
//! the developer on that end can quickly identify the problem. If you can't provide a meaningful
//! address for it, e.g. because the category is kARAAssertInvalidThread, pass NULL here.
typedef void (ARA_CALL * ARAAssertFunction) (ARAAssertCategory category, const void * problematicArgument,
                                             const char * diagnosis);

//! @}


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Model graph objects
#endif

//! @defgroup ARA_Model_Graph ARA Model Graph
//! @{
/***************************************************************************************************/


/***************************************************************************************************/
//! @defgroup Model_Document Document
//! The document is the root object for a model graph and typically represents a piece of music
//! such as a song or an entire performance.
//! It is bound to a document controller in a 1:1 relationship. The document controller is used to
//! manage the entire graph it contains. Because of the 1:1 relationship, the document is never
//! specified when calling into the document controller.
//! Edits of the document and any of the objects it contains are done in cycles started with
//! ARADocumentControllerInterface::beginEditing() and concluded with
//! ARADocumentControllerInterface::endEditing().
//! This allows plug-ins to deal with any render thread synchronization that may be necessary,
//! as well as postponing any internal updates until the end of the cycle when the ARA graph has
//! its full state available.
//! A document is the root object for persistency and is the owner of any amount of associated
//! audio sources, region sequences and musical contexts.
//! \br
//! Plug-in developers using the C++ ARA Library can use the ARA::PlugIn::Document class.
//! @{

//! Document properties.
//! Note that like all properties, a pointer to this struct is only valid for the duration of the
//! call receiving the pointer - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
typedef struct ARADocumentProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! User-readable name of the document as displayed in the host.
    //! The plug-in must copy the name, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a name for the document.
    //! In that case, the host should not make up some dummy name just to satisfy the API, but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper
    //! name visible somewhere in the host and a dummy name implicitly derived from other state.
    ARAUtf8String name;
} ARADocumentProperties;

// Convenience constant for easy struct validation.
enum { kARADocumentPropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARADocumentProperties, name) };
//! @}


/***************************************************************************************************/
//! @defgroup Model_Musical_Context Musical Context
//! A musical context describes both rhythmical concepts of the music such as bars and beats and
//! their distribution over time, as well as harmonic structures and their distribution over time.
//! A musical context is always owned by one document.
//! Musical contexts are not persistent when storing documents, instead the host re-creates them
//! as needed.
//! \br
//! Plug-in developers using the C++ ARA Library can use the ARA::PlugIn::MusicalContext class.
//! @{

//! Reference to the plug-in side representation of a musical context (opaque to the host).
typedef ARA_REF(ARAMusicalContextRef);
//! Reference to the host side representation of a musical context (opaque to the plug-in).
typedef ARA_HOST_REF(ARAMusicalContextHostRef);

//! Musical context properties.
//! Note that like all properties, a pointer to this struct is only valid for the duration of the
//! call receiving the pointer - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
typedef struct ARAMusicalContextProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! User-readable name of the musical context as displayed in the host.
    //! The plug-in must copy the name, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a name for the musical context (which is typically
    //! true for all hosts that only use a single context per document.)
    //! In that case, the host should not make up some dummy name just to satisfy the API, but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper
    //! name visible somewhere in the host and a dummy name implicitly derived from other state.
    ARA_ADDENDUM(2_0_Draft) ARAUtf8String name;

    //! Sort order of the musical context in the host.
    //! The index must allow for the plug-in to order the musical contexts as shown in the host,
    //! but the actual index values are not shown to the user. They can be arbitrary, but must
    //! increase strictly monotonically.
    ARA_ADDENDUM(2_0_Draft) ARAInt32 orderIndex;

    //! Color associated with the musical context in the host.
    //! The plug-in must copy the color, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a color for the musical context.
    ARA_ADDENDUM(2_0_Draft) const ARAColor * color;
} ARAMusicalContextProperties;

// Convenience constant for easy struct validation.
enum { kARAMusicalContextPropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAMusicalContextProperties, structSize) };

//! @}


/***************************************************************************************************/
//! @defgroup Model_Region_Sequences Region Sequences (Added In ARA 2.0)
//! Region sequences allow hosts to group playback regions, typically by "tracks" or "lanes" in
//! their arrangement.
//! Each sequence is associated with a musical context, and all regions in a sequence will be adapted
//! to that same context.
//! Further, all regions within a sequence are expected to play back through the same routing (incl.
//! same latency), typically the same "mixer track" or "audio channel".
//! Regions in a sequence can overlap, and such overlapping regions will sound concurrently.
//! A region sequence is always owned by one document, and refers to a musical context.
//! Region sequences are not persistent when storing documents, instead the host re-creates them
//! as needed.
//! \br
//! Plug-in developers using the C++ ARA Library can use the ARA::PlugIn::RegionSequence class.
//! @{

//! Reference to the plug-in side representation of a region sequence (opaque to the host).
ARA_ADDENDUM(2_0_Draft) typedef ARA_REF(ARARegionSequenceRef);
//! Reference to the host side representation of a region sequence (opaque to the plug-in).
ARA_ADDENDUM(2_0_Draft) typedef ARA_HOST_REF(ARARegionSequenceHostRef);

//! Region sequence properties.
//! Note that like all properties, a pointer to this struct is only valid for the duration of the
//! call receiving the pointer - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
ARA_ADDENDUM(2_0_Draft) typedef struct ARARegionSequenceProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! User-readable name of the region sequence as displayed in the host.
    //! The plug-in must copy the name, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a name for the region sequence.
    //! In that case, the host should not make up some dummy name just to satisfy the API, but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper
    //! name visible somewhere in the host and a dummy name implicitly derived from other state.
    ARAUtf8String name;

    //! Sort order of the region sequence in the host.
    //! The index must allow for the plug-in to order the region sequences as shown in the host,
    //! but the actual index values are not shown to the user. They can be arbitrary, but must
    //! increase strictly monotonically.
    ARAInt32 orderIndex;

    //! Musical context in which the playback regions of the sequence will be edited and rendered.
    //! Note that when rendering the playback regions via any plug-in instance, the time information
    //! provided for this plug-in through the companion API must match this musical context.
    ARAMusicalContextRef musicalContextRef;

    //! Color associated with the region sequence in the host.
    //! The plug-in must copy the color, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a color for the region sequence.
    ARA_ADDENDUM(2_0_Draft) const ARAColor * color;
} ARARegionSequenceProperties;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Draft) { kARARegionSequencePropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARARegionSequenceProperties, musicalContextRef) };

//! @}


/***************************************************************************************************/
//! @defgroup Model_Audio_Source Audio Source
//! An audio source represents a continuous sequence of sampled audio data. Typically a host will
//! create an audio source object for each audio file used with ARA plug-ins.
//! Conceptually, the contents of an audio source are immutable (even though updates are possible,
//! this is an expensive process, and user edits based on the modified content may get lost).
//! An audio source is always owned by one document, and in turn owns any amount of associated
//! audio modifications.
//! Audio sources are persistent when storing documents.
//! \br
//! Plug-in developers using the C++ ARA Library can use the ARA::PlugIn::AudioSource class.
//! @{

//! Reference to the plug-in side representation of an audio source (opaque to the host).
typedef ARA_REF(ARAAudioSourceRef);
//! Reference to the host side representation of an audio source (opaque to the plug-in).
typedef ARA_HOST_REF(ARAAudioSourceHostRef);

//! Audio source properties.
//! Note that like all properties, a pointer to this struct is only valid for the duration of the
//! call receiving the pointer - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
typedef struct ARAAudioSourceProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! User-readable name of the audio source as displayed in the host.
    //! The plug-in must copy the name, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a name for the audio source.
    //! In that case, the host should not make up some dummy name just to satisfy the API, but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper
    //! name visible somewhere in the host and a dummy name implicitly derived from other state.
    ARAUtf8String name;

    //! ID used to re-connect model graph when archiving/unarchiving.
    //! This ID must be unique for all audio sources within the document.
    //! The plug-in must copy the persistentID, the pointer may be only valid for the duration of the call.
    ARAPersistentID persistentID;

    //! Total number of samples per channel of the contained audio material.
    //! May only be changed while access to the audio source is disabled,
    //! see ARADocumentControllerInterface::enableAudioSourceSamplesAccess().
    ARASampleCount sampleCount;

    //! Sample rate of the contained audio material.
    //! May only be changed while access to the audio source is disabled,
    //! see ARADocumentControllerInterface::enableAudioSourceSamplesAccess().
    //! Note that the sample rate of the audio source may not match the sample rate(s) used in the
    //! companion plug-in instances that render playback regions based on this audio source -
    //! plug-ins must apply sample rate conversion as needed.
    //! However, if the sample rate is changed, plug-ins are not required to translate their model
    //! to the new sample rate, and may instead restart with a fresh analysis, causing all user
    //! edits applied at the previous sample rate to be lost.
    ARASampleRate sampleRate;

    //! Count of discrete channels of the contained audio material.
    //! May only be changed while access to the audio source is disabled,
    //! see ARADocumentControllerInterface::enableAudioSourceSamplesAccess().
    //! As with sample rate changes, plug-ins may discard all edits and start with a fresh analysis
    //! if the channel count is changed.
    ARAChannelCount channelCount;

    //! Flag to indicating that the data is available in a resolution that cannot be represented
    //! in 32 bit float samples without losing quality.
    //! Depending on its internal algorithms, the plug-in may or may not base its decision to
    //! read either 32 or 64 bit samples on this flag,
    //! see ARAAudioAccessControllerInterface::createAudioReaderForSource().
    ARABool merits64BitSamples;

    //! Type information of the data the opaque #channelArrangement actually points to.
    //! Host shall use the data type associated with the companion API that was used to create
    //! the respective document controller.
    ARA_ADDENDUM(2_0_Final) ARAChannelArrangementDataType channelArrangementDataType;

    //! Spacial arrangement information: defines which channel carries the signal from which direction.
    //! The data type that this pointer references is defined by #channelArrangementDataType,
    //! see ARAChannelArrangementDataType.
    //! \br
    //! If #channelCount not larger than 2 (i.e. mono or stereo), this information may omitted by
    //! setting #channelArrangementDataType to kARAChannelArrangementUndefined and #channelArrangement
    //! to NULL. The behavior is then the same as in hosts that do not support surround for ARA:
    //! for stereo, channel 0 is the left and channel 1 the right speaker.
    //! \br
    //! To determine which channel arrangements are supported by the plug-in, the host will use the
    //! companion API and read the valid render input formats.
    ARA_ADDENDUM(2_0_Final) const void * channelArrangement;
} ARAAudioSourceProperties;

// Convenience constant for easy struct validation.
enum { kARAAudioSourcePropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAAudioSourceProperties, merits64BitSamples) };

//! @}


/***************************************************************************************************/
//! @defgroup Model_Audio_Modification Audio Modification
//! An audio modification contains a set of musical edits that the user has made to transform
//! the content of an audio source when rendered by the ARA plug-in.
//! An audio modification is always owned by one audio source, and in turn owns any amount of
//! associated playback regions.
//! Audio modifications are persistent when storing documents.
//! \br
//! Plug-in developers using the C++ ARA Library can use the ARA::PlugIn::AudioModification class.
//! @{

//! Reference to the plug-in side representation of an audio modification (opaque to the host).
typedef ARA_REF(ARAAudioModificationRef);
//! Reference to the host side representation of an audio modification (opaque to the plug-in).
typedef ARA_HOST_REF(ARAAudioModificationHostRef);

//! Audio modification properties.
//! Note that like all properties, a pointer to this struct is only valid for the duration of the
//! call receiving the pointer - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
typedef struct ARAAudioModificationProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! User-readable name of the audio modification as displayed in the host.
    //! The plug-in must copy the name, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a name for the audio modification.
    //! In that case, the host should not make up some name (e.g. derived from the audio source), but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper name
    //! visible somewhere in the host and a dummy name implicitly derived from other state.
    ARAUtf8String name;

    //! ID used to re-connect model graph when archiving/unarchiving.
    //! This ID must be unique for all audio modifications within the document.
    //! The plug-in must copy the persistentID, the pointer may be only valid for the duration of the call.
    ARAPersistentID persistentID;
} ARAAudioModificationProperties;

// Convenience constant for easy struct validation.
enum { kARAAudioModificationPropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAAudioModificationProperties, persistentID) };

//! @}


/***************************************************************************************************/
//! @defgroup Model_Playback_Region Playback Region
//! A playback region is a reference to an arbitrary time section of an audio modification,
//! mapped to a certain section of playback time.
//! It is linked to a region sequence, which in turn is linked to a musical context.
//! All playback regions that share the same audio modification play back the same musical
//! content, but may adapt that content to the given section of the musical context and to the
//! content of other regions in the same region sequence (see content based fades).
//! Note that if a plug-in offers any user settings to control this adaptation (such as groove settings),
//! then these settings should be part of the audio modification state, not of the individual
//! playback regions.
//! A playback is always owned by one audio modification, and refers to a region sequence.
//! Playback regions are not persistent when storing documents, instead the host re-creates them
//! as needed.
//! \br
//! Plug-in developers using the C++ ARA Library can use the ARA::PlugIn::PlaybackRegion class.
//! @{

//! Reference to the plug-in side representation of a playback region (opaque to the host).
typedef ARA_REF(ARAPlaybackRegionRef);
//! Reference to the host side representation of a playback region (opaque to the plug-in).
typedef ARA_HOST_REF(ARAPlaybackRegionHostRef);

//! Playback region transformations.
//! Plug-ins may or may not support all transformations that can be configured in a playback region.
//! They express these capabilities at factory level, and the host must respect this.
//! Also used in ARAFactory::supportedPlaybackTransformationFlags.
typedef ARA_32_BIT_ENUM(ARAPlaybackTransformationFlags)
{
    //! Named constant if no flags are set.
    //! If no flags are set, the modification is played back "as is", without further adoption
    //! to the given playback situation.
    kARAPlaybackTransformationNoChanges = 0,

    //! Time-stretching enable flag.
    //! If time-stretching is supported by the plug-in, the host can use this flag to enable it.
    //! If disabled, the host must always specify the same duration in modification and playback time,
    //! and the plug-in should ignore ARAPlaybackRegionProperties::durationInModificationTime.
    kARAPlaybackTransformationTimestretch = 1 << 0,

    //! Time-stretching tempo configuration flag.
    //! If kARAPlaybackTransformationTimestretch is set, this flag allows to distinguish whether
    //! the stretching shall be done in a strictly linear fashion (flag is off) or whether it
    //! shall reflect the tempo relationship between the musical context and the content of the
    //! audio modification (flag is on).
    kARAPlaybackTransformationTimestretchReflectingTempo = 1 << 1,


    ARA_ADDENDUM(2_0_Draft) kARAPlaybackTransformationContentBasedFadeAtTail = 1 << 2,  //!< see ::kARAPlaybackTransformationContentBasedFades
    ARA_ADDENDUM(2_0_Draft) kARAPlaybackTransformationContentBasedFadeAtHead = 1 << 3,  //!< see ::kARAPlaybackTransformationContentBasedFades

    //! Content-based fades enabling flags.
    //! These flags are used to enable smart, content-based fades at either end of the playback region.
    //! If supported by the plug-in, the host no longer needs to apply its regular overall fades at
    //! region borders, but can instead delegate this functionality to the plug-in.
    //! Based on the region sequence grouping, the plug-in can determine neighboring regions and
    //! utilize head and tail time to calculate a smart, musical transition.
    //! Even when no neighboring region is found, it may be appropriate to fade in or out to avoid
    //! cutting off signals abruptly.
    //! Note that while the transformation of a playback region can be defined separately for each
    //! border of the region, it must be enabled for either both or neither border in the
    //! ARAFactory::supportedPlaybackTransformationFlags.
    ARA_ADDENDUM(2_0_Draft) kARAPlaybackTransformationContentBasedFades = kARAPlaybackTransformationContentBasedFadeAtHead | kARAPlaybackTransformationContentBasedFadeAtTail,
};


//! Playback region properties.
//! Note that like all properties, a pointer to this struct is only valid for the duration of the
//! call receiving the pointer - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
typedef struct ARAPlaybackRegionProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Configuration of possible transformations upon playback, i.e. time-stretching etc.
    //! The host may only enable flags that are listed in ARAFactory::supportedPlaybackTransformationFlags.
    ARAPlaybackTransformationFlags transformationFlags;

    //! #startInModificationTime and #durationInModificationTime define the
    //! audible audio modification time range. This section of the modification's audio data will
    //! be mapped to the song playback time range defined below as configured by the #transformationFlags,
    //! including optional time stretching. See @ref sec_ManipulatingTheTiming for more information.
    ARATimePosition startInModificationTime;

    //! See #startInModificationTime. \br
    //! Plug-ins must deal with #durationInModificationTime being 0.0.
    ARATimeDuration durationInModificationTime;

    //! #startInPlaybackTime and #durationInPlaybackTime define the relationship between the
    //! audible modification time range and the song playback time in seconds as communicated
    //! via the companion API.
    //! Musical context content (such as ::kARAContentTypeTempoEntries) is also expressed
    //! in song playback time.
    ARATimePosition startInPlaybackTime;

    //! See #startInPlaybackTime. \br
    //! Plug-ins must deal with #durationInPlaybackTime being 0.0.
    ARATimeDuration durationInPlaybackTime;

    //! Musical context in which the playback region will be edited and rendered.
    //! Note that when rendering the playback region via any plug-in instance, the time information
    //! provided for this plug-in through the companion API must match this musical context.
    //! \deprecated
    //! No longer used since adding region sequences in ARA 2.0, which already define the musical
    //! context for all their respective regions. If structSize indicates that a sequence is used
    //! (i.e. when using ARA 2.0 or higher), this field must be ignored by plug-ins. Hosts are
    //! free to additionally set a valid musical context if desired for ARA 1 backwards compatibility,
    //! or leave the field uninitialized otherwise.
    ARA_DEPRECATED(2_0_Draft) ARAMusicalContextRef musicalContextRef;

    //! Region sequence with which the playback region is associated in the host.
    //! Required when using ARA 2_0_Draft or newer.
    ARA_ADDENDUM(2_0_Draft) ARARegionSequenceRef regionSequenceRef;

    //! User-readable name of the playback region as displayed in the host.
    //! The plug-in must copy the name, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a name for the audio modification.
    //! In that case, the host should not make up some name (e.g. derived from the audio source), but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper name
    //! visible somewhere in the host and a dummy name implicitly derived from other state.
    ARA_ADDENDUM(2_0_Draft) ARAUtf8String name;

    //! Color associated with the playback region in the host.
    //! The plug-in must copy the color, the pointer may be only valid for the duration of the call.
    //! It may be NULL if the host cannot provide a color for the playback region.
    //! In that case, the host should not make up some color (e.g. derived from the track color), but
    //! rather let the plug-in do this if desired - this way it can distinguish between a proper color
    //! visible somewhere in the host and a dummy color implicitly derived from other state.
    ARA_ADDENDUM(2_0_Draft) const ARAColor * color;

} ARAPlaybackRegionProperties;

// Convenience constant for easy struct validation.
enum { kARAPlaybackRegionPropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlaybackRegionProperties, musicalContextRef) };

//! @}

//! @}


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Content Reading
#endif

// Note: when reading this header for the first time, you may want to skip the topic of content
// updates and content reading - proceed directly to the host controller interfaces to gain a better
// overall understanding of ARA, then come back to content reading later.
//! @defgroup Content_Reading Content Reading
//! @{
/***************************************************************************************************/


/***************************************************************************************************/
//! @defgroup Model_Content_Updates Content Updates
//! \br
//! There are several levels of abstraction when analyzing musical recordings.
//! Initially, there is the signal in its "physical" form.
//! On the next level, the signal interpreted as a series of musical events - the notes played when
//! creating the signal.
//! These notes and their relationship in time and pitch can be interpreted further, leading to
//! abstractions like tempo, bar signatures, key signatures, tuning and chords.
//! \br
//! Updates may happen on any of these levels, both independently or concurrently.
//! In the most simple but most unlikely case, the signal is completely replaced, and all the
//! higher abstractions therefore also invalidated. This also means that all user edits done in an
//! audio modification will be lost.
//! More likely is a minor modification of the signal, such as applying a high pass filtering to
//! remove rumble in the audio source. This will not change any higher abstractions (all notes etc.
//! remain the same), so any edits inside the audio modification or any notation of the music based
//! on the analysis will remain intact.
//! Another case is the correction of the analysis by the user. The signal does not change in this
//! case, but mis-detected notes are added or removed so the mid-level abstraction which is
//! considered with the notes changes. Whether or not this also changes higher interpretations
//! such as the detected harmonic structure depends on the case at hand.
//! \br
//! ARA defines a set of flags that allow to communicate the level of change, which helps to avoid
//! unnecessary flushing of the user edits inside an audio modification and allows for optimizations
//! of the analysis. The flags are providing a guarantee what has NOT changed.
//! This may seem odd at first but if for example a given host does not know about harmonies, it
//! cannot make any assumption about whether these have changed or not.
//! @{

//! Flags indicating the scope of a content update.
//! If notifying the API partner about a content update, the caller can make guarantees about which
//! abstractions of the signal are unaffected by the given change.
//! The enum flags describing these abstractions are or'd together into a single ARAInt32 value.
//! \br
//! The C++ ARA Library encapsulates content updates in the ARA::ContentUpdateScopes class.
typedef ARA_32_BIT_ENUM(ARAContentUpdateFlags)
{
    //! No flags set means update everything.
    kARAContentUpdateEverythingChanged = 0,

    //! The actual signal is unaffected by the change.
    //! Note that in some cases even when the signal is considered to be unchanged, the values of
    //! the actual sample data may change, e.g. if the user applies a sample rate conversion.
    //! Whenever the output signal for a playback region changes, this implies that the head
    //! and tail times for that region may have changed too, so the host must update these values
    //! after receiving such a change.
    kARAContentUpdateSignalScopeRemainsUnchanged = 1<<0,

    //! Content information for notes, beat-markers etc. is unaffected by the change.
    kARAContentUpdateNoteScopeRemainsUnchanged = 1<<1,

    //! Content information for tempo, bar signatures etc. is unaffected by the change.
    kARAContentUpdateTimingScopeRemainsUnchanged = 1<<2,

    //! Content readers for tuning (static or dynamic) are unaffected by the change. (added in ARA 2.0)
    ARA_ADDENDUM(2_0_Final) kARAContentUpdateTuningScopeRemainsUnchanged = 1<<3,

    //! Content readers for key signatures, chords etc. are unaffected by the change. (added in ARA 2.0)
    ARA_ADDENDUM(2_0_Final) kARAContentUpdateHarmonicScopeRemainsUnchanged = 1<<4
};

//! @}


/***************************************************************************************************/
//! @defgroup Model_Content_Readers_and_Content_Events Content Readers And Content Events
//! \br
//! Reading content description follows the same pattern both from the host and from the plug-in side.
//! ARA establishes iterator objects called content reader to access the data in small units called
//! content events. There are several types available, each defining a certain abstract representation
//! of its associated events.
//! \br
//! Upon creation, content readers are bound to a given content type and to an object of which the
//! content shall be read. Optionally the reader can be restricted to only cover a given time range.
//! Once created, its event count is queried and the individual events are read, then the reader is
//! disposed of. This is all done immediately, reader objects are only temporary objects that are
//! created and destroyed from the same stack frame.
//! \br
//! The data pointer returned when reading an event's data remains owned by the content reader and
//! must remain valid until the reader is either another event is read or the reader is destroyed.
//! \br
//! The events returned by the reader are sorted in an order that depends on the content type, but
//! generally follows their appearance on the timeline. If several events appear at the same
//! (start-)time, their order is not defined and the receiver must apply further sorting if desired.
//! \br
//! The C++ ARA Library offers convenient content reader classes for host and plug-in developers.
//! Host developers can read plug-in content using ARA::Host::ContentReader, and plug-in developers
//! can use ARA::PlugIn::HostContentReader to read host content.
//! @{

//! Reference to the plug-in side representation of a content reader (opaque to the host).
typedef ARA_REF(ARAContentReaderRef);
//! Reference to the host side representation of a content reader (opaque to the plug-in).
typedef ARA_HOST_REF(ARAContentReaderHostRef);

//! Types of data that can be shared between host and plug-in.
typedef ARA_32_BIT_ENUM(ARAContentType)
{
//! @name Physical scope descriptions: currently none.
//! May be added in later release, e.g. for restoration plug-ins.
//@{
//@}

//! @name Note scope descriptions.
//@{
    //! Returns const ARAContentNote * for each note.
    kARAContentTypeNotes  = 10,

/* postponed until needed
// at this point it's not sure if there really is a need for beat-markers in addition to notes, but
// there is a conceptual difference between both when dealing with chords, arpeggios etc.

    //! Returns const ARAContentBeatmarker * for each transient.
    kARAContentTypeBeatmarkers = 11,
*/
//@}

//! @name Time scope descriptions.
//@{
    //! Returns const ARAContentTempoEntry * for each tempo sync point.
    kARAContentTypeTempoEntries = 20,

    //! Returns const ARAContentBarSignature * for each bar signature change.
    //! In the original ARA 1 API, this value was called kARAContentTypeSignatures -
    //! while its name has been changed with ARA 2 to distinguish it from kARAContentTypeKeySignatures,
    //! its semantics and binary encoding are still the same.
    kARAContentTypeBarSignatures = 21,
//@}

//! @name Tuning, Key Signatures and Chords (added in ARA 2.0)
//! pitch scope descriptions:
//@{
    //! Returns single const ARAContentTuning *.
    ARA_ADDENDUM(2_0_Final) kARAContentTypeStaticTuning = 31,
//@}

//! @name Harmonic scope descriptions.
//@{
    //! Returns const ARAContentKeySignature * for each key signature change.
    ARA_ADDENDUM(2_0_Final) kARAContentTypeKeySignatures = 42,

    //! Returns const ARAContentChord * for each chord in a lead-sheet-like notation.
    //! (i.e. the sheet chord can imply notes that are not actually played in the audio,
    //! or vice versa. also, sheet chords are typically quantized to integer beats or even bars.)
    ARA_ADDENDUM(2_0_Final) kARAContentTypeSheetChords = 45
//@}
};


//! Content reader optional creation parameter: a range in time to filter content events.
//! As an optimization hint, a content reader can be asked to restrict its data to only those events
//! that intersect with the given time range. Reader implementations should strive to respect this
//! request, but focus on overall performance - the events actually returned may exceed the specified
//! range by any amount, and calling code must evaluate the returned event positions/event durations.
//! \br
//! Note that when calls accept a pointer to a content time range, that pointer is only valid for
//! the duration of the call - the data must be evaluated/copied inside the call, and the pointer
//! must not be stored anywhere.
//! Further, in most of these calls the pointer to a content range may be NULL, indicating that the
//! entire content range of the object should be read.
typedef struct ARAContentTimeRange
{
    //! Events at start time are considered part of the range.
    ARATimePosition start;

    //! Events at start time + duration are not considered part of the range.
    ARATimeDuration duration;
} ARAContentTimeRange;


//! Content grade: degree of reliability of the provided content information.
//! The most prominent use of the content grade is to solve conflicts between data provided by the
//! host and data found via analysis on the plug-in side. Another example is that when being notified
//! about content changes in the plug-in, a host may choose to trigger certain automatic updates only
//! if the grade of the content is above a certain reliability threshold.
typedef ARA_32_BIT_ENUM(ARAContentGrade)
{
    //! Default data used as placeholder value.
    //! This grade can be used when no actual content information is present, such as the widely
    //! used default tempo of 120 bpm. This grade will typically be encountered while the analysis
    //! is still pending or still being executed. It may also be used if analysis failed to provide
    //! any meaningful results.
    kARAContentGradeInitial  = 0,

    //! Data was provided by automatic content detection without any user intervention.
    //! Since the user has not reviewed the data, it may not be reliable in some cases.
    kARAContentGradeDetected = 1,

    //! Data was reviewed or edited by the user but not specifically approved as fully correct.
    //! This is the typical state of the data in the regular studio workflow.
    kARAContentGradeAdjusted = 2,

    //! Data has been specifically approved by the user as fully correct, e.g. for shipping it
    //! as part of a content library.
    kARAContentGradeApproved = 3
};

//! @}


/***************************************************************************************************/
//! @defgroup Model_Timeline Timeline
//! \br
//! ARA expresses musical timing as a mapping between song time measured in seconds and musical
//! time measured in quarter notes. The mapping is created by dividing the timeline into sections
//! of constant musical tempo. These tempo sections are then annotated as a list of tempo sync
//! points, where each point represents both the end of one and the beginning of another section.
//! The location of a tempo sync point is specified both in song time and musical time. The actual
//! tempo of a section can be easily derived from the relationship of the duration of the section
//! in song time and the duration of the section in musical time (note that neither the quarters nor
//! the seconds must necessarily be integer values here):
//! \verbatim
//!                     rightTempoEntry.quarterPosition - leftTempoEntry.quarterPosition
//! sectionTempoInBpm = ----------------------------------------------------------------- * 60.0 sec
//!                     rightTempoEntry.timePosition  - leftTempoEntry.timePosition
//! \endverbatim
//! The advantage of providing such sync points whenever the tempo changes instead of specifying
//! the tempo directly is that there are no rounding errors that sum up over time - whenever the
//! tempo changes, this happens fully in sync.
//! The disadvantage of this representation is that there is no way to express the tempo before
//! the first and after the last tempo sync point, because the initial and final tempo sections
//! stretch "forever" into the past resp. future. ARA works around this by defining that the
//! initial tempo is equal to the tempo between the first and the second tempo sync point and that
//! the final tempo is equal to the tempo between the last-but-one and the last tempo sync point.
//! This means that there must always be at least 2 sync points in a valid ARA time line definition.
//! \br
//! To ease parsing the timeline, ARA further requires that there must be a tempo sync point given
//! at quarter 0, even if there is no actual tempo change at this point in time. This allows for
//! precisely determining any offset between time 0 seconds and quarter 0 without introducing possible
//! rounding errors. (If a content range is specified, this only applies if quarter 0 is part of the
//! content range.)
//! \br
//! Musical timing is commonly not notated by simply counting quarter notes - instead bars are
//! defined that form repeating patterns. ARA expresses this by providing a list of bar signatures.
//! Like in standard musical notation, the bar signatures are expressed as a fraction of two integer
//! values: numerator/denominator.
//! The location of a bar signature is specified in musical time. To make sense musically, the
//! distance between two bar signatures must be an integer multiple of the bar length of the earlier
//! of the two signatures (even though the bar length itself may not be integer, e.g. when using a
//! measure of 7/8). Note that when implementing the translation of these values to/from your code,
//! potential rounding issues must be handled properly to ensure the desired positions are extracted.
//! @{

//! Content reader event class: tempo map provided by kARAContentTypeTempoEntries.
//! Event sort order is by timePosition.
//! As with all content readers, a pointer to this struct retrieved via getContentReaderDataForEvent()
//! is still owned by the callee and must remain valid until either getContentReaderDataForEvent()
//! is called again or the reader is destroyed via destroyContentReader().
typedef struct ARAContentTempoEntry
{
    //! Time in seconds relative to the start of the song or the audio source/modification.
    ARATimePosition timePosition;

    //! Corresponding time in quarter notes.
    ARAQuarterPosition quarterPosition;
} ARAContentTempoEntry;


//! Content reader event class: bar signatures provided by kARAContentTypeBarSignatures.
//! The event position relates to ARAContentTempoEntry, a valid tempo map must be provided
//! by any provider of ARAContentBarSignature.
//! Each bar signature is valid until the following one, and the first bar signature is assumed to
//! also be valid any time before it is actually defined.
//! The location of the first bar signature is also considered to be the location of bar 1.
//! Event sort order is by position.
//! As with all content readers, a pointer to this struct retrieved via getContentReaderDataForEvent()
//! is still owned by the callee and must remain valid until either getContentReaderDataForEvent()
//! is called again or the reader is destroyed via destroyContentReader().
typedef struct ARAContentBarSignature
{
    //! Numerator of the bar signature.
    ARAInt32 numerator;

    //! Denominator of the bar signature.
    ARAInt32 denominator;

    //! Start time in quarter notes, see ARAContentTempoEntry.
    ARAQuarterPosition position;
} ARAContentBarSignature;

//! @}


/***************************************************************************************************/
//! @defgroup Model_Notes Notes
//! \br
//! Notes in ARA correspond to what a composer would notate to describe the music.
//! Notes are described by their position in time, their pitch and their relative volume.
//! The pitch can be interpreted as frequency, but ARA also offers a musical description of the
//! pitch very similar to MIDI: it defines the tuning for the overall musical scale and provides
//! an integer number to identify the pitch for each note within this tuning, along with an average
//! detune for each note actually played.
//! ARA pitch numbers match MIDI note numbers, so that the note A4 has the value 69.
//! This note is also used to specify the tuning reference, commonly at 440 Hz.
//! At 440 Hz reference tuning the ARA pitch number 0 thus equals 8.1757989 Hz.
//! Some notes may not have a well-defined pitch, such as percussive notes. For such  notes,
//! a frequency of kARAInvalidFrequency and a pitch number of kARAInvalidPitchNumber are used.
//! @{

//! Quantized pitch, corresponds to the MIDI note number in the range 0...127, but may exceed this range.
typedef ARAInt32 ARAPitchNumber;

//! Used if there is no pitch associated with a note (e.g. purely percussive note).
#if defined(__cplusplus)
    constexpr ARAPitchNumber kARAInvalidPitchNumber { INT32_MIN };
#else
    #define kARAInvalidPitchNumber INT32_MIN
#endif

//! Used if there is no pitch associated with a note (e.g. purely percussive note).
#if defined(__cplusplus)
    constexpr float kARAInvalidFrequency { 0.0f };
#else
    #define kARAInvalidFrequency 0.0f
#endif

//! Default tuning reference.
#if defined(__cplusplus)
    constexpr float kARADefaultConcertPitchFrequency { 440.0f };
#else
    #define kARADefaultConcertPitchFrequency 440.0f
#endif


//! Content reader event class: notes provided by kARAContentTypeNotes.
//! Event sort order is by startPosition.
//! As with all content readers, a pointer to this struct retrieved via getContentReaderDataForEvent()
//! is still owned by the callee and must remain valid until either getContentReaderDataForEvent()
//! is called again or the reader is destroyed via destroyContentReader().
typedef struct ARAContentNote
{
    //! Average frequency in Hz, kARAInvalidFrequency if note has no defined pitch (percussive).
    float frequency;

    //! Index corresponding to MIDI note number (or kARAInvalidPitchNumber).
    ARAPitchNumber pitchNumber;

    //! Normalized level: 0.0f (weak) <= level <= 1.0f (strong).
    //! This value is scaled according to human perception (i.e. closer to a dB scale than to a linear one).
    float volume;

    //! Time in seconds marking the beginning of the note (aka "note on" in MIDI), relative to the
    //! start of the song or the audio source/modification.
    ARATimePosition startPosition;

    //! Time marking the musical/quantization anchor of the note, relative to the start of the note.
    ARATimeDuration attackDuration;

    //! Time marking the release point of the note (aka "note off" in MIDI), relative to the start of the note.
    ARATimeDuration noteDuration;

    //! Time marking the end of the entire sound of the note (end of release phase), relative to the start of the note.
    ARATimeDuration signalDuration;
} ARAContentNote;

//! @}


/***************************************************************************************************/
//! @defgroup Model_Tuning_Key_Signatures_and_Chords Tuning, Key Signatures and Chords (Added In ARA 2.0)
//! \br
//! ARA expresses "western standard" octave-cyclic, 12-tone scales as tunings and key signatures.
//! While some applications such as Melodyne offer a much more complex model that allows for acyclic
//! and/or micro-tonal scales, those models usually don't map well to each others, and introduce
//! a complexity that can not meaningfully be handled by applications with the "main stream" model.
//! Further, there is no standardized musical theory for expressing chords in such scales.
//! Should the actual need to deal with more complex scales arise in the future, a new content type
//! may be added to cover this.
//! @{

//! The root of a key signature or chord as an index (or angle) in the circle of fifths from 'C'.
//! Enharmonic equivalents such as Db and C# are distinguished:
//! \verbatim
//! ...
//! -5: Db
//! ...
//! -1: F
//! 0: C
//! 1: G
//! 2: D
//! ...
//! 7: C#
//! ...
//! 11: E#
//! ...
//! \endverbatim
ARA_ADDENDUM(2_0_Final) typedef ARAInt32 ARACircleOfFifthsIndex;


//! Content reader event class: periodic 12-tone tuning table provided by kARAContentTypeStaticTuning.
//! Defines the tuning of each pitch class in the octave-cyclic 12-tone pitch system.
//! Allows to import (12-tone) Scala files.
//! Stretched tunings are not supported by ARA at this point, but may be added in a future release
//! as an additional tuning stretch curve applied on top of this average tuning.
//! ARA defines a single overall tuning (i.e. there's always only one event for this reader).
//! As with all content readers, a pointer to this struct retrieved via getContentReaderDataForEvent()
//! is still owned by the callee and must remain valid until either getContentReaderDataForEvent()
//! is called again or the reader is destroyed via destroyContentReader().
ARA_ADDENDUM(2_0_Final) typedef struct ARAContentTuning
{
    //! Frequency of the concert pitch 'A' in hertz, defaulting to kARADefaultConcertPitchFrequency aka 440.0f.
    float concertPitchFrequency;

    //! Root key for the following per-key tunings.
    ARACircleOfFifthsIndex root;

    //! Tuning of each note pitch as an offset from the equal temperament tuning in cent.
    //! Each entry defaults to 0.0f.
    //! The first entry relates to the root, increasing chromatically up to the full octave.
    //! \verbatim
    //! Example:
    //! Arabian Rast: {0.0f, 0.0f, 0.0f, 0.0f, -50.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -50.0f}
    //! \endverbatim
    float tunings[12];

    //! User-readable name of the tuning as displayed in the content provider.
    //! The tuning name may or may not include the root note, depending on context - for example,
    //! equal temperament does not care about the root note so it is always omitted, Werckmeister
    //! tunings typically imply a root note of C unless noted explicitly otherwise, etc.
    //! The name is provided only for display purposes, in case the receiver wants to display the
    //! tuning exactly as done in the sender instead of utilizing its built-in naming system for
    //! tunings based on the above properties.
    //! The receiver must copy the name, the pointer may be only valid as long as the containing
    //! ARAContentTuning struct.
    //! It may be NULL if the provider does not provide a name for the tuning in its UI.
    ARAUtf8String name;
} ARAContentTuning;


//! The ARAKeySignatureIntervalUsage defines whether a particular interval is used
//! (kARAKeySignatureIntervalUsed) or not (kARAKeySignatureIntervalUnused).
//! Future extensions of the API could further specify the usage of a given interval, similar to the
//! chord intervals below. However since there are currently no clear-cut use cases for such a
//! distinction, this is not yet specified.
ARA_ADDENDUM(2_0_Final) typedef ARAByte ARAKeySignatureIntervalUsage;

//! @name Markup values of ARAKeySignatureIntervalUsage.
//! @{

#if defined(__cplusplus)
    //! Marks an interval of the ARAContentKeySignature as unused.
    ARA_ADDENDUM(2_0_Final) constexpr ARAKeySignatureIntervalUsage kARAKeySignatureIntervalUnused { 0x00 };
    //! Marks an interval of the ARAContentKeySignature as used.
    ARA_ADDENDUM(2_0_Final) constexpr ARAKeySignatureIntervalUsage kARAKeySignatureIntervalUsed { 0xFF };
#else
    #define kARAKeySignatureIntervalUnused (ARA_ADDENDUM(2_0_Final) (ARAKeySignatureIntervalUsage)0x00)
    #define kARAKeySignatureIntervalUsed   (ARA_ADDENDUM(2_0_Final) (ARAKeySignatureIntervalUsage)0xFF)
#endif

//! @}

//! Content reader event class: key signature provided by kARAContentTypeKeySignatures.
//! Defines the usage of each pitch class in the octave-cyclic 12-tone pitch system.
//! This content type describes the key signatures as would be annotated in a score, not the local
//! scales (which may be using some out-of-key notes via additional per-note accidentals).
//! The event position relates to ARAContentTempoEntry, a valid tempo map must be provided
//! by any provider of ARAContentBarSignature.
//! Each key signature is valid until the following one, the first key signature is assumed to also
//! be valid any time before it is actually defined.
//! Event sort order is by position.
//! As with all content readers, a pointer to this struct retrieved via getContentReaderDataForEvent()
//! is still owned by the callee and must remain valid until either getContentReaderDataForEvent()
//! is called again or the reader is destroyed via destroyContentReader().
ARA_ADDENDUM(2_0_Final) typedef struct ARAContentKeySignature
{
    //! Root key of the signature.
    ARACircleOfFifthsIndex root;

    //! Scales intervals (aka scale mode) of the signature.
    //! The index of this arrays entry is the chromatic interval to the keys root pitch class.
    //! \verbatim
    //! Examples (Hex Values):
    //! major            {FF, 00, FF, 00, FF, FF, 00, FF, 00, FF, 00, FF}
    //! natural minor    {FF, 00, FF, FF, 00, FF, 00, FF, FF, 00, FF, 00}
    //! \endverbatim
    ARAKeySignatureIntervalUsage intervals[12];

    //! Optional user-readable name of the key signature as displayed in the content provider
    //! (including the root note name).
    //! Typically, the receiver has a built-in system to generate a suitable name for a key
    //! signature based on its internal abstractions. However, this internal model might cover all
    //! states that the ARA model can provide, or vice versa. If there is such a mismatch, the
    //! internal name generation algorithm will likely fail - in which case the receiver may fall
    //! back to the string provided here.
    //! Note that the utility library that ships with the ARA SDK contains C++ code that can create
    //! a proper name for the most common key signatures, otherwise falling back to this name.
    //! The receiver must copy the name, the pointer may be only valid as long as the containing
    //! ARAContentKeySignature struct.
    //! It may be NULL if the provider does not provide a name for the key signature in its UI.
    //! When encoding the string to send it across the ARA API, flat and sharp symbols must be
    //! represented with the Unicodes 0x266D "MUSIC FLAT SIGN" and  0x266F "MUSIC SHARP SIGN"
    //! respectively (as also required for ARAContentChord::name).
    ARAUtf8String name;

    //! Start time in quarter notes, see ARAContentTempoEntry.
    ARAQuarterPosition position;
} ARAContentKeySignature;


//! The ARAChordIntervalUsage defines whether a particular interval is used
//! (kARAChordIntervalUsed) or not (kARAChordIntervalUnused), or if used may instead further
//! specify the function of the interval in the chord by specifying its diatonic degree:
//! 1 = unison, 3 = third, up to 13 = thirteenth
//! Note that the bass note of a chord is treated separately, see below.
typedef ARAByte ARAChordIntervalUsage;

//! @name Markup values of ARAChordIntervalUsage.
//! \verbatim
//! common degrees per note if root is C:
//!   C           D            E     F              G                  A             B
//!   1    b9    2/9   #9/3    3    4/11   #11/b5   5    #5/b6/b13   6/7/13  7/#13   7
//!                                                              (7 only if dim)
//! \endverbatim
//! @{

#if defined(__cplusplus)
    //! ARAChordIntervalUsage value when the corresponding chromatic interval is used with the given diatonic function.
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree1 { 0x01 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree2 { 0x02 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree3 { 0x03 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree4 { 0x04 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree5 { 0x05 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree6 { 0x06 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree7 { 0x07 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree9 { 0x09 };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree11 { 0x0B };
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordDiatonicDegree13 { 0x0D };
    //! ARAChordIntervalUsage value when the corresponding chromatic interval is used, but its diatonic function is unknown.
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordIntervalUsed { 0xFF };
    //! ARAChordIntervalUsage value when the corresponding chromatic interval is not used.
    ARA_ADDENDUM(2_0_Final) constexpr ARAChordIntervalUsage kARAChordIntervalUnused { 0x00 };
#else
    #define kARAChordDiatonicDegree1  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x01)
    #define kARAChordDiatonicDegree2  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x02)
    #define kARAChordDiatonicDegree3  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x03)
    #define kARAChordDiatonicDegree4  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x04)
    #define kARAChordDiatonicDegree5  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x05)
    #define kARAChordDiatonicDegree6  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x06)
    #define kARAChordDiatonicDegree7  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x07)
    #define kARAChordDiatonicDegree9  (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x09)
    #define kARAChordDiatonicDegree11 (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x0B)
    #define kARAChordDiatonicDegree13 (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x0D)
    #define kARAChordIntervalUsed     (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0xFF)
    #define kARAChordIntervalUnused   (ARA_ADDENDUM(2_0_Final) (ARAChordIntervalUsage)0x00)
#endif

//! @}

//! Content reader event class: chords provided by kARAContentTypeSheetChords.
//! The event position relates to ARAContentTempoEntry, a valid tempo map must be provided
//! by any provider of ARAContentBarSignature.
//! Each chord is valid until the following one, and the first chord is assumed to also be valid
//! any time before it is actually defined (i.e. its position is effectively ignored).
//! The "undefined chord" markup (all intervals unused) can be used to express a range where no
//! chord is applicable. Such gaps may appear between "regular" chords, or they can be used
//! to limit the otherwise infinite duration of the first and last "regular" chord if desired.
//! Event sort order is by position.
//! As with all content readers, a pointer to this struct retrieved via getContentReaderDataForEvent()
//! is still owned by the callee and must remain valid until either getContentReaderDataForEvent()
//! is called again or the reader is destroyed via destroyContentReader().
ARA_ADDENDUM(2_0_Final) typedef struct ARAContentChord
{
    //! Root note of the chord.
    //! \br
    //! Examples: F: root = -1    C/E: root = 0    G/D: root = 1
    ARACircleOfFifthsIndex root;

    //! Bass note of the chord.
    //! Usually identical to the root, but may be different from root,
    //! or even different from any other note represented by the intervals below.
    //! \br
    //! Examples: F: bass = -1    C/E: bass = 4    G/D: bass = 2
    ARACircleOfFifthsIndex bass;

    //! Chords intervals, defining gender, suspensions and extensions.
    //! The index of this array's entry is the chromatic interval to
    //! the chords root pitch class.
    //! Depending on the chord's musical interpretation, the interval
    //! corresponding to the bass note may or may not be included in here.
    //! If all intervals are unused, this chord represents an "undefined chord",
    //! which is used as a markup for gaps in the chord progression.
    //! \verbatim
    //! Examples (hexadecimal values):
    //! major            {FF, 00, 00, 00, FF, 00, 00, FF, 00, 00, 00, 00}
    //! major            {01, 00, 00, 00, 03, 00, 00, 05, 00, 00, 00, 00}
    //! minor            {01, 00, 00, 03, 00, 00, 00, 05, 00, 00, 00, 00}
    //! major 13         {01, 00, 09, 00, 03, 00, 00, 05, 00, 0D, 07, 00}
    //! major add13      {01, 00, 00, 00, 03, 00, 00, 05, 00, 0D, 00, 00}
    //! major 6          {01, 00, 00, 00, 03, 00, 00, 05, 00, 06, 00, 00}
    //! \endverbatim
    ARAChordIntervalUsage intervals[12];

    //! Optional user-readable name of the chord as displayed in the content provider
    //! (including the root note name).
    //! Typically, the receiver has a built-in system to generate a suitable name for a chord
    //! based on its internal abstractions. However, this internal model might cover all
    //! states that the ARA model can provide, or vice versa. If there is such a mismatch, the
    //! internal name generation algorithm will likely fail - in which case the receiver may fall
    //! back to the string provided here.
    //! Note that the utility library that ships with the ARA SDK contains C++ code that can create
    //! a proper name for any ARA chord, completely avoiding the necessity to use this name string.
    //! The receiver must copy the name, the pointer may be only valid as long as the containing
    //! ARAContentChord struct.
    //! It may be NULL if the provider does not provide a name for the chord in its UI.
    //! \br
    //! Chord annotation systems often use symbols in addition to Latin letters and Arabic numbers.
    //! Depending on UI considerations such as the font in use, different applications may use
    //! different Unicode codes to represent the same symbols internally.
    //! In order to send strings containing such symbol across the ARA API without ambiguities,
    //! they must be mapped to the following Unicode codes:
    //! - flat symbol:
    //!   0x266D "MUSIC FLAT SIGN"
    //! - sharp symbol:
    //!   0x266F "MUSIC SHARP SIGN"
    //! - upward-pointing triangle used to annotate major 7 chords:
    //!   0x2206 "INCREMENT"
    //! - minus sign used to annotate minor chords:
    //!   0x002D "HYPHEN-MINUS"
    //! - circle with slash used to annotate half-diminished chords:
    //!   0x00F8 "LATIN SMALL LETTER O WITH STROKE"
    //! - circle used to annotate diminished chords:
    //!   0x00B0 "DEGREE SIGN"
    //! - plus sign used to annotate augmented chords:
    //!   0x002B "PLUS SIGN"
    ARAUtf8String name;

    //! Start time in quarter notes, see ARAContentTempoEntry.
    ARAQuarterPosition position;
} ARAContentChord;

//! @}

//! @}


/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Host side controller interfaces
#endif

//! @defgroup Host_Interfaces Host Interfaces
//! @{
/***************************************************************************************************/


/***************************************************************************************************/
//! @defgroup Host_Audio_Access_Controller Audio Access Controller
//! This interface allows plug-ins to read the audio data from the host in a random access order.
//! It is used from multiple threads, both host and plug-in need to carefully observe the threading
//! rules for each function. The basic design idea is that each audio reader is used single-threaded,
//! but multiple audio readers can work concurrently (even on the same audio source).
//! Audio readers can be considered random access iterators, and like most iterators operate on
//! conceptually constant data structures. If the host changes sample rate, channel count or other
//! audio source properties that the reader relies upon, the plug-in must discard any existing
//! audio readers for the source and later re-create them based on the new configuration.
//! The host can temporarily disable access to the audio source in order to control the exact timing
//! of stopping the readers from accessing the source.
//! Whenever an audio reader is destroyed, the plug-in is responsible for thread safety - it may
//! need to block until a concurrent read operation on the I/O thread has finished. Hosts must take
//! care in their audio reader implementation to avoid potential deadlocks in this situation.
//! Note that when rendering, the audio source will often not be read in a consecutive order -
//! depending on the edits the user applied at audio modification level, the access may jump back
//! and forth quite often. The reader implementation should be optimized accordingly.
//! \br
//! Host developer using C++ ARA Library can implement the ARA::Host::AudioAccessControllerInterface.
//! For plug-in developers this interface is wrapped by the ARA::PlugIn::HostAudioAccessController.
//! @{

//! Reference to the host side representation of an audio access controller (opaque to the plug-in).
typedef ARA_HOST_REF(ARAAudioAccessControllerHostRef);

//! Reference to the host side representation of an audio reader (opaque to the plug-in).
typedef ARA_HOST_REF(ARAAudioReaderHostRef);

//! Host interface: audio access controller.
//! As with all host interfaces, the function pointers in this struct must remain valid until
//! all document controllers on the plug-in side that use it have been destroyed.
typedef struct ARAAudioAccessControllerInterface
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Synchronous reading: blocking until data is available
//@{
    //! Create audio reader instance to access sample data in an audio source.
    //! The format of the data is matching the format of the audio source, with a choice of reading
    //! samples as 32 bit or 64 bit (hosts must support both formats).
    //! Similar to the rules for creating content readers, the plug-in may call these functions only
    //! from calls in the "Audio Source Management" section of ARADocumentControllerInterface for
    //! the particular audio source the call is referring to, or from
    //! ARADocumentControllerInterface::endEditing() for any audio source.
    //! Contrary to content readers, audio readers are long-living objects that are not bound to a
    //! certain stack frame.
    ARAAudioReaderHostRef (ARA_CALL *createAudioReaderForSource) (ARAAudioAccessControllerHostRef controllerHostRef,
                                                                  ARAAudioSourceHostRef audioSourceHostRef, ARABool use64BitSamples);

    //! Read audio samples.
    //! The samples are provided in non-interleaved buffers of double or float data, depending on
    //! whether use64BitSamples was set when creating the reader. The channel count equals the
    //! channel count of the audio source. The data alignment and byte order always matches the
    //! machine's native layout.
    //! If the requested sample range extends beyond the start or end of the audio source, the
    //! out-of-range samples should be filled with silence. This should not be treated as an error.
    //! This potentially blocking function may be called from any non-realtime thread (including
    //! threads for offline rendering), but not from more than one thread per reader at the same time.
    //! The host may decide to let the thread sleep until the requested data is available, the
    //! plug-in must be designed to deal with this without triggering priority inversion.
    //! The target buffer(s) are provided by the caller.
    //! Result is kARATrue upon success, or kARAFalse when there is a critical, nonrecoverable
    //! I/O error, such as a network failure while the file is being read from a server.
    //! In case of failing in this call, the buffers must be filled with silence and the host must
    //! notify the user about the problem in an appropriate way. The plug-in must deal gracefully
    //! with any such I/O errors, both during analysis and rendering.
    ARABool (ARA_CALL *readAudioSamples) (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef,
                                          ARASamplePosition samplePosition, ARASampleCount samplesPerChannel, void * const buffers[]);

    //! Destroy given audio reader created by the host.
    //! The caller must guarantee that the reader is currently not in use in some other thread.
    //! See ARAAudioAccessControllerInterface::createAudioReaderForSource() about the restrictions when to call this.
    void (ARA_CALL *destroyAudioReader) (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef);
//@}
} ARAAudioAccessControllerInterface;

// Convenience constant for easy struct validation.
enum { kARAAudioAccessControllerInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAAudioAccessControllerInterface, destroyAudioReader) };

//! @}


/***************************************************************************************************/
//! @defgroup Host_Archiving_Controller Archiving Controller
//! This interface allows plug-ins to read and write archives with minimal memory impact.
//! It also allows for displaying progress when archiving or unarchiving model graphs.
//! Its functions may only be called during the archiving/unarchiving process.
//! \br
//! Because of the potentially large size of the archives, ARA does not use simple monolithic memory
//! blocks as known from many companion APIs. Instead, it establishes a stream-like archive format
//! so that copying large blocks of memory can be avoided.
//! Plug-ins that create large archives should use any mean of data reduction that is appropriate
//! to reduce the archive size. For example, they may implement gzip compression. Since it has good
//! knowledge of the characteristics of the data, it can configure the compression algorithms so
//! that optimal results are achieved. Consequently, there's no point for host to try to compress
//! the data any further with generic algorithms.
//! \br
//! Hosts that support both 32 and 64 bit architectures shall be aware of the fact that ARA archive
//! sizes are pointer-sized data types, so they will differ in bit width between these architectures.
//! This must be taken into account when storing the archive size in the host's document structure.
//! Also, when importing documents from 64 bit into 32 bit, the host must check whether the archive
//! is small enough to be loaded at all (i.e. its size fits into 32 bits). If not, it shall refuse
//! to load the archive and provide a proper error message.
//! This may seem like a restriction, but the reasoning behind this is that if the archive already
//! exceeds the available address space, the resulting unarchived graph will do so too.
//! \br
//! There's no creation or destruction call for the archive readers/writers because they are provided
//! by the host for the duration of the (un-)archiving process, so the lifetime is implicitly defined.
//! \br
//! When using API generation 1 or older and loading an archive through the deprecated functions
//! begin-/endRestoringDocumentFromArchive(), plug-ins may choose to access the associated archive reader
//! upon either begin- or endRestoringDocumentFromArchive() or even upon both calls, as suitable for their
//! implementation - hosts must be able to provide the requested data during the duration of both calls.
//! \br
//! Host developer using C++ ARA Library can implement the ARA::Host::ArchivingControllerInterface.
//! For plug-in developers this interface is wrapped by the ARA::PlugIn::HostArchivingController.
//! @{

//! Reference to the host side representation of an archiving controller (opaque to the plug-in).
typedef ARA_HOST_REF(ARAArchivingControllerHostRef);

//! Reference to the host side representation of an archive reader (opaque to the plug-in).
typedef ARA_HOST_REF(ARAArchiveReaderHostRef);
//! Reference to the host side representation of an archive writer (opaque to the plug-in).
typedef ARA_HOST_REF(ARAArchiveWriterHostRef);

//! Host interface: archive controller.
//! As with all host interfaces, the function pointers in this struct must remain valid until
//! all document controllers on the plug-in side that use it have been destroyed.
typedef struct ARAArchivingControllerInterface
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Reading Archives
//@{
    //! Query the size of the archive.
    //! This may only be called from ARADocumentControllerInterface::restoreObjectsFromArchive(),
    //! or if using API generation 1 from the deprecated begin-/endRestoringDocumentFromArchive() calls.
    //! Plug-ins must respect this size when reading the archive, reading beyond the end of the data
    //! is a programming error (and should thus be asserted by the host).
    ARASize (ARA_CALL *getArchiveSize) (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef);

    //! Read bytes.
    //! This may only be called from ARADocumentControllerInterface::restoreObjectsFromArchive(),
    //! or if using API generation 1 from the deprecated begin-/endRestoringDocumentFromArchive() calls.
    //! Result is kARATrue upon success, or kARAFalse when there is a critical, nonrecoverable
    //! I/O error, such as a network failure while the file is being read from a server.
    //! In case of failing in this call, the host must notify the user about the problem in some
    //! appropriate way. The archive will not be restored by the plug-in then, it'll fall back into
    //! some proper initial state for the affected objects.
    ARABool (ARA_CALL *readBytesFromArchive) (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef,
                                              ARASize position, ARASize length, ARAByte buffer[]);
//@}

//! @name Writing Archives
//@{
    //! Write bytes.
    //! This may only be called from storeObjectsToArchive() or the deprecated storeDocumentToArchive().
    //! Result is kARATrue upon success, or kARAFalse when there is a critical, nonrecoverable
    //! I/O error, such as a network failure while the file is being written to a server.
    //! In case of failing in this call, the host must notify the user about the problem in an appropriate way.
    //! \br
    //! Note that a plug-in should strive to write the data consecutively in a stream-like manner.
    //! Nevertheless, repositioning is needed to support chunk-style archives where the chunk length
    //! must be specified at the start of the chunk, but is not known until the chunk data has been
    //! fully created (and possibly compressed). In that case, the plug-in will need to "rewind" to
    //! the chunk size entry in the chunk header after writing the chunk and update it accordingly.
    //! As in most file APIs, any range of bytes that was skipped when writing should be filled with
    //! 0 by the host.
    ARABool (ARA_CALL *writeBytesToArchive) (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveWriterHostRef archiveWriterHostRef,
                                             ARASize position, ARASize length, const ARAByte buffer[]);
//@}

//! @name (Un-)Archiving Progress Information
//@{
    //! Message to the host signaling document save progress, value ranges from 0.0f to 1.0f.
    //! This may only be called from storeObjectsToArchive() or from the deprecated storeDocumentToArchive().
    //! In order to keep CPU load low, plug-ins should try to keep the update rate for this call
    //! as low as about 1000 calls per archive, which equals increments of 0.1%.
    void (ARA_CALL *notifyDocumentArchivingProgress) (ARAArchivingControllerHostRef controllerHostRef, float value);

    //! Message to the host signaling document save progress, value ranges from 0.0f to 1.0f.
    //! This may only be called from ARADocumentControllerInterface::restoreObjectsFromArchive(),
    //! or if using API generation 1 from the deprecated begin-/endRestoringDocumentFromArchive()
    //! and the associated model object creation/update calls guarded by them. In the deprecated form,
    //! the first call should be made from begin- and the last call from endRestoringDocumentFromArchive().
    //! In order to keep CPU load low, plug-ins should try to keep the update rate for this call
    //! as low as about 1000 calls per archive, which equals increments of 0.1%.
    void (ARA_CALL *notifyDocumentUnarchivingProgress) (ARAArchivingControllerHostRef controllerHostRef, float value);
//@}

//! @name Reading Archives
//@{
    //! Query the document archive ID that the plug-in's factory provided when saving the archive.
    //! This may only be called from ARADocumentControllerInterface::restoreObjectsFromArchive().
    //! Plug-ins can use this information to optimize their unarchiving code in case different
    //! archive formats are used depending on document archive ID.
    //! The returned pointer is owned by the host and must remain valid until the archive reader
    //! is destroyed.
    //! All hosts that support kARAAPIGeneration_2_0_Final or newer must implement this call.
    ARA_ADDENDUM(2_0_Final) ARAPersistentID (ARA_CALL *getDocumentArchiveID) (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef);
//@}
} ARAArchivingControllerInterface;

// Convenience constant for easy struct validation.
enum { kARAArchivingControllerInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAArchivingControllerInterface, notifyDocumentUnarchivingProgress) };

//! @}


/***************************************************************************************************/
//! @defgroup Host_Model_Content_Access_Controller Model Content Access Controller
//! This optional interface provides access to host model data such as the musical context.
//! Its functions may only be called from ARADocumentControllerInterface.create...() or update...()
//! for the object currently created/updated, or from ARADocumentControllerInterface::endEditing()
//! for any object.
//! \br
//! Host developer using C++ ARA Library can implement the ARA::Host::ContentAccessControllerInterface.
//! For plug-in developers this interface is wrapped by the ARA::PlugIn::HostContentAccessController.
//! @{

//! Reference to the host side representation of a content access controller (opaque to the plug-in).
typedef ARA_HOST_REF(ARAContentAccessControllerHostRef);

//! Host interface: content access controller.
//! As with all host interfaces, the function pointers in this struct must remain valid until
//! all document controllers on the plug-in side that use it have been destroyed.
typedef struct ARAContentAccessControllerInterface
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Content Reader Management
//! These functions mirror the content reader section in ARADocumentControllerInterface.
//@{
    //! Query whether the given content type is currently available for the given musical context.
    ARABool (ARA_CALL *isMusicalContextContentAvailable) (ARAContentAccessControllerHostRef controllerHostRef,
                                                          ARAMusicalContextHostRef musicalContextHostRef, ARAContentType contentType);

    //! Query the current quality of the information provided for the given musical context and content type.
    ARAContentGrade (ARA_CALL *getMusicalContextContentGrade) (ARAContentAccessControllerHostRef controllerHostRef,
                                                               ARAMusicalContextHostRef musicalContextHostRef, ARAContentType contentType);

    //! Create a content reader for the given musical context and content type.
    //! This should only be called after availability has been confirmed using isMusicalContextContentAvailable(),
    //! and is mainly used to communicate the song timeline to the plug-in.
    //! The time range may be NULL, which means that the entire musical context shall be read.
    //! If a time range is specified, all events that at least partially intersect with the range
    //! will be read.
    ARAContentReaderHostRef (ARA_CALL *createMusicalContextContentReader) (ARAContentAccessControllerHostRef controllerHostRef, ARAMusicalContextHostRef musicalContextHostRef,
                                                                           ARAContentType contentType, const ARAContentTimeRange * range);

    //! Query whether the given content type is currently available for the given audio source.
    ARABool (ARA_CALL *isAudioSourceContentAvailable) (ARAContentAccessControllerHostRef controllerHostRef,
                                                       ARAAudioSourceHostRef audioSourceHostRef, ARAContentType contentType);

    //! Query the current quality of the information provided for the given audio source and content type.
    ARAContentGrade (ARA_CALL *getAudioSourceContentGrade) (ARAContentAccessControllerHostRef controllerHostRef,
                                                            ARAAudioSourceHostRef audioSourceHostRef, ARAContentType contentType);

    //! Create a content reader for the given audio source and content type.
    //! This should only be called after availability has been confirmed using isAudioSourceContentAvailable().
    //! The host may be able to provide meta-information such as a known tempo or a known set of
    //! notes used in the material, which may enable the plug-in to speed up or even skip completely
    //! certain analysis passes (often depending on the content grade of the information).
    //! The time range may be NULL, which means that the entire audio source shall be read.
    //! If a time range is specified, all events that at least partially intersect with the range
    //! will be read.
    ARAContentReaderHostRef (ARA_CALL *createAudioSourceContentReader) (ARAContentAccessControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef,
                                                                        ARAContentType contentType, const ARAContentTimeRange * range);

    //! Query how many events the given reader exposes.
    ARAInt32 (ARA_CALL *getContentReaderEventCount) (ARAContentAccessControllerHostRef controllerHostRef, ARAContentReaderHostRef contentReaderHostRef);

    //! Query data of the given event of the given reader.
    //! The returned pointer is owned by the host and must remain valid until either
    //! getContentReaderDataForEvent() is called again or the content reader is destroyed.
    const void * (ARA_CALL *getContentReaderDataForEvent) (ARAContentAccessControllerHostRef controllerHostRef,
                                                           ARAContentReaderHostRef contentReaderHostRef, ARAInt32 eventIndex);

    //! Destroy the given content reader.
    void (ARA_CALL *destroyContentReader) (ARAContentAccessControllerHostRef controllerHostRef, ARAContentReaderHostRef contentReaderHostRef);
//@}
} ARAContentAccessControllerInterface;

// Convenience constant for easy struct validation.
enum { kARAContentAccessControllerInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAContentAccessControllerInterface, destroyContentReader) };

//! @}


/***************************************************************************************************/
//! @defgroup Host_Model_Update_Controller_Interface Model Update Controller Interface
//! This optional host interface allows the host to be notified about content changes in the plug-in.
//! Its functions may only be called from ARADocumentControllerInterface::notifyModelUpdates().
//! \br
//! Host developer using C++ ARA Library can implement the ARA::Host::ModelUpdateControllerInterface.
//! For plug-in developers this interface is wrapped by the ARA::PlugIn::HostModelUpdateController.
//! @{

//! Reference to the host side representation of a model update controller (opaque to the plug-in).
typedef ARA_HOST_REF(ARAModelUpdateControllerHostRef);

//! Audio source analysis progress indication.
typedef ARA_32_BIT_ENUM(ARAAnalysisProgressState)
{
    //! Required as first state for any given analysis.
    kARAAnalysisProgressStarted = 0,

    //! State for normal progress of an analysis.
    kARAAnalysisProgressUpdated = 1,

    //! Required as last state for any given analysis (no matter whether it completed or was cancelled).
    kARAAnalysisProgressCompleted = 2
};


//! Host interface: model update controller.
//! As with all host interfaces, the function pointers in this struct must remain valid until
//! all document controllers on the plug-in side that use it have been destroyed.
typedef struct ARAModelUpdateControllerInterface
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Message to the host signaling analysis progress, value ranges from 0.0f to 1.0f.
    //! The first message must be marked with kARAAnalysisProgressStarted, the last
    //! with kARAAnalysisProgressCompleted.
    //! This notification is intended solely for displaying a progress indication if desired, but
    //! not to trigger content reading for updating content information. That is instead done when
    //! receiving notifyAudioSourceContentChanged(), see below.
    //! Note that since the updates are polled by the host, any analysis may already have progressed
    //! somewhat by the time the start notification is actually delivered, so it may start with a
    //! progress larger than 0. It is even possible that an analysis fully completes before its
    //! start is notified, in which case the plug-in may choose not notify it at all.
    //! If the plug-in internally executes multiple analysis task per audio source simultaneously
    //! (for example because it splits them by content type), it must merge all internal progress
    //! into a single outer progress.
    void (ARA_CALL *notifyAudioSourceAnalysisProgress) (ARAModelUpdateControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef,
                                                        ARAAnalysisProgressState state, float value);

    //! Message to the host when content of the given audio source changes.
    //! Not to be called if the change was explicitly triggered by the host, e.g. when restoring
    //! audio source state or if the host calls ARADocumentControllerInterface::updateAudioSourceContent().
    //! When restoring state, plug-ins shall only make this call if the object state after loading is
    //! not equal to the archived state. This can happen e.g when unarchiving encounters errors, or
    //! when state is imported from older versions and converted to some updated model state.
    //! The time range may be NULL, this means that the entire audio source is affected.
    //! Note that this notification is what hosts will listen to when determining whether the state
    //! returned by isAudioSourceContentAnalysisIncomplete() has changed. In other words, the
    //! completion state of the analysis itself is considered to be a part of the overall content.
    //! Thus, if it changes, the overall content has changed, even if neither the grade nor the data
    //! for content readers did change (e.g. because an analysis failed to provide proper results).
    //! Plug-ins must send this notification reliably to avoid data loss when hosts rely on it in
    //! order to optimize saving ARA data only when it has actually changed.
    void (ARA_CALL *notifyAudioSourceContentChanged) (ARAModelUpdateControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef,
                                                      const ARAContentTimeRange * range, ARAContentUpdateFlags flags);

    //! Message to the host when content of the given audio modification changes.
    //! Not to be called if the change was explicitly triggered by the host, e.g. when restoring
    //! audio modification state.
    //! When restoring state, plug-ins will only make this call if the object state after loading is
    //! not equal to the archived state. This can happen e.g when unarchiving encounters errors, or
    //! when state is imported from older versions and converted to some updated model state.
    //! The time range may be NULL, this means that the entire audio modification is affected.
    //! Whenever this notification is received, all playback regions for the given modification
    //! are affected as well if their range in modification time intersects with the given time range.
    //! Because of this, there was no separate change notification for playback region in ARA 1.
    //! This changed with the introduction of content-based fades in ARA 2.0, because playback region
    //! content can now change without any audio modification changes - hosts should now use playback
    //! region readers to visualize content in their arrangement, see below.
    //! Plug-ins must send this notification reliably to avoid data loss when hosts rely on it in
    //! order to optimize saving ARA data only when it has actually changed.
    void (ARA_CALL *notifyAudioModificationContentChanged) (ARAModelUpdateControllerHostRef controllerHostRef, ARAAudioModificationHostRef audioModificationHostRef,
                                                            const ARAContentTimeRange * range, ARAContentUpdateFlags flags);

    //! Message to the host when content of the given playback region changes (added in ARA 2.0).
    //! In ARA 1, region content updates were implicitly derived from audio modification updates,
    //! (see notifyAudioModificationContentChanged()), based on the assumption that the content
    //! of a playback region did depend only on the modification content and the transformation
    //! described in the playback region properties.
    //! Since ARA 2.0 however, plug-ins use the grouping provided by the region sequences to
    //! derive further parameters for the transformation, which enables them to perform content
    //! based fades or other custom adjustments. This means the content of any given region can
    //! change even if its modification and region properties remain unchanged, and that the host
    //! can no longer calculate the content of a region based on the content of its modification.
    //! Note that when kARAContentUpdateSignalScopeRemainsUnchanged is not set, this message also
    //! indicates that the host needs to update any cached copies of the rendered signal it may hold,
    //! and needs to query getPlaybackRegionHeadAndTailTime() for potential updates.
    //! The time range is specified in playback time and potentially covers head and tail time too.
    //! The time range may be NULL, this means that the entire playback region (including its
    //! head and tail time, which may be updated at the same time) is affected.
    ARA_ADDENDUM(2_0_Draft) void (ARA_CALL *notifyPlaybackRegionContentChanged) (ARAModelUpdateControllerHostRef controllerHostRef, ARAPlaybackRegionHostRef playbackRegionHostRef,
                                                                                 const ARAContentTimeRange * range, ARAContentUpdateFlags flags);

    //! Message to the host when private, opaque document state that is not associated with any audio
    //! source or modification changes (added in ARA 2.3).
    //! Saving/restoring this data is controlled via ARAStoreObjectsFilter::documentData and
    //! ARARestoreObjectsFilter::documentData, see there.
    //! Plug-ins must send this notification reliably to avoid data loss when hosts rely on it in
    //! order to optimize saving ARA data only when it has actually changed.
    ARA_ADDENDUM(2_3_Final) void (ARA_CALL *notifyDocumentDataChanged) (ARAModelUpdateControllerHostRef controllerHostRef);
} ARAModelUpdateControllerInterface;

// Convenience constant for easy struct validation.
enum { kARAModelUpdateControllerInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAModelUpdateControllerInterface, notifyAudioModificationContentChanged) };

//! @}


/***************************************************************************************************/
//! @defgroup Host_Playback_Controller_Interface Playback Controller Interface
//! This optional host interface allows the plug-in to request playback state changes.
//! The functions in this interface may be called concurrently, but not from render-threads.
//! The host may choose to ignore any of these requests.
//! The requests will typically be scheduled and executed with some delay.
//! The current state of playback is transmitted via the companion API.
//! \br
//! Host developer using C++ ARA Library can implement the ARA::Host::PlaybackControllerInterface.
//! For plug-in developers this interface is wrapped by the ARA::PlugIn::HostPlaybackController.
//! @{

//! Reference to the host side representation of a playback controller (opaque to the plug-in).
typedef ARA_HOST_REF(ARAPlaybackControllerHostRef);

//! Host interface: playback controller.
//! As with all host interfaces, the function pointers in this struct must remain valid until
//! all document controllers on the plug-in side that use it have been destroyed.
typedef struct ARAPlaybackControllerInterface
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Message to the host to start playback of our document.
    void (ARA_CALL *requestStartPlayback) (ARAPlaybackControllerHostRef controllerHostRef);

    //! Message to the host to stop playback of our document.
    void (ARA_CALL *requestStopPlayback) (ARAPlaybackControllerHostRef controllerHostRef);

    //! Message to the host to set the playback position of our document.
    //! Note that this may be called both when playing back or when stopped.
    void (ARA_CALL *requestSetPlaybackPosition) (ARAPlaybackControllerHostRef controllerHostRef,
                                                 ARATimePosition timePosition);

    //! Message to the host to set the playback cycle range of our document.
    void (ARA_CALL *requestSetCycleRange) (ARAPlaybackControllerHostRef controllerHostRef,
                                           ARATimePosition startTime, ARATimeDuration duration);

    //! Message to the host to enable or disable the playback cycle of our document.
    void (ARA_CALL *requestEnableCycle) (ARAPlaybackControllerHostRef controllerHostRef, ARABool enable);
} ARAPlaybackControllerInterface;

// Convenience constant for easy struct validation.
enum { kARAPlaybackControllerInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlaybackControllerInterface, requestEnableCycle) };

//! @}


/***************************************************************************************************/
//! @defgroup Host_Document_Controller_Instance Document Controller Instance
//! The callbacks into the host are published by the host when creating a document controller on
//! the plug-in side to maintain an ARA model graph. The instance struct and all interfaces and
//! host refs therein must remain valid until the document controller is destroyed.
//! The host can choose to create its controller objects per document controller instance, or it
//! can share a single instance between all document controllers, whatever fits its needs.
//! It may even mix-and-match both approaches per individual interface.
//! @{

//! The document controller host instance struct and all interfaces and refs therein must remain valid
//! until all plug-in document controllers created with this struct have been destroyed by the host.
typedef struct ARADocumentControllerHostInstance
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name AudioAccessControllerInterface
//! Must always be supported.
//@{
    ARAAudioAccessControllerHostRef audioAccessControllerHostRef;
    const ARAAudioAccessControllerInterface * audioAccessControllerInterface;
//@}

//! @name ArchiveControllerInterface
//! Must always be supported.
//@{
    ARAArchivingControllerHostRef archivingControllerHostRef;
    const ARAArchivingControllerInterface * archivingControllerInterface;
//@}

//! @name ContentAccessControllerInterface
//! OPTIONAL INTERFACE: plug-in must check contentAccessControllerInterface is not NULL before calling!
//@{
    ARAContentAccessControllerHostRef contentAccessControllerHostRef;
    const ARAContentAccessControllerInterface * contentAccessControllerInterface;
//@}

//! @name ModelUpdateControllerInterface
//! OPTIONAL INTERFACE: plug-in must check modelUpdateControllerInterface is not NULL before calling!
//@{
    ARAModelUpdateControllerHostRef modelUpdateControllerHostRef;
    const ARAModelUpdateControllerInterface * modelUpdateControllerInterface;
//@}

//! @name PlaybackControllerInterface
//! OPTIONAL INTERFACE: plug-in must check playbackControllerInterface is not NULL before calling!
//@{
    ARAPlaybackControllerHostRef playbackControllerHostRef;
    const ARAPlaybackControllerInterface * playbackControllerInterface;
//@}
} ARADocumentControllerHostInstance;

// Convenience constant for easy struct validation.
enum { kARADocumentControllerHostInstanceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARADocumentControllerHostInstance, playbackControllerInterface) };

//! @}

//! @}



/***************************************************************************************************/
#if defined(__clang__)
#pragma mark Plug-In side controller interfaces and factory
#endif

//! @defgroup Plug_In_Interfaces Plug-In Interfaces
//! @{
/***************************************************************************************************/


// forward-declaration, defined below
struct ARAFactory;
typedef struct ARAFactory ARAFactory;


//! @defgroup Partial_Document_Persistency Partial Document Persistency
//! \br
//! These optional filters allow to only store a subset of the document graph into an archive,
//! or only restore a subset of an archive into the document graph.
//! @{

//! Optional filter when restoring objects.
//! \br
//! Allows the host to specify a subset of the persistent objects in the archive to restore in
//! ARADocumentControllerInterface::restoreObjectsFromArchive().
//! \br
//! The given IDs refer to objects in the archive, but can optionally be mapped to those used in the
//! current document. This may be necessary to resolve potential conflicts between persistent IDs
//! from different documents when importing parts of one document into another (since persistent IDs
//! are only required to be unique within a document, not across documents).
//! \br
//! The C++ ARA Library offers plug-in developers the ARA::PlugIn::RestoreObjectsFilter
//! utility class to ease the implementation of partial persistency.
ARA_ADDENDUM(2_0_Final) typedef struct ARARestoreObjectsFilter
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Flag to indicate whether the plug-in should include its private, opaque document state
    //! in the archive - see ARAStoreObjectsFilter::documentData for details.
    ARABool documentData;

    //! Length of #audioSourceArchiveIDs and #audioSourceCurrentIDs (if provided).
    ARASize audioSourceIDsCount;

    //! Variable-sized C array listing the persistent IDs of the archived audio sources to restore.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    const ARAPersistentID * audioSourceArchiveIDs;

    //! Optional variable-sized C array mapping each of the persistent IDs provided in audioSourceArchiveIDs
    //! to a potentially different persistent ID currently used for the audio sources to be restore
    //! in the current graph.
    //! If no mapping is desired, i.e. all audio source persistent IDs to restore match those in
    //! the current graph, the pointer should be NULL.
    const ARAPersistentID * audioSourceCurrentIDs;

    //! Length of #audioModificationArchiveIDs and #audioModificationCurrentIDs (if provided).
    ARASize audioModificationIDsCount;

    //! Variable-sized C array listing the persistent IDs of the archived audio modifications to restore.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    const ARAPersistentID * audioModificationArchiveIDs;

    //! Optional variable-sized C array mapping each of the persistent IDs provided in audioModificationArchiveIDs
    //! to a potentially different persistent ID currently used for the audio modifications to be restore
    //! in the current graph.
    //! If no mapping is desired, i.e. all audio modification persistent IDs to restore match those in
    //! the current graph, the pointer should be NULL.
    const ARAPersistentID * audioModificationCurrentIDs;
} ARARestoreObjectsFilter;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Final) { kARARestoreObjectsFilterMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARARestoreObjectsFilter, audioModificationCurrentIDs) };


//! Optional filter when storing objects.
//! \br
//! Allows the host to specify a subset of the objects in the model graph to be stored in
//! ARADocumentControllerInterface::storeObjectsToArchive().
//! \br
//! The C++ ARA Library offers plug-in developers the ARA::PlugIn::StoreObjectsFilter
//! utility class to ease the implementation of partial persistency.
ARA_ADDENDUM(2_0_Final) typedef struct ARAStoreObjectsFilter
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Flag to indicate whether the plug-in should include its private, opaque document state
    //! in the archive.
    //! A typical example of private data is a fallback implementation in the plug-in for data not
    //! provided by the host. For example, if the host does not implement a chord track, a plug-in
    //! may need to implement this in its own UI in order to be fully usable. Since the host is not
    //! aware of this, the data must be stored privately at document level.
    //! This flag should be set to kARAFalse if the archive is intended for copy/paste or other
    //! means of data import/export between documents, or kARATrue if a host uses partial
    //! persistency as a general technique to store ARA documents for performance reasons (e.g.
    //! to avoid re-saving data that hasn't been changed, or to minimize sync activity when
    //! implementing collaborative editing across the network - note that such optimizations rely
    //! on proper update notifications by the plug-in).
    //! If implementing the latter, hosts will typically split the document into a partial archive
    //! that contains only the document data, plus a set of archives that each contain a single
    //! audio source. In such a setup, the audio modifications are either stored in the same archive
    //! as their underlying audio source, or each audio modification is separated into an archive
    //! of its own.
    //! The only restriction for such archive splicing (in addition to respecting the general data
    //! dependency rules for partial persistency outlined in the documentation of
    //! ARADocumentControllerInterface::restoreObjectsFromArchive()) is that when restoring, the
    //! partial archive which was saved with documentData == kARATrue is restored as last archive in
    //! the restore cycle, where the graph has its final structure and all object states are available.
    ARABool documentData;

    //! Length of #audioSourceRefs.
    ARASize audioSourceRefsCount;

    //! Variable-sized C array listing the audio sources to store.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    const ARAAudioSourceRef * audioSourceRefs;

    //! Length of #audioModificationRefs.
    ARASize audioModificationRefsCount;

    //! Variable-sized C array listing the audio modifications to store.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    const ARAAudioModificationRef * audioModificationRefs;
} ARAStoreObjectsFilter;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Final) { kARAStoreObjectsFilterMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAStoreObjectsFilter, audioModificationRefs) };
//! @}


//! @defgroup Processing_Algorithm_Selection Processing Algorithm Selection
//! @{

//! Processing algorithm description returned by ARADocumentControllerInterface::getProcessingAlgorithmProperties()
//! Provides a unique identifier and a user-readable name of the algorithm, as displayed in the plug-in.
//! The pointers contained in this struct must remain valid until the document controller that has
//! provided the struct is destroyed.
ARA_ADDENDUM(2_0_Final) typedef struct ARAProcessingAlgorithmProperties
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! ID for this particular processing algorithm.
    ARAPersistentID persistentID;

    //! Name as displayed by the plug-in (may be localized).
    ARAUtf8String name;
} ARAProcessingAlgorithmProperties;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Final) { kARAProcessingAlgorithmPropertiesMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAProcessingAlgorithmProperties, name) };

//! @}


/***************************************************************************************************/
//! @defgroup Plug-In_Document_Controller Document Controller
//! ARA model objects are created and managed by the ARA Document Controller provided by the plug-in.
//! The host uses the factory and management functions of the Document Controller to create a partial
//! copy of its model translated into the ARA world.
//! It is created using a factory which can be either retrieved by scanning the plug-in binaries
//! for a dedicated factory or by requesting it from a living companion plug-in instance.
//! The host is responsible for keeping the document controller alive as long as any objects created
//! through it are still living (kind of implicit ref-counting). This means that its live time is
//! independent of the companion plug-in instances - they may all be gone at some point but the
//! ARA graph may still be accessed through its document controller.
//! The host must also keep the document controller alive as long as any companion plug-in instance
//! which it has bound to it is actively used. The actual destruction of the plug-in instance may
//! be done later (to ease reference counting implementation), but rendering the plug-in, accessing
//! its state or showing its UI is only valid as long as the ARA document controller it has been
//! bound is still alive.
//! Except for some rare, explicitly documented functions like getPlaybackRegionHeadAndTailTime(),
//! the document controller interface must always be called from the same thread - usually hosts
//! will manage their internal model as well as the attached ARA graph from the application's main
//! thread, triggered from the main run loop. If a host decides to use a different thread for
//! maintaining the ARA model, it may need to implement some sort of locking so that its updates on
//! the ARA model thread do not interfere concurrently with the main run loop's event processing
//! as it drives the plug-in's UI code and notification system.
//! \br
//! Plug-in developers using C++ ARA Library can implement the ARA::PlugIn::DocumentControllerInterface,
//! or extend the already implemented ARA::PlugIn::DocumentController class as needed.
//! For host developers this interface is wrapped by the ARA::Host::DocumentController.
//! @{

//! Reference to the plug-in side representation of a document controller (opaque to the host).
typedef ARA_REF(ARADocumentControllerRef);

//! Plug-in interface: document controller.
//! The function pointers in this struct must remain valid until the document controller is
//! destroyed by the host.
typedef struct ARADocumentControllerInterface
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Destruction
//@{
    //! Destroy the controller and its associated document.
    //! The host must delete all objects associated with the document graph (audio sources,
    //! musical contexts etc.) before making this call.
    //! Note that the objects exported via the ARAPlugInExtensionInstance are not considered
    //! part of the document graph, their destruction may happen before or after destroying the
    //! document controller that they are bound to.
    void (ARA_CALL *destroyDocumentController) (ARADocumentControllerRef controllerRef);
//@}

//! @name Link back to the factory
//@{
    //! Query the static ARA factory that was used to create this controller.
    //! This provides a convenient traversal to the name of the plug-in, the description of its
    //! capabilities, its archive IDs etc.
    const ARAFactory * (ARA_CALL *getFactory) (ARADocumentControllerRef controllerRef);
//@}

//! @name Update management
//@{
    //! Start an editing session on a document.
    //! An editing session can contain an arbitrary set of modifications that belong together.
    //! Since many model edits can result in rather expensive updates on the plug-in side, this call
    //! allows for grouping the edits and postponing the updates until the new model state is final,
    //! which potentially saves some intermediate updates.
    void (ARA_CALL *beginEditing) (ARADocumentControllerRef controllerRef);

    //! End an editing session on a document.
    //! Note that when receiving this call, the plug-in will update any amount of internal state.
    //! These edits may lead to update notifications to the host, and the host may in turn read
    //! affected content from the plug-in and update its own model accordingly.
    //! One example for this the way that Melodyne maintains chords and scales associated with
    //! audio modifications. It copies this data from the musical context into the audio
    //! modifications, so that when editing regions the notes appear in the proper pitch grid.
    //! If moving playback regions in the song, these copies may need to be updated, and Melodyne
    //! will report the resulting audio modification content changes to the host.
    //! To ensure that any such follow-up updates are added to the same undo cycle, hosts that
    //! actively read plug-in content data should immediately (i.e. within the same undo frame)
    //! call notifyModelUpdates() after making this call.
    void (ARA_CALL *endEditing) (ARADocumentControllerRef controllerRef);

    //! Tell the plug-in to send all pending update notifications for the given document.
    //! This must be called periodically by the host whenever not editing nor restoring the document.
    //! Only when processing this call, the plug-in may call back into the host using
    //! ARAModelUpdateControllerInterface.
    //! Hosts must be aware after receiving beginEditing(), plug-ins may choose to postpone any subset
    //! of their internal state updates until the matching call to endEditing(). This means that if
    //! the host for some reason needs to wait for a specific update in the plug-in to occur
    //! (such as waiting for an analysis to finish) it must do so outside of pairs of beginEditing()
    //! and endEditing().
    void (ARA_CALL *notifyModelUpdates) (ARADocumentControllerRef controllerRef);
//@}

//! @name Document Persistency (extended in ARA 2.0)
//! This has been updated with ARA 2.0, see below.
//@{
    //! Begin an unarchiving session of the document and its associated objects.
    //! \deprecated
    //! Since version 2_0_Final this call has been superseded by the combination of beginEditing()
    //! and restoreObjectsFromArchive(). This allows for optional filtering, but also simplifies
    //! both host and plug-in implementation.
    ARA_DEPRECATED(2_0_Final) ARABool (ARA_CALL *beginRestoringDocumentFromArchive) (ARADocumentControllerRef controllerRef, ARAArchiveReaderHostRef archiveReaderHostRef);

    //! End an unarchiving session of the document and its associated objects.
    //! \deprecated
    //! Since version 2_0_Final this call has been superseded by the combination of endEditing()
    //! and restoreObjectsFromArchive(). This allows for optional filtering, but also simplifies
    //! both host and plug-in implementation.
    //! \br
    //! When using API generation 1 or older and using this call, the host must pass the same
    //! archiveReaderHostRef as used for beginRestoringDocumentFromArchive().
    //! This way, plug-ins can choose to evaluate the archive upon beginRestoring() or endRestoring(),
    //! or even upon both calls if needed.
    ARA_DEPRECATED(2_0_Final) ARABool (ARA_CALL *endRestoringDocumentFromArchive) (ARADocumentControllerRef controllerRef, ARAArchiveReaderHostRef archiveReaderHostRef);

    //! Create an archive of the internal state of a given document and all its associated objects.
    //! \deprecated
    //! Since version 2_0_Final this call has been superseded by storeObjectsToArchive(),
    //! which allows for optional filtering, but is otherwise identical.
    ARA_DEPRECATED(2_0_Final) ARABool (ARA_CALL *storeDocumentToArchive) (ARADocumentControllerRef controllerRef, ARAArchiveWriterHostRef archiveWriterHostRef);
//@}

//! @name Document Management
//! As with all model graph edits, calling these functions must be guarded by beginEditing() and endEditing().
//@{
    //! Update the properties of the controller's document.
    //! All properties must be specified, the plug-in will determine which have actually changed.
    void (ARA_CALL *updateDocumentProperties) (ARADocumentControllerRef controllerRef, const ARADocumentProperties * properties);
//@}

//! @name Musical Context Management
//! As with all model graph edits, calling these functions must be guarded by beginEditing() and endEditing().
//@{
    //! Create a new musical context associated with the controller's document.
    ARAMusicalContextRef (ARA_CALL *createMusicalContext) (ARADocumentControllerRef controllerRef, ARAMusicalContextHostRef hostRef,
                                                           const ARAMusicalContextProperties * properties);

    //! Update the properties of a given musical context.
    //! All properties must be specified, the plug-in will determine which have actually changed.
    void (ARA_CALL *updateMusicalContextProperties) (ARADocumentControllerRef controllerRef, ARAMusicalContextRef musicalContextRef,
                                                     const ARAMusicalContextProperties * properties);

    //! Tell the plug-in to update the information obtainable via content readers for a given musical context.
    //! The time range may be NULL, this means that the entire musical content is affected.
    //! Creating a new musical context implies an initial content update for it - the host will
    //! call this explicitly only for later content updates.
    void (ARA_CALL *updateMusicalContextContent) (ARADocumentControllerRef controllerRef, ARAMusicalContextRef musicalContextRef,
                                                  const ARAContentTimeRange * range, ARAContentUpdateFlags flags);

    //! Destroy a given musical context.
    //! Destroying a musical context also implies removing it from its document.
    //! The musical context must no longer be referred to by any playback region when making this call.
    void (ARA_CALL *destroyMusicalContext) (ARADocumentControllerRef controllerRef, ARAMusicalContextRef musicalContextRef);
//@}

//! @name Audio Source Management
//! As with all model graph edits, calling these functions must be guarded by beginEditing() and endEditing(),
//! with the exception of enableAudioSourceSamplesAccess() (which is no model graph edit, but
//! but still has to occur on the ARA model thread).
//@{
    //! Create a new audio source associated with the controller's document.
    //! The newly created audio source has its sample data access initially disabled,
    //! an explicit call to enableAudioSourceSamplesAccess() is needed.
    ARAAudioSourceRef (ARA_CALL *createAudioSource) (ARADocumentControllerRef controllerRef, ARAAudioSourceHostRef hostRef,
                                                     const ARAAudioSourceProperties * properties);

    //! Update the properties of a given audio source.
    //! Depending on which properties are changed (see documentation of ARAAudioSourceProperties),
    //! the host may not be able to make this call while the plug-in is reading data -
    //! in that case, use enableAudioSourceSamplesAccess() accordingly.
    //! All properties must be specified, the plug-in will determine which have actually changed.
    void (ARA_CALL *updateAudioSourceProperties) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef,
                                                  const ARAAudioSourceProperties * properties);

    //! Tell the plug-in that the sample data or content information for the given audio source has changed.
    //! Not to be called in response to ARAModelUpdateControllerInterface::notifyAudioSourceContentChanged().
    //! The time range may be NULL, this means that the entire audio source is affected.
    //! When implementing this call, remember to also flush any caches of the sampled data if needed
    //! (see ::kARAContentUpdateSignalScopeRemainsUnchanged).
    //! Creating a new audio source always implies an initial content update for it, i.e. the host
    //! will call this function only for later content updates. Since audio sources are persistent,
    //! plug-ins should preferably postpone the initial content reading until endEditing() -
    //! if the host is restoring the audio source, this will remove the need to read initial content.
    void (ARA_CALL *updateAudioSourceContent) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef,
                                               const ARAContentTimeRange * range, ARAContentUpdateFlags flags);

    //! Enable or disable access to a given audio source.
    //! This call allows the host to control the time when the plug-in may access sample data from
    //! the given audio source. Disabling access forces the plug-in to destroy all audio readers
    //! it currently has created for the affected audio source. This is a synchronous call,
    //! blocking until all currently executing reads of the audio source are finished.
    //! Access is disabled by default, hosts must explicitly enable it after creating an audio source.
    //! Since this call does not modify the model graph, it may be called outside the usual
    //! beginEditing() and endEditing() scope.
    //! Note that disabling access will also abort any analysis currently being executed for the
    //! audio source, making it necessary to start it from scratch when the access is enabled again.
    //! This means that enableAudioSourceSamplesAccess() is an expensive call that only should be
    //! made when necessary. It should not be (ab-)used to simply "pause ARA" whenever convenient.
    void (ARA_CALL *enableAudioSourceSamplesAccess) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef, ARABool enable);

    //! Deactivate the given audio source because it has become part of the undo history
    //! and is no longer used actively.
    //! The plug-in will cancel any pending analysis for this audio source and may free memory
    //! that is only needed when the audio source can be edited or rendered.
    //! Before deactivating an audio source, the host must deactivate all associated audio
    //! modifications, and the opposite order is required when re-activating upon redo.
    //! When deactivated, updating the properties or content of the audio source or reading its
    //! content is no longer valid.
    //! Like properties, deactivation is not necessarily persistent in the plug-in, so the host must
    //! call this explicitly when restoring deactivated audio sources.
    //! Note that with the introduction of partial persistency with ARA 2.0, hosts likely will prefer
    //! to simply create partial archives of deleted audio sources and manage these in their undo
    //! history rather than utilizing this call.
    void (ARA_CALL *deactivateAudioSourceForUndoHistory) (ARADocumentControllerRef controllerRef,
                                                          ARAAudioSourceRef audioSourceRef, ARABool deactivate);

    //! Destroy a given audio source.
    //! Destroying an audio source also implies removing it from its document.
    //! The host must delete all objects associated with the audio source (audio modifications etc.)
    //! before deleting the audio source.
    //! The host does not need to explicitly disable access to the audio source before making this call.
    void (ARA_CALL *destroyAudioSource) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef);
//@}

//! @name Audio Modification Management
//! As with all model graph edits, calling these functions must be guarded by beginEditing() and endEditing().
//@{
    //! Create a new audio modification associated with the given audio source.
    ARAAudioModificationRef (ARA_CALL *createAudioModification) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef,
                                                                 ARAAudioModificationHostRef hostRef, const ARAAudioModificationProperties * properties);

    //! Create a new audio modification which copies the state of another given audio modification.
    //! The new modification will be associated with the same audio source.
    //! This call is used to create independent variations of the audio edits as opposed to creating
    //! aliases by merely adding playback regions to a given audio modification.
    //! Note that with the introduction of partial persistency with ARA 2.0, hosts can achieve the
    //! same effect by creating an archive of the modification that should be cloned and unarchiving
    //! that state into a new modification.
    ARAAudioModificationRef (ARA_CALL *cloneAudioModification) (ARADocumentControllerRef controllerRef, ARAAudioModificationRef audioModificationRef,
                                                                ARAAudioModificationHostRef hostRef, const ARAAudioModificationProperties * properties);

    //! Update the properties of a given audio modification.
    //! All properties must be specified, the plug-in will determine which have actually changed.
    void (ARA_CALL *updateAudioModificationProperties) (ARADocumentControllerRef controllerRef, ARAAudioModificationRef audioModificationRef,
                                                        const ARAAudioModificationProperties * properties);

    //! Deactivate the given audio modification because it has become part of the undo history
    //! and is no longer used actively.
    //! The plug-in may free some memory that is only needed when the audio modification can be
    //! edited or rendered.
    //! Before deactivating an audio modification, the host must destroy all associated playback
    //! regions, and the opposite order is required when re-activating upon redo.
    //! When deactivated, updating the properties of the audio modification or reading its
    //! content is no longer valid.
    //! Like properties, deactivation is not necessarily persistent in the plug-in, so the host must
    //! call this explicitly when restoring deactivated audio modifications.
    //! Note that with the introduction of partial persistency with ARA 2.0, hosts likely will prefer
    //! to simply create partial archives of deleted audio modifications and manage these in their undo
    //! history rather than utilizing this call.
    void (ARA_CALL *deactivateAudioModificationForUndoHistory) (ARADocumentControllerRef controllerRef,
                                                                ARAAudioModificationRef audioModificationRef, ARABool deactivate);

    //! Destroy a given audio modification.
    //! Destroying an audio modification also implies removing it from its audio source.
    //! The host must delete all objects associated with the audio modification (playback regions etc.)
    //! before deleting the audio modification.
    void (ARA_CALL *destroyAudioModification)(ARADocumentControllerRef controllerRef, ARAAudioModificationRef audioModificationRef);
//@}

//! @name Playback Region Management
//! As with all model graph edits, calling these functions must be guarded by beginEditing() and endEditing().
//@{
    //! Create a new playback region associated with the given audio modification.
    ARAPlaybackRegionRef (ARA_CALL *createPlaybackRegion) (ARADocumentControllerRef controllerRef, ARAAudioModificationRef audioModificationRef,
                                                           ARAPlaybackRegionHostRef hostRef, const ARAPlaybackRegionProperties * properties);

    //! Update the properties of a given playback region.
    //! All properties must be specified, the plug-in will determine which have actually changed.
    void (ARA_CALL *updatePlaybackRegionProperties) (ARADocumentControllerRef controllerRef, ARAPlaybackRegionRef playbackRegionRef,
                                                     const ARAPlaybackRegionProperties * properties);

    //! Destroy a given playback region.
    //! Destroying a playback region also implies removing it from its audio modification.
    //! The playback region must no longer be referred to by any plug-in extension when making this call.
    void (ARA_CALL *destroyPlaybackRegion) (ARADocumentControllerRef controllerRef, ARAPlaybackRegionRef playbackRegionRef);
//@}

//! @name Content Reader Management
//! Content readers are not model objects but rather auxiliary objects to parse content without
//! extensive data copying, following a standard iterator pattern.
//! As is common with iterators, no changes may be made to the model graph while reading content.
//! Prior to the final release of ARA 2.0, this meant that making any of the content related calls
//! was limited to be done outside of pairs of beginEditing() and endEditing().
//! Since version 2_0_Final, it is also valid to use them while the document is in editing state,
//! but no other call to this document controller may be made in-between a series of content related
//! calls (except for getFactory() and getPlaybackRegionHeadAndTailTime()).
//! For example for a given audio source such a series of calls typically would be
//! isAudioSourceContentAvailable(), getAudioSourceContentGrade(), createAudioSourceContentReader(),
//! getContentReaderEventCount(), n times getContentReaderDataForEvent(), destroyContentReader().
//! Typically, content readers are temporary local objects on the stack of the ARA model thread,
//! which naturally satisfies these conditions.
//! This change is mainly intended to support partial persistency introduced with ARA 2.0, since
//! it allows hosts to read content of imported audio sources/modifications to adjust the playback
//! regions accordingly without toggling the editing state of the document back and forth.
//@{
    //! Query whether the given content type is currently available for the given audio source.
    ARABool (ARA_CALL *isAudioSourceContentAvailable) (ARADocumentControllerRef controllerRef,
                                                       ARAAudioSourceRef audioSourceRef, ARAContentType contentType);

    //! Query whether an analysis of the given content type has been done for the given audio source.
    //! This call will typically be used when the host uses the plug-in as a detection engine in the
    //! background (i.e. without presenting the UI to the user). In that scenario, the host will
    //! trigger the analysis of the desired content types using requestAudioSourceContentAnalysis()
    //! and then wait until the plug-in calls ARAModelUpdateControllerInterface::notifyAudioSourceContentChanged().
    //! From that call, the host will query the plug-in via isAudioSourceContentAnalysisIncomplete()
    //! to determine which of the analysis requests have completed.
    //! If the host did request a specific algorithm to be used, then the plug-in should return
    //! kARATrue here until the request was satisfied (or rejected).
    ARABool (ARA_CALL *isAudioSourceContentAnalysisIncomplete) (ARADocumentControllerRef controllerRef,
                                                                ARAAudioSourceRef audioSourceRef, ARAContentType contentType);

    //! Explicitly trigger a certain analysis.
    //! If the host wants to use the plug-in as detection engine, it needs to explicitly trigger the
    //! desired analysis, since otherwise the plug-in may postpone any analysis as suitable.
    //! To allow for optimizing the analysis on the plug-in side, all content types that are of
    //! interest for the host should be specified in a single call if possible.
    //! \br
    //! Note that the plug-in may choose to perform any additional analysis at any point in time
    //! if this is appropriate for its design. It will call
    //! ARAModelUpdateControllerInterface::notifyAudioSourceContentChanged() if such an analysis
    //! concludes successfully so that the host can update accordingly.
    //! \br
    //! The provided content types must be a non-empty subset of the plug-in's
    //! ARAFactory::analyzeableContentTypes. To request all analysis types exported by the plug-in,
    //! hosts can directly pass analyzeableContentTypes and -Count from the plug-in's ARAFactory.
    //! The contentTypes pointer may be only valid for the duration of the call, it must be
    //! evaluated inside the call, and the pointer must not be stored anywhere.
    //! \br
    //! Note that ARA 2.0 adds the capability for the host to also request the use of a certain
    //! processing algorithm via requestProcessingAlgorithmForAudioSource() - if both are used then
    //! the algorithm must be selected before requesting the analysis.
    void (ARA_CALL *requestAudioSourceContentAnalysis) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef,
                                                        ARASize contentTypesCount, const ARAContentType contentTypes[]);

    //! Query the current quality of the information provided for the given audio source and content type.
    ARAContentGrade (ARA_CALL *getAudioSourceContentGrade) (ARADocumentControllerRef controllerRef,
                                                            ARAAudioSourceRef audioSourceRef, ARAContentType contentType);

    //! Create a content reader for the given audio source and content type.
    //! This should only be called after availability has been confirmed using isAudioSourceContentAvailable().
    //! The time range may be NULL, which means that the entire audio source shall be read.
    //! If a time range is specified, all events that at least partially intersect with the range
    //! will be read.
    ARAContentReaderRef (ARA_CALL *createAudioSourceContentReader) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef,
                                                                    ARAContentType contentType, const ARAContentTimeRange * range);

    //! Query whether the given content type is currently available for the given audio modification.
    //! Note that since ARA 2.0, reading at playback region level is recommended for most content types,
    //! see createAudioModificationContentReader().
    ARABool (ARA_CALL *isAudioModificationContentAvailable) (ARADocumentControllerRef controllerRef,
                                                             ARAAudioModificationRef audioModificationRef, ARAContentType contentType);

    //! Query the current quality of the information provided for a given audio modification and content type.
    //! Note that since ARA 2.0, reading at playback region level is recommended for most content types,
    //! see createAudioModificationContentReader().
    ARAContentGrade (ARA_CALL *getAudioModificationContentGrade) (ARADocumentControllerRef controllerRef,
                                                                  ARAAudioModificationRef audioModificationRef, ARAContentType contentType);

    //! Create a content reader for the given audio modification and content type.
    //! This should only be called after availability has been confirmed using isAudioModificationContentAvailable().
    //! The time range may be NULL, which means that the entire audio modification shall be read.
    //! If a time range is specified, all events that at least partially intersect with the range
    //! will be read.
    //! Note that with the introduction of region transitions in ARA 2.0, the content of a given
    //! playback region can no longer be externally calculated by the host based on the content of
    //! its underlying audio modification and the transformation flags. Instead, hosts should read
    //! such content directly at region level. This particularly applies to kARAContentTypeNotes -
    //! notes at borders will be adjusted on a per-region basis when using content based fades.
    //! Playback region content reading is available in ARA 1.0 already, thus such an implementation
    //! will be fully backwards compatible.
    //! Reading content at audio modification (or audio source) level is still valid and useful if
    //! the host needs access to the content in its original state, not transformed by a playback
    //! region, e.g. when implementing features such as tempo and signature detection through ARA.
    ARAContentReaderRef (ARA_CALL *createAudioModificationContentReader) (ARADocumentControllerRef controllerRef, ARAAudioModificationRef audioModificationRef,
                                                                          ARAContentType contentType, const ARAContentTimeRange * range);

    //! Query whether the given content type is currently available for the given playback region.
    ARABool (ARA_CALL *isPlaybackRegionContentAvailable) (ARADocumentControllerRef controllerRef,
                                                          ARAPlaybackRegionRef playbackRegionRef, ARAContentType contentType);

    //! Query the current quality of the information provided for the given playback region and content type.
    ARAContentGrade (ARA_CALL *getPlaybackRegionContentGrade) (ARADocumentControllerRef controllerRef,
                                                               ARAPlaybackRegionRef playbackRegionRef, ARAContentType contentType);

    //! Create a content reader for the given playback region and content type.
    //! This should only be called after availability has been confirmed using isPlaybackRegionContentAvailable().
    //! The time range may be NULL, which means that the entire playback region shall be read,
    //! including its potential head and tail time.
    //! If a time range is specified, all events that at least partially intersect with the range
    //! will be read.
    //! The time range must be given in playback time, and the time stamps provided by the content
    //! reader are in playback time as well.
    ARAContentReaderRef (ARA_CALL *createPlaybackRegionContentReader) (ARADocumentControllerRef controllerRef, ARAPlaybackRegionRef playbackRegionRef,
                                                                       ARAContentType contentType, const ARAContentTimeRange * range);

    //! Query how many events the given reader exposes.
    ARAInt32 (ARA_CALL *getContentReaderEventCount) (ARADocumentControllerRef controllerRef, ARAContentReaderRef contentReaderRef);

    //! Query data of the given event of the given reader.
    //! The returned pointer is owned by the plug-in and must remain valid until either
    //! getContentReaderDataForEvent() is called again or the content reader is destroyed.
    const void * (ARA_CALL *getContentReaderDataForEvent) (ARADocumentControllerRef controllerRef,
                                                           ARAContentReaderRef contentReaderRef, ARAInt32 eventIndex);

    //! Destroy the given content reader.
    void (ARA_CALL *destroyContentReader) (ARADocumentControllerRef controllerRef, ARAContentReaderRef contentReaderRef);
//@}

//! @name Region Sequence Management (added in ARA 2.0)
//! As with all model graph edits, calling these functions must be guarded by beginEditing() and endEditing().
//@{
    //! Create a new region sequence associated with the controller's document.
    ARA_ADDENDUM(2_0_Draft) ARARegionSequenceRef (ARA_CALL *createRegionSequence) (ARADocumentControllerRef controllerRef, ARARegionSequenceHostRef hostRef,
                                                                                   const ARARegionSequenceProperties * properties);

    //! Update the properties of a given region sequence.
    //! All properties must be specified, the plug-in will determine which have actually changed.
    ARA_ADDENDUM(2_0_Draft) void (ARA_CALL *updateRegionSequenceProperties) (ARADocumentControllerRef controllerRef, ARARegionSequenceRef regionSequenceRef,
                                                                             const ARARegionSequenceProperties * properties);

    //! Destroy a given region sequence.
    //! The region sequence must no longer be referred to by any playback region when making this call.
    ARA_ADDENDUM(2_0_Draft) void (ARA_CALL *destroyRegionSequence) (ARADocumentControllerRef controllerRef, ARARegionSequenceRef regionSequenceRef);
//@}

//! @name Playback Region Head and Tail Time (added in ARA 2.0)
//! Depending on the specific DSP performed by the plug-in, the resulting signal for any region may
//! exceed the region borders. Among other use cases such as delays, this will typically happen when
//! the content based fades are enabled.
//! Reporting this potential excess of signal both at start and end of a region allows for the host
//! to adjust their rendering so that it includes head and tail time, or else fade the signal at the
//! region borders to avoid crackles.
//! This function can be called either from the model thread or from any (realtime or offline)
//! audio rendering thread.
//! If edits that the host performs on the model affect the head or tail times (such as toggling
//! content based fades on or off), the new values can only be reliably queried on the model thread
//! once endEditing() has returned. On audio threads, they can only be reliably queried until all
//! render calls that may have occurred concurrently with endEditing() have returned.
//@{
    //! Query the current head and tail time of a given playback region.
    //! Note that when a plug-in optimizes region transitions, the head and tail of any given
    //! region can change upon any model edit, even if it is not directly affected by the edit.
    //! Also, in order to properly track interaction between regions, plug-ins may lazily update
    //! this information upon endEditing(). Plug-ins will call notifyPlaybackRegionContentChanged()
    //! whenever these values change.
    //! headTime and tailTime must not be NULL.
    //! Host may query this often, so plug-ins should cache the value if there's any expensive
    //! calculation involved.
    //! Note that most companion APIs also feature a tail time concept. For playback renderer plug-in
    //! instances, the tail time reported via the companion API should be equal to or greater than
    //! the maximum of the tail times of all playback regions currently associated with the given
    //! renderer (i.e. the tail for any given playback region may be somewhat shorter then the
    //! companion API tail, depending on the region's content).
    ARA_ADDENDUM(2_0_Draft) void (ARA_CALL *getPlaybackRegionHeadAndTailTime) (ARADocumentControllerRef controllerRef, ARAPlaybackRegionRef playbackRegionRef,
                                                                               ARATimeDuration * headTime, ARATimeDuration * tailTime);
//@}

//! @name Document Persistency (extended in ARA 2.0)
//! Persistently storing and restoring an ARA graph requires that both the host and the plug-in data
//! are properly maintained. When the host creates an archive of its own data, it therefore needs
//! to store a matching archive of the plug-in data. The link between the host objects and the
//! plug-in objects is preserved using the ARAPersistentID defined in the properties of each
//! persistent model graph object.
//! ARA 1.0 started out with calls to store and restore the complete model graph managed by a given
//! ARADocumentController instance. While such monolithic document archives handle song persistency
//! fine, they are not suitable for more elaborate features such as copying and pasting audio source
//! and audio modification state between songs.
//! ARA 2.0 therefore adds the option to store and restore arbitrary subsections of an ARA graph.
//! Such partial archives may even be restored from ARA-specific chunks inside an audio file - see
//! \ref ARAAudioFileChunks.
//! Using partial persistency is optional on the host side, but ARA 2 plug-ins are required to fully
//! support it.
//! \br
//! A crucial point to keep in mind when archiving and unarchiving partial graphs is to honor data
//! dependencies in the overall graph. The opaque state of a persistent object can depend both on its
//! properties, such as the sample count of an audio source, and on parent objects inside the graph,
//! such as audio modification state referencing data in the underlying audio source.
//! The host must honor these dependencies, or else the plug-in may not be able to properly restore
//! the archived state, causing some degree of data loss, up to a full reset of the affected object.
//! ARA 1 style full-document persistency intrinsically complies with this rule and can be trivially
//! migrated to the ARA 2 calls. For partial archives however, some notable consequences are:
//! - Before restoring the state of any ARA object, hosts must configure the properties for that
//!   object appropriately.
//! - If a host splits restoring an ARA graph into multiple calls, each call that restores some
//!   audio source state must either include or precede restoring the state of any audio modification
//!   associated with the affected audio source.
//! - Restoring an audio modification without restoring its underlying audio source may not succeed
//!   if the audio source state has changed since storing the audio modification. Hosts can observe
//!   audio source content change notifications to track such changes (note that these notifications
//!   are sent asynchronously, so they have to be polled directly before creating the archive, from
//!   the same stack frame).
//! - When restoring the state of an audio source, the state of all audio modifications associated
//!   with the audio source must be restored too.
//! \br
//! In some use cases, partial data loss may be acceptable. One example would be to copy and paste
//! just an audio modification state from one document to another when the underlying audio source
//! has been edited in either the source or target document.
//! Depending on what the actual difference in the audio source state is, the plug-in may be able to
//! adjust the pasted audio modification to the altered audio source state and still retain a valuable
//! amount of the original data. Therefore the host is allowed to perform such operations (at the
//! risk of loosing an unspecified amount of data), and plug-ins must deal with this situation as
//! reasonable as their internal models allow them to and not threat this as an error.
//@{
    //! Unarchive the internal state of the specified objects.
    //! This call can be used both for unarchiving entire documents and for importing arbitrary
    //! objects into an existing document.
    //! An unarchiving session is conceptually identical to an editing session: after starting the
    //! session, the host rebuilds the graph using the regular object creation calls, then makes
    //! this call to let the plug-in parse the archive and inject the archived internal state into
    //! the graph as indicated by the persistentIDs of the relevant objects.
    //! Similarly, when importing objects, the host will perform an editing session and either create
    //! new objects or re-use existing objects (potentially adjusting their persistentID), then
    //! make this call to inject the imported state.
    //! \br
    //! The optional filter allows for restoring only a subset of the archived states into the graph.
    //! It can be NULL, in which case all archived states with matching persistentIDs will be restored.
    //! In that case, the call sequence beginEditing(), restoreObjectsFromArchive(), endEditing()
    //! is equivalent to the deprecated begin-/endRestoringDocumentFromArchive() for ARA 1, which
    //! has been superseded by this call.
    //! The host is not required to restore all objects in the archive. Any archived states that are
    //! either filtered explicitly, or for which there is no object with a matching persistent ID in
    //! the current graph are simply ignored.
    //! Since persistent IDs are only required to be unique per document (and not globally), hosts
    //! may encounter persistent ID conflicts when importing data from other documents.
    //! The optional ARARestoreObjectsFilter provided for this call therefore allows to map between
    //! the IDs used in the archive and those used in the current graph if needed.
    //! \br
    //! The host can make multiple calls to restoreObjectsFromArchive() within the same editing
    //! session to import objects from multiple archives in one operation. It may even decide
    //! to implement its persistency based on partial archives entirely, using several calls to
    //! storeObjectsToArchive() with varying filters to split the document into slices appropriate
    //! to its implementation, see ARAStoreObjectsFilter::documentData.
    //! \br
    //! Result is kARAFalse if the access to the archive reader failed while trying to read the
    //! archive, or if decoding the data failed, kARATrue otherwise. Potential reason for failure
    //! include data corruption due to storage hardware failures, or broken dependencies when
    //! restoring partial archives as discussed above. The plug-in should try to recover as much
    //! state as possible in all cases, and the host should notify the user of such errors.
    //! If a failure happened already while reading the archive, the host is aware of this and can
    //! augment its error message to the user accordingly. If the failure happens inside the plug-in
    //! when decoding the data, the plug-in is responsible for guiding the user as good as possible,
    //! e.g. by listing or marking the affected objects.
    //! Note that since versioning is expressed through the ARA factory, the host must deal with
    //! potential versioning conflicts before making this call, and provide proper UI too.
    ARA_ADDENDUM(2_0_Final) ARABool (ARA_CALL *restoreObjectsFromArchive) (ARADocumentControllerRef controllerRef, ARAArchiveReaderHostRef archiveReaderHostRef,
                                                                           const ARARestoreObjectsFilter * filter);

    //! Create a partial archive of the internal state of the specified objects.
    //! Archives may only be created from documents that are not being currently edited.
    //! The optional filter allows for storing only a subset of the document graph into the archive.
    //! It can be NULL, in which case all objects in the graph will be stored.
    //! In that case, the call is equivalent to the deprecated storeDocumentToArchive(),
    //! which has been superseded by this call.
    //! Result is kARAFalse if the access to the archive writer failed while trying to write the
    //! archive, kARATrue otherwise.
    //! The host is responsible for alerting the user about archive write errors,
    //! see ARAArchivingControllerInterface::writeBytesToArchive().
    //! Note that for creating ARA audio file chunk archives, storeAudioSourceToAudioFileChunk()
    //! must be used instead, so that the plug-in can pick the correct encoding and return the
    //! corresponding (compatible) document archive ID.
    ARA_ADDENDUM(2_0_Final) ARABool (ARA_CALL *storeObjectsToArchive) (ARADocumentControllerRef controllerRef, ARAArchiveWriterHostRef archiveWriterHostRef,
                                                                       const ARAStoreObjectsFilter * filter);
//@}

//! @name Processing Algorithm Selection (added in ARA 2.0)
//! Many plug-ins feature a set of different processing algorithms, each suited for a given kind of
//! audio material. When analyzing the audio, plug-ins either try to automatically determine the
//! appropriate algorithm that best matches the nature of the given material, or delegate this
//! decision to the user. This fundamental selection may affect all further processing of the
//! audio source - in addition to the analysis results, internal editing and rendering parameters
//! and even the entire UI may change. Accordingly, changing this selection may invalidate some or
//! all edits done by the user in the audio modifications based on the audio source. To avoid such
//! data loss, making this selection appropriately should always be the first step when working with
//! audio sources.
//! By exporting the list of processing algorithms here, hosts can implement this selection,
//! based on their knowledge about the origin of the audio material.
//! Some hosts allow for configuring their built-in detection in a very similar way on a per-track
//! basis, usually via a menu. They can use this technique for compatible ARA plug-ins as well.
//! Host might also be able to implicitly deduce an appropriate processing algorithm. For example,
//! when adding a new audio source to a region sequence that already refers to a set of previously
//! analyzed audio sources which all use the same detection algorithm, this algorithm is likely
//! appropriate for the new audio source as well.
//! Along the same lines, when recording a new take of some material that already has been analyzed
//! by the plug-in, it is reasonable to use the same algorithm for the new take.
//! Note that each processing algorithm must implement detection for all content types exported by
//! the plug-in, albeit the detection quality may likely be different for some types.
//! Plug-ins should keep the list of available processing algorithms restricted to the smallest
//! reasonable set - after all, in an ideal (but currently not achievable) implementation, a
//! single detection algorithm would be powerful enough to cover all types of material flawlessly.
//@{
    //! Return the count of processing algorithms provided by the plug-in.
    //! If this optional method is not implemented or the call returns 0, then the plug-in does not support
    //! algorithm selection through the host and the other related functions below must not be called.
    ARA_ADDENDUM(2_0_Final) ARAInt32 (ARA_CALL *getProcessingAlgorithmsCount) (ARADocumentControllerRef controllerRef);

    //! List of processing algorithms provided by the plug-in, described by their properties.
    //! This method must be implemented if getProcessingAlgorithmsCount() is implemented.
    //! Provides a unique identifier and a user-readable name of the algorithm as displayed in the plug-in.
    //! The host should present the algorithms to the user in the order of this list, e.g. in a menu.
    //! For a given version of the plug-in, the count and the order and values of the persistentIDs
    //! must be the same, while the names may depend on localization settings that can be different
    //! on different machines or between individual runs of the host.
    //! The list may however change between different versions of the plug-in.
    //! Both hosts and plug-ins must implement fallbacks for loading a document that contains an
    //! processing algorithm persistentID which is no longer supported by the plug-in.
    //! The pointers returned by this calls must remain valid until the document controller
    //! is destroyed.
    ARA_ADDENDUM(2_0_Final) const ARAProcessingAlgorithmProperties * (ARA_CALL *getProcessingAlgorithmProperties) (ARADocumentControllerRef controllerRef,
                                                                                                                   ARAInt32 algorithmIndex);

    //! Query currently used processing algorithm for a given audio source.
    //! This method must be implemented if getProcessingAlgorithmsCount() is implemented.
    //! After the plug-in has concluded an analysis (as indicated via
    //! ARAModelUpdateControllerInterface::notifyAudioSourceContentChanged()),
    //! the host can query which processing algorithm was used and update its UI accordingly.
    //! This is particularly relevant if the host did explicitly request an analysis with a
    //! specific algorithm, but the plug-in was unable to satisfy this request for some reason.
    //! Similarly, the user may have changed the algorithm through the plug-in's UI.
    //! Note that until the first analysis has completed (i.e. as long as
    //! isAudioSourceContentAnalysisIncomplete() returns kARATrue), the value returned here may be
    //! an abstract default, not related to the actual audio source content. This will e.g. typically
    //! be the case in Melodyne, which uses an "automatic mode" as default until the first analysis
    //! has determined the actual processing algorithm that is suitable for the material.
    //! The call should not be made while isAudioSourceContentAnalysisIncomplete() returns kARATrue,
    //! because the returned value is likely stale.
    ARA_ADDENDUM(2_0_Final) ARAInt32 (ARA_CALL *getProcessingAlgorithmForAudioSource) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef);

    //! Request that any future analysis of the given audio source should use the given processing algorithm.
    //! This method must be implemented if getProcessingAlgorithmsCount() is implemented.
    //! This both affects any analysis requested by the host via requestAudioSourceContentAnalysis()
    //! as well as any analysis done by the plug-in on demand.
    //! Since this typically results in a model graph edit, calling this functions must be guarded
    //! by beginEditing() and endEditing().
    //! Note that the plug-in is not required to heed this request if its internal state suggest
    //! otherwise, or if the user switches actively to a different algorithm.
    //! Also, some algorithms may be "meta" algorithms that will be replaced by a different actual
    //! algorithm, such as the "automatic" default algorithm in Melodyne which will pick an
    //! appropriate algorithm from the remaining list of algorithms when doing the initial analysis.
    ARA_ADDENDUM(2_0_Final) void (ARA_CALL *requestProcessingAlgorithmForAudioSource) (ARADocumentControllerRef controllerRef, ARAAudioSourceRef audioSourceRef,
                                                                                       ARAInt32 algorithmIndex);
//@}

//! @name License Management (added in ARA 2.0)
//! If using ARA plug-ins for regular editing, their UI is shown and they can implement all license
//! handling fully transparent for the host within their regular UI. If however the host is using the
//! plug-in as internal engine for analysis (e.g. audio-to-MIDI conversion) or for time-stretching,
//! it will typically not open the plug-in UI, and accordingly must then deal with potentially missing
//! licenses that the plug-in requires to perform the desired tasks.
//! As an example, Melodyne's analysis capabilities are only available when running Melodyne essential
//! or higher, i.e. they are not supported by an unlicensed "playback-only" installation.
//@{
    //! With this optional call, hosts can test whether the current license state of the plug-in allows
    //! for requesting analysis of the given content types and rendering the given playback transformations
    //! (see requestAudioSourceContentAnalysis() and ARAPlaybackRegionProperties::transformationFlags).
    //! The host can also optionally instruct the plug-in to run a modal licensing dialog if the current
    //! license is not sufficient to perform the selected engine tasks, so that the user can review and
    //! adjust the licensing accordingly, such downloading a license from their respective user account
    //! or even purchase an upgrade that enables the requested features.
    //! \br
    //! The provided content types must be a subset of the plug-in's ARAFactory::analyzeableContentTypes.
    //! To request all analysis types exported by the plug-in, hosts can directly pass
    //! analyzeableContentTypes and -Count from the ARAFactory.
    //! The contentTypes pointer may be only valid for the duration of the call, it must be evaluated
    //! inside the call, and the pointer must not be stored anywhere.
    //! If not intending to use analysis, the count should be 0 and the array pointer NULL.
    //! The transformationFlags must be a subset of the plug-in's ARAFactory::supportedPlaybackTransformationFlags,
    //! and may be kARAPlaybackTransformationNoChanges if not intending to use transformations.
    //! The call returns kARATrue if the (potentially updated) license is sufficient to perform the
    //! requested tasks.
    ARA_ADDENDUM(2_0_Final) ARABool (ARA_CALL *isLicensedForCapabilities) (ARADocumentControllerRef controllerRef,
                                                                           ARABool runModalActivationDialogIfNeeded,
                                                                           ARASize contentTypesCount, const ARAContentType contentTypes[],
                                                                           ARAPlaybackTransformationFlags transformationFlags);
//@}

//! @name Document Persistency (extended in ARA 2.0)
//@{
    //! Create an archive of the internal state of the specified audio source suitable to be
    //! embedded into the underlying audio file as ARA audio file chunks, see @ref ARAAudioFileChunks.
    //! Hosts must check ARAFactory::supportsStoringAudioFileChunks before enabling users to store
    //! audio file chunks for the given plug-in.
    //! Archives may only be created from documents that are not being currently edited.
    //! \br
    //! This call differs from using storeObjectsToArchive() with an ARAStoreObjectsFilter in that
    //! the plug-in may choose a different internal encoding more suitable for this use case,
    //! indicated by returning a \p documentArchiveID that is likely one of the
    //! ARAFactory::compatibleDocumentArchiveIDs rather than the ARAFactory::documentArchiveID.
    //! The plug-in also returns whether openAutomatically should be set in the audio file chunk.
    //! Result is kARAFalse if the access to the archive writer failed while trying to write the
    //! archive, kARATrue otherwise.
    //! The host is responsible for alerting the user about archive write errors,
    //! see ARAArchivingControllerInterface::writeBytesToArchive().
    ARA_ADDENDUM(2_0_Final) ARABool (ARA_CALL *storeAudioSourceToAudioFileChunk) (ARADocumentControllerRef controllerRef, ARAArchiveWriterHostRef archiveWriterHostRef,
                                                                                  ARAAudioSourceRef audioSourceRef, ARAPersistentID * documentArchiveID, ARABool * openAutomatically);
//@}
//! @name Audio Modification Management
//@{
    //! Some hosts such as Pro Tools provide indicators whether a given plug-in's current
    //! settings cause it to alter the sound of the original audio source, or preserve it so that
    //! bypassing/removing the plug-in would not change the perceived audible result (note that
    //! actual rendering involves using a playback region, which still may apply time-stretching
    //! or pitch-shifting to the audio modification's potentially unaltered output).
    //! \br
    //! Changes to this state are tracked via ARAModelUpdateControllerInterface::notifyAudioModificationContentChanged()
    //! with ::kARAContentUpdateSignalScopeRemainsUnchanged == false. Note that it is possible to
    //! perform other edits such as reassigning the chords associated with the audio modification
    //! which would not affect this state.
    //! \br
    //! It is valid for plug-in implementations to deliver false negatives here to reasonably
    //! limit the cost of maintaining the state. For example, if the plug-in does some
    //! threshold-based processing, but the signal happens to never actually reach the threshold,
    //! the plug-in still may report to alter the sound.
    //! Another example is pitch&time editing in a Melodyne-like plug-in: if notes are moved to a
    //! different pitch or time position so that this flag is cleared, but later the user manually
    //! moves them back to the original location, this might not cause this flag to turn back on.
    //! If however the user invokes undo, or some explicit reset command instead of the manual
    //! adjustment, then the plug-in should maintain this state properly.
    ARA_ADDENDUM(2_0_Final) ARABool (ARA_CALL *isAudioModificationPreservingAudioSourceSignal) (ARADocumentControllerRef controllerRef, ARAAudioModificationRef audioModificationRef);
//@}
} ARADocumentControllerInterface;

// Convenience constant for easy struct validation.
enum { kARADocumentControllerInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARADocumentControllerInterface, destroyContentReader) };

//! @}


/***************************************************************************************************/
//! @defgroup Plug-In_Document_Controller_Instance Document Controller Instance
//! The callbacks into the plug-in are published by the plug-in when the host requests the creation
//! of a document controller via the factory. The instance struct and all interfaces and
//! host refs therein must remain valid until the document controller is destroyed.
//! The plug-in can choose to create its controller objects per document controller instance, or it
//! can share a single instance between all document controllers, whatever fits its needs.
//! It may even mix-and-match both approaches per individual interface.
//! @{

//! The document controller instance struct and all interfaces and refs therein must remain valid
//! until the document controller is destroyed by the host.
typedef struct ARADocumentControllerInstance
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Document Controller Interface
//@{
    ARADocumentControllerRef documentControllerRef;
    const ARADocumentControllerInterface * documentControllerInterface;
//@}
} ARADocumentControllerInstance;

// Convenience constant for easy struct validation.
enum { kARADocumentControllerInstanceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARADocumentControllerInstance, documentControllerInterface) };

//! @}


/***************************************************************************************************/
//! @defgroup Plug-In_Factory Plug-In Factory
//! Static entry into ARA, allows to create ARA objects.
//! @{

//! API configuration.
//! This configuration struct allows for setting the desired API version, the debug callback etc.
//! Note that a pointer to this struct is only valid for the duration of the call to
//! initializeARAWithConfiguration() - the data must be fully evaluated/copied inside the call.
typedef struct ARAInterfaceConfiguration
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Defines the API generation to use, must be within the range of supported generations.
    ARAAPIGeneration desiredApiGeneration;

    //! Pointer to the global assert function address.
    //! Be aware that this is a pointer to a function pointer, not the function pointer itself!
    //! This indirection is necessary so that plug-ins can inject their debug code if needed.
    //! The pointer must be the same for all instances that use the same API generation.
    //! It must be always provided by the host, but can point to NULL to suppress debugging in
    //! release versions. It must remain valid until uninitializeARA() is called.
    ARAAssertFunction * assertFunctionAddress;
} ARAInterfaceConfiguration;

// Convenience constant for easy struct validation.
enum { kARAInterfaceConfigurationMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAInterfaceConfiguration, assertFunctionAddress) };


//! Static plug-in factory.
//! All pointers herein must remain valid as long as the binary is loaded.
//! The declaration of this struct will not change when updating to a new generation of the API,
//! only additions are possible.
/*typedef*/ struct ARAFactory   // the typedef was forward-declared above
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Factory and Global Initialization
//! The following data members and functions are involved when loading/unloading the plug-in binary.
//! \br
//! All function calls in this header may only be made between a single call to
//! initializeARAWithConfiguration() and another single call to uninitializeARA().
//! Typically, initialization/uninitialization is done right after loading/right before unloading
//! the binary, but it is valid to call them multiple times without loading/unloading the binary
//! in-between, provided the described order of calls is respected.
//@{
    ARAAPIGeneration lowestSupportedApiGeneration;  //!< Lower bound of supported ARAAPIGeneration.
    ARAAPIGeneration highestSupportedApiGeneration; //!< Upper bound of supported ARAAPIGeneration.

    //! Unique and versioned plug-in identifier.
    //! This ID must be globally unique and identifies the plug-in's document controller class
    //! created by this factory at runtime.
    //! The ID includes versioning, it must be updated if e.g. the plug-in's (compatible) document
    //! archive ID(s) or its analysis or playback transformation capabilities change.
    //! Host applications can therefore use it to trigger cache updates if they implement a plug-in
    //! caching mechanism to avoid scanning all plug-ins each time the program is launched.
    //! If a given plug-in supports multiple companion APIs, it will return the same ID across all
    //! companion APIs, allowing the host to choose which API to use for this particular plug-in.
    //! See @ref sec_ManagingARAArchives for more information.
    ARAPersistentID factoryID;

    //! Start up ARA with the given configuration.
    void (ARA_CALL * initializeARAWithConfiguration) (const ARAInterfaceConfiguration * config);
    //! Shut down ARA.
    void (ARA_CALL * uninitializeARA) (void);
//@}

//! @name User-Presentable Meta Information
//! This information is only intended for display purposes and should never be used for flow control.
//! In addition to all other potential uses, this information should always be stored alongside any
//! document archive that the host creates in order to be able to provide a proper error message if
//! the archive is being restored on a different system where the plug-in is not installed (or only
//! an older version of it).
//! See @ref sec_ManagingARAArchives for more information.
//@{
    //! Name of the plug-in to display to the user.
    ARAUtf8String plugInName;

    //! Name of the manufacturer of the plug-in to display to the user.
    ARAUtf8String manufacturerName;

    //! Web page to refer the user to if they need further information about the plug-in.
    ARAUtf8String informationURL;

    //! Version string of the plug-in to display to the user.
    ARAUtf8String version;
//@}

//! @name Document Controller and Document Archives
//! This section of the struct deals with creating document controllers and handling versioned
//! document archiving based on archive IDs.
//! Note that in addition to the actual document archive and its ID, the host should save the above
//! meta information about the plug-in alongside so it can do proper error handling, see above.
//@{
    //! Factory function for the document controller.
    //! This call is used both when creating a new document from scratch and when restoring
    //! a document from an archive.
    const ARADocumentControllerInstance * (ARA_CALL *createDocumentControllerWithDocument) (const ARADocumentControllerHostInstance * hostInstance,
                                                                                            const ARADocumentProperties * properties);

    //! Identifier for document archives created by the document controller.
    //! This ID must be globally unique and is shared only amongst document controllers that
    //! create the same archives and produce the same render results based upon the same input data.
    //! This means that the ID must be updated if the archive format changes in any way that is
    //! no longer downwards compatible.
    //! See @ref sec_ManagingARAArchives for more information.
    ARAPersistentID documentArchiveID;

    //! Length of compatibleDocumentArchiveIDs.
    ARASize compatibleDocumentArchiveIDsCount;

    //! Variable-sized C array listing other identifiers of archives that the document controller can import.
    //! This list is used for example when updating the data structures inside the controller.
    //! It is ordered descending, expressing a preference which state to load in case a document
    //! contains more than one compatible archive of older versions.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    const ARAPersistentID * compatibleDocumentArchiveIDs;
//@}

//! @name Capabilities
//! If the host wants to utilize the plug-in as an internal engine for analysis or time-stretching,
//! it can use this information to determine if the plug-in is actually capable of handling its needs.
//! Further, the host may want to restrict the user from doing operations that make no sense for the
//! given plug-in, such as trying to time-stretch with a plug-in that cannot perform this task.
//! Note that even if a plug-in cannot analyze a given content type, it still may provide a UI where
//! the user can edit that content type, so it may support content reading for types missing here.
//@{
    //! Length of analyzeableContentTypes.
    ARASize analyzeableContentTypesCount;

    //! Variable-sized C array listing the content types for which the plug-in can perform an analysis.
    //! This list allows the host to determine whether the plug-in can be used as analysis engine in
    //! the background (i.e. without presenting the UI to the user) to achieve certain host features.
    //! Note that the plug-in may support more content types than listed here for some or all objects
    //! in the graph, but may rely on user interaction when dealing with those content types.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    const ARAContentType * analyzeableContentTypes;

    //! Set of transformations that the plug-in supports when configuring playback regions.
    //! These flags allow the host to determine whether e.g. the plug-in can be used as time-stretch engine.
    ARAPlaybackTransformationFlags supportedPlaybackTransformationFlags;

    //! Flag whether the plug-in supports exporting ARA audio file chunks via
    //! ARADocumentControllerInterface::storeAudioSourceToAudioFileChunk().
    //! Note that reading such chunks is unaffected by this flag - as long as the documentArchiveID
    //! in the chunk is compatible, the plug-in must be able to read the data via
    //! ARADocumentControllerInterface::restoreObjectsFromArchive().
    ARA_ADDENDUM(2_0_Final) ARABool supportsStoringAudioFileChunks;
//@}
} /*ARAFactory*/;

// Convenience constant for easy struct validation.
enum { kARAFactoryMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAFactory, supportedPlaybackTransformationFlags) };

//! @}


/***************************************************************************************************/
//! @defgroup Plug-In_Extension Plug-In Extension
//! The plug-in extension provides ARA-specific additional functionality per companion plug-in instance.
//! On a conceptual level, the plug-in extension is not an object of its own, but merely a set of
//! additional interfaces of the companion plug-in instance, augmenting it with a few ARA-specific
//! features in a fashion that is independent from the actual companion API in use.
//! Accordingly, it is coupled 1:1 to the companion plug-in instance and its lifetime matches the
//! lifetime of the companion plug-in instance (no separate destruction function needed).
//! Along the same lines, plug-in extensions themselves are not persistent.
//! \br
//! The plug-in extension is exposed towards the host when it binds a plug-in instance as created by the
//! companion APIs to a specific ARA model graph, represented by its associated document controller.
//! This setup call is executed via a vendor-specific extension of the companion API and may only be
//! made once. It shifts the "normal" companion plug-in into the ARA world, and once established, this
//! coupling cannot be undone, it remains active until the plug-in instance is destroyed.
//! \br
//! Note that both performing the explicit binding and the implicit unbinding upon destruction will
//! likely need to access plug-in internal data structures shared with with the document controller
//! implementation. To avoid adding costly thread safety measures when maintaining this shared state,
//! hosts should always perform these operations from the document controller thread (typically the
//! main thread). This restriction may or may not apply when using the same companion API without ARA,
//! so host developers might need to add extra precaution for the ARA case.
//! \br
//! When ARA is enabled, the renderer behavior has slightly different semantics compared to the
//! non-ARA use case. Since ARA renderers are essentially generators that use non-realtime data to
//! generate realtime signals, they do not use the realtime input signal for processing.
//! Playback renderers will simply ignore their inputs, but editor renderers will always add their
//! output signal to the input signal provided by the host. If a plug-in assumes both rendering
//! roles, playback rendering will already ignore the inputs, so the editor rendering will directly
//! add to the playback output, not to the input.
//! \br
//! Since ARA 2.0, the host can explicitly establish the roles that the given instance will assume
//! in its specific implementation upon binding the plug-in instance to the ARA document controller.
//! Each role is associated with a dedicated feature set that only is available when the particular
//! role has been established.
//! Depending on the chosen roles, the following calls control which playback regions are to be
//! rendered according to which rule.
//! Separating roles allows for more flexible ARA integrations and optimizes resource usage.
//! A host could for example use a playback renderer plug-in instance playback region, plus
//! one plug-in instance per track for editor rendering and viewing all regions on that track.
//! Amongst other behavior, the roles heavily affect the relationship between plug-in instances
//! and playback regions.
//! For rendering, each plug-in extension can handle multiple playback regions if desired, albeit
//! the semantics for modifying the set of associated regions per renderer are somewhat different
//! between playback and editor renderers, see below.
//! For editor view purposes, the relationship is not explicit to accommodate for a very broad range
//! of user interface concepts that need to interact with the API. Generally, each editor view is
//! associated with all playback regions in the document controller to which the plug-in is bound.
//! However, typically only a varying subset of those regions will be shown at any point in time,
//! depending on the intrinsic feature set of the plug-in, and reflecting the selection that the
//! user has performed in the host - see notifySelection().
//! @{

//! Plug-in instance role flags.
typedef ARA_32_BIT_ENUM(ARAPlugInInstanceRoleFlags)
{
    //! Role: playback render.
    //! Plug-in instances fulfilling this role are performing playback rendering, both for realtime
    //! song playback or for offline or realtime bounces/exports.
    //! Playback render plug-ins can be responsible for more than one playback region at a time,
    //! although hosts may prefer to use a 1:1 relationship to simplify their implementation and
    //! easily allow for further per-region processing in the host (fades, region gain).
    //! When dealing with looped regions and using the content based fades, a trivial optimization
    //! of the above pattern is to use the same renderer instance for all consecutive repetitions
    //! of the loop, which still allows for properly applying region gain and fades at the start or
    //! end of the consecutive repetitions.
    //! Another viable approach would be to use as many playback renderer instances per track as
    //! there are concurrently sounding regions on the track (i.e. including head and tail time),
    //! and distribute the regions in a round-robin fashion across those renderers so that in each
    //! renderer the regions never overlap. This way, the host could extract a separate signal per
    //! region with a minimal count of playback renderers.
    //! Note that there may be several playback renderers per playback region, for example if a host
    //! executes an export as background tasks that run concurrently with realtime playback.
    //! A playback render plug-in will replace its inputs with the rendered signal. If it does not
    //! also have editor rendering responsibilities, it does not need to be rendered while stopped,
    //! and during playback only needs to be rendered  for the range covered by the region, plus its
    //! head and tail time.
    //! Playback renderers are transient, the host does not need to store the state of these
    //! instances via the companion API (unless they also fulfill kARAEditorViewRole or other
    //! persistent roles).
    kARAPlaybackRendererRole = 1 << 0,

    //! Role: editor render.
    //! Plug-in instances fulfilling this role are performing auxiliary realtime rendering
    //! that is only used to support the editing process, such as metronome clicks or playing
    //! the pitch of a note while it's being dragged to a different pitch.
    //! There should only be one plug-in instance that handles editor rendering for any given
    //! playback region. Since it is up to the plug-in to decide when it will generate previewing
    //! output based on the editing, it needs to be permanently rendered by the host.
    //! If a editor rendering plug-in is not responsible for playback, it will always forward its
    //! input signal to the output, adding its preview signal as needed. Otherwise it will add the
    //! signal to the playback render output described above.
    //! Editor renderers are transient, the host does not need to store the state of these
    //! instances via the companion API (unless they also fulfill kARAEditorViewRole or other
    //! persistent roles).
    kARAEditorRendererRole = 1 << 1,

    //! Role: editor view.
    //! Plug-in instances fulfilling this role can be used to display a GUI.
    //! Unlike rendering for playback or editing, the view role is not tied to individual
    //! playback regions or region sequences, but rather to all regions and sequences within the
    //! document controller to which the given plug-in instance is bound.
    //! Selection and hiding of host entities is communicated to editor view plug-ins so that
    //! they can dynamically show a proper subset of the full document graph.
    //! Editor view plug-ins may contain some state related to user interface configuration, and
    //! thus their state needs to be persistent via the companion API.
    kARAEditorViewRole = 1 << 2
};


/***************************************************************************************************/
//! @defgroup Playback_Renderer_Interface Playback Renderer Interface (Added In ARA 2.0)
//! See ::kARAPlaybackRendererRole.
//! \br
//! Plug-in developers using C++ ARA Library can implement the ARA::PlugIn::PlaybackRendererInterface,
//! or extend the already implemented ARA::PlugIn::PlaybackRenderer class as needed.
//! For host developers this interface is wrapped by the ARA::Host::PlaybackRenderer.
//! @{

//! Reference to the plug-in side representation of a playback renderer (opaque to the host).
ARA_ADDENDUM(2_0_Draft) typedef ARA_REF(ARAPlaybackRendererRef);

//! Plug-in interface: playback renderer.
//! The function pointers in this struct must remain valid until the companion API plug-in instance
//! (and accordingly its plug-in extension) is destroyed by the host.
ARA_ADDENDUM(2_0_Draft) typedef struct ARAPlaybackRendererInterface
{
//! @name Size-based struct versioning
//! See \ref Versioned_structs "Versioned structs".
//@{
    ARASize structSize;
//@}

//! @name Assigning the playback region(s) for playback rendering
//! Specify the playback region(s) for which the this plug-in instance provides playback rendering
//! (and, if using ARA1 which does not distinguish roles: also editor rendering and view).
//! These calls must only be made when the plug-in is not in render-state (aka "not active" in VST3
//! and CLAP, "not initialized"/"render resources not allocated" in Audio Unit v2/v3), but the host
//! may display the UI for the plug-in while making these calls.
//! Some companion APIs such AAX do not provide an explicit API to communicate the render state to
//! the plug-in. In that case, the host needs to make sure it does not concurrently render the plug-in
//! while changing playback regions for a playback renderer, and the plug-in can accordingly toggle
//! its internal render state back and forth as needed while these calls are executed.
//! The calls can be made both inside or outside any editing or restoration cycles of the associated
//! ARA document controller, but doing them between ARADocumentControllerInterface::beginEditing()
//! and ARADocumentControllerInterface::endEditing() when feasible may yield better performance
//! since all updates can be calculated together.
//! When called outside beginEditing() and endEditing(), the changes may not be audible until the
//! next render call that starts after the plug-in has returned from the respective call.
//! The calls shall be made on the same thread that also hosts the document controller.
//! The host is not required to remove the playback regions before destroying the plug-in.
//! If multiple regions are set, they will all be sounding concurrently in case they overlap.
//! @anchor Assigning_ARAPlaybackRendererInterface_Regions
//@{
    void (ARA_CALL *addPlaybackRegion) (ARAPlaybackRendererRef playbackRendererRef, ARAPlaybackRegionRef playbackRegionRef);
    void (ARA_CALL *removePlaybackRegion) (ARAPlaybackRendererRef playbackRendererRef, ARAPlaybackRegionRef playbackRegionRef);
//@}
} ARAPlaybackRendererInterface;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Draft) { kARAPlaybackRendererInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlaybackRendererInterface, removePlaybackRegion) };

//! @}


/***************************************************************************************************/
//! @defgroup Editor_Renderer_Interface Editor Renderer Interface (Added In ARA 2.0)
//! See ::kARAEditorRendererRole.
//! \br
//! Plug-in developers using C++ ARA Library can implement the ARA::PlugIn::EditorRendererInterface,
//! or extend the already implemented ARA::PlugIn::EditorRenderer class as needed.
//! For host developers this interface is wrapped by the ARA::Host::EditorRenderer.
//! @{

//! Reference to the plug-in side representation of a editor renderer (opaque to the host).
ARA_ADDENDUM(2_0_Draft) typedef ARA_REF(ARAEditorRendererRef);

//! Plug-in interface: editor renderer.
//! The function pointers in this struct must remain valid until the companion API plug-in instance
//! (and accordingly its plug-in extension) is destroyed by the host.
ARA_ADDENDUM(2_0_Draft) typedef struct ARAEditorRendererInterface
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Assigning the playback region(s) for preview while editing
//! Specify the set of playback region(s) for which the this plug-in instance provides previewing.
//! Hosts can choose to either maintain individual regions, or conveniently maintain regions by
//! specifying region sequences as a shortcut for all regions on those sequences.
//! Behavior is identical in both cases, but hosts should not mix both types of call on a single
//! plug-in instance.
//! Preview should be unambiguous, i.e. there should be only one editor renderer plug-in instance
//! associated with each playback region. During transitions however (such as moving a region
//! from one track to another), this rule may be disregarded temporarily.
//! The host can make these calls while the plug-in is in render-state ("active" in VST3 speak,
//! "initialized" in Audio Unit speak). Plug-ins must implement a proper bridging to the
//! concurrent render threads.
//! Further, the host may display the UI for the plug-in while making these calls.
//! They can be made both inside or outside any editing or restoration cycles of the associated
//! ARA document controller, but doing them between ARADocumentControllerInterface::beginEditing()
//! ARADocumentControllerInterface::endEditing() when feasible may yield better performance since
//! all updates can be calculated together.
//! The calls shall be made on the same thread that also hosts the document controller.
//! The host is not required to remove any or all playback regions or region sequences before
//! destroying the plug-in.
//! If a plug-in does not implement any audio preview features, it can safely ignore these calls
//! and provide empty implementations.
//! @anchor Assigning_ARAEditorRendererInterface_Regions
//@{
    void (ARA_CALL *addPlaybackRegion) (ARAEditorRendererRef editorRendererRef, ARAPlaybackRegionRef playbackRegionRef);
    void (ARA_CALL *removePlaybackRegion) (ARAEditorRendererRef editorRendererRef, ARAPlaybackRegionRef playbackRegionRef);

    void (ARA_CALL *addRegionSequence) (ARAEditorRendererRef editorRendererRef, ARARegionSequenceRef regionSequenceRef);
    void (ARA_CALL *removeRegionSequence) (ARAEditorRendererRef editorRendererRef, ARARegionSequenceRef regionSequenceRef);
//@}
} ARAEditorRendererInterface;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Draft) { kARAEditorRendererInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAEditorRendererInterface, removeRegionSequence) };

//! @}


/***************************************************************************************************/
//! @defgroup Editor_View_Interface Editor View Interface (Added In ARA 2.0)
//! See ::kARAEditorViewRole.
//! \br
//! Users will often reconfigure the plug-in view through scrolling, zooming, navigating lists of
//! entities, etc. to select the subset of ARA entities that they currently need to view or edit.
//! Those selection features can be implemented in the plug-in (which was the only available
//! solution in ARA 1). However, the host applications already have established user workflows for
//! selecting their representations of the ARA objects. Making those workflows available to the
//! plug-ins is leads to a much more consistent, streamlined user experience.
//! Since the views are implemented through the companion API, there is no matching ARA entity yet.
//! Instead, the companion plug-in instance is used as a controller for its associated view.
//! (Note that while some companion APIs allow for multiple views of a given plug-in used at the
//! same time, this is not recommended when using ARA editor views.)
//! These calls only affect views, not the audio rendering.
//! They only should be made while the plug-in is showing its UI, or before entering this
//! state (i.e. during GUI setup phase), in order to optimize resource usage. Accordingly, the
//! host should send an update of the selection when (re-)opening an ARA plug-in view.
//! These calls also may be made while changes are being made to the model graph (i.e. inside of pairs
//! of ARADocumentControllerInterface::beginEditing() and ARADocumentControllerInterface::endEditing()).
//! \br
//! Plug-in developers using C++ ARA Library can implement the ARA::PlugIn::EditorViewInterface,
//! or extend the already implemented ARA::PlugIn::EditorView class as needed.
//! For host developers this interface is wrapped by the ARA::Host::EditorView.
//! @{

//! Host generated ARA view selection.
typedef struct ARAViewSelection
{
    //! @see_Versioned_Structs
    ARASize structSize;

    //! Length of #playbackRegionRefs.
    ARASize playbackRegionRefsCount;

    //! Variable-sized C array listing the explicitly selected playback regions.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    //! If the plug-in requires a playback region selections in its view, it can derive an implicit
    //! playback region selection from the region sequence selection and the time range in that case.
    const ARAPlaybackRegionRef * playbackRegionRefs;

    //! Length of #regionSequenceRefs.
    ARASize regionSequenceRefsCount;

    //! Variable-sized C array listing the explicitly selected region sequences.
    //! The list may be empty, in which case count should be 0 and the pointer NULL.
    //! If the plug-in requires a region sequence selections in its view, it can derive an implicit
    //! region sequence selection from the playback region selection in that case.
    const ARARegionSequenceRef * regionSequenceRefs;

    //! Pointer to the explicitly selected time range.
    //! This pointer can be NULL to indicate that selection command does not include an explicit
    //! time range selection.
    //! If the plug-in requires a time range selection in its view, it can derive an implicit
    //! time range selection from the playback region selection, or if that isn't provided either
    //! it can evaluate the list of all playback regions of the selected region sequences.
    const ARAContentTimeRange * timeRange;
} ARAViewSelection;

// Convenience constant for easy struct validation.
enum { kARAViewSelectionMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAViewSelection, timeRange) };

//! Reference to the plug-in side representation of a editor view (opaque to the host).
ARA_ADDENDUM(2_0_Draft) typedef ARA_REF(ARAEditorViewRef);

//! Plug-in interface: view controller.
//! The function pointers in this struct must remain valid until the document controller is
//! destroyed by the host.
ARA_ADDENDUM(2_0_Draft) typedef struct ARAEditorViewInterface
{
    //! @see_Versioned_Structs_
    ARASize structSize;

//! @name Host UI notifications
//@{
    //! Apply the given host selection to all associated views.
    //! This ARA 2.0 addition allows hosts to translate the effects of their selection implementation
    //! to a selection of ARA objects that can be interpreted by the plug-in.
    //! The command is not strict a setter for the given selection state, it rather notifies the
    //! plug-in about relevant user interaction in the host so that it can adopt in whatever way
    //! provides the best user experience within the context of the given plug-in design.
    //! A plug-in may e.g. implement scrolling to relevant positions, select the playback regions in
    //! its inspector(s), or filter the provided objects further depending on user settings.
    //! The plug-in also remains free to modify its internal selection at any time through its
    //! build-in UI behavior.
    //! For example, a plug-in may design its UI around individual regions being edited, or around
    //! entire region sequences. Melodyne features both modes, switchable by the user. When editing
    //! an individual region, it will always pick the "most suitable" region from the selection,
    //! and ignore the other selected regions.
    //! Melodyne also implements various ways to switch the editable region sequence(s) or playback
    //! region at any time independently from the host selection, as well as a user option to
    //! temporarily ignore host selection changes to "pin" the current selection.
    //! The same "loose" coupling applies on the host side: the selection sent by the host is not
    //! necessarily equal to its actual selection, but rather the best representation of the
    //! users' intent. For example, the user may be able to select entities that are not directly
    //! represented in the ARA API, but relate to a set of playback regions in some meaningful way.
    //! In that case, those regions may be sent as selection.
    //! The selection command includes various optional entities that describe the selection on the
    //! host side, such as playback regions or region sequences. Host will only explicitly provide
    //! those objects that best describe their current selection, but plug-ins can often derive other
    //! selections from that, such as calculating a region selection based on sequence selection and
    //! time range.
    //! Most hosts offer both an object-based selection which typically centers around selected
    //! arrange events (playback regions) and tracks (region sequences) versus a time-range based
    //! selection that is independent of the arrange events but typically also includes track selection.
    //! Both modes shall be distinguished by providing a time range only in the latter case.
    //! Some hosts even allow to select multiple time ranges, this should be expressed by sending
    //! their union range across the API.
    //! For an object-based selection, the time range remains NULL, and plug-ins can calculate an
    //! implicit time range from the selected playback regions if desired.
    //! Arrange event selection may linked to track selection in a variety of ways. For example,
    //! selecting a track may select its events (often filter by playback cycle range), and selecting
    //! an event may select its associated track as well. The ARA selection must be always updated
    //! to reflect any such linking accordingly.
    //! In each object list, objects are ordered by importance, with the most relevant, "focused"
    //! object being first. So if a plug-in supports only a single region selection, it should use
    //! the first region. If it can only show a single region sequence at a time, it should use the
    //! first selected sequence, or if no sequences are included in the selection fall back to the
    //! sequence of the first selected region, etc.
    //! Each new selection call describes a full selection, i.e. replaces the previous selection.
    //! An empty selection is valid and should be communicated by the host, since some plug-ins may
    //! need to evaluate this in special cases.
    //! Being selected does not affect the life time of the objects - selected objects may be deleted
    //! without updating the selection status first.
    //! Note that a pointer to this struct and all pointers contained therein are only valid for the
    //! duration of the current call receiving the pointer - the data must be evaluated/copied inside
    //! the call, and the pointers must not be stored anywhere.
    void (ARA_CALL *notifySelection) (ARAEditorViewRef editorViewRef, const ARAViewSelection * selection);

    //! Reflect hiding of region sequences in all associated views.
    //! Some hosts offer the option to hide arrange tracks from view, so that they are no longer
    //! visible, while still being played back regularly. This can be communicated to the plug-in
    //! so that it follows suite.
    //! Each call implicitly unhides all previously hidden region sequences, so calling this with
    //! an empty list makes all sequences visible.
    //! The regionSequenceRefs pointer is only valid for the duration of the call, it must be evaluated
    //! inside the call, and the pointer must not be stored anywhere.
    //! It should be NULL if regionSequenceRefsCount is 0.
    void (ARA_CALL *notifyHideRegionSequences) (ARAEditorViewRef editorViewRef,
                                                ARASize regionSequenceRefsCount, const ARARegionSequenceRef regionSequenceRefs[]);
//@}
} ARAEditorViewInterface;

// Convenience constant for easy struct validation.
enum ARA_ADDENDUM(2_0_Draft) { kARAEditorViewInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAEditorViewInterface, notifyHideRegionSequences) };

//! @}


//! @defgroup Plug-In_Extension_Interface Deprecated: Plug-In Extension Interface.
//! This interface was used before ARA 2.0 defined dedicated plug-in roles.
//! It is only to be implemented when ARA 1 backwards compatibility is desired.
//! An ARA 1 call to set/removePlaybackRegion() in this interface is equivalent
//! to calling both set/removePlaybackRegion() in ARAPlaybackRendererInterface
//! and add/removePlaybackRegion() in ARAEditorRendererInterface.
//! To some extend ARA 1 also uses this to for tasks now associated with
//! ARAEditorViewInterface: opening the UI of an ARA 1 plug-in instance is
//! interpreted as selection of the playback region set via this interface.
//! @{

ARA_DEPRECATED(2_0_Draft) typedef ARA_REF(ARAPlugInExtensionRef);
ARA_DEPRECATED(2_0_Draft) typedef struct ARAPlugInExtensionInterface
{
    ARASize structSize;
    void (ARA_CALL *setPlaybackRegion) (ARAPlugInExtensionRef plugInExtensionRef, ARAPlaybackRegionRef playbackRegionRef);
    void (ARA_CALL *removePlaybackRegion) (ARAPlugInExtensionRef plugInExtensionRef, ARAPlaybackRegionRef playbackRegionRef);
} ARAPlugInExtensionInterface;
enum ARA_DEPRECATED(2_0_Draft) { kARAPlugInExtensionInterfaceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlugInExtensionInterface, removePlaybackRegion) };

//! @}


//! The plug-in extension instance struct and all interfaces and refs therein must remain valid
//! until the companion plug-in is destroyed by the host.
//! Note that the companion plug-in destruction may happen before or after destroying the document
//! controller it has been bound to, plug-ins must handle both possible destruction orders.
//! Plug-ins must provide all interfaces that have been requested by the host through the role
//! assignment, and suppress interfaces explicitly excluded by the roles - e.g. if the host did
//! not assign kARAEditorRendererRole even it was known, editorRendererInterface will be NULL.
typedef struct ARAPlugInExtensionInstance
{
    //! @see_Versioned_Structs_
    ARASize structSize;

    ARA_DEPRECATED(2_0_Draft) ARAPlugInExtensionRef plugInExtensionRef;
    ARA_DEPRECATED(2_0_Draft) const ARAPlugInExtensionInterface * plugInExtensionInterface;

//! @name ARA2 Instance Roles
//@{
    ARAPlaybackRendererRef playbackRendererRef;
    const ARAPlaybackRendererInterface * playbackRendererInterface;

    ARAEditorRendererRef editorRendererRef;
    const ARAEditorRendererInterface * editorRendererInterface;

    ARAEditorViewRef editorViewRef;
    const ARAEditorViewInterface * editorViewInterface;
//@}
} ARAPlugInExtensionInstance;

// Convenience constant for easy struct validation.
enum { kARAPlugInExtensionInstanceMinSize = ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlugInExtensionInstance, plugInExtensionInterface) };

//! @}

//! @}


/***************************************************************************************************/
// various configurations/decorations to ensure binary compatibility

#undef ARA_32_BIT_ENUM

ARA_DISABLE_DOCUMENTATION_DEPRECATED_WARNINGS_END

#if defined(_MSC_VER) || defined(__GNUC__)
    #pragma pack(pop)
#else
    #error "struct packing and alignment not yet defined for this compiler"
#endif

#if defined(__cplusplus) && !(ARA_DOXYGEN_BUILD)
}   // extern "C"
}   // namespace ARA
#endif


#endif // ARAInterface_h
