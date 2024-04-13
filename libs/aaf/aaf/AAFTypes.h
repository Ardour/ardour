/*
 * Copyright (C) 2017-2024 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __AAFTypes_h__
#define __AAFTypes_h__

#include <stdint.h>

#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__ ((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__) __pragma (pack (push, 1)) __Declaration__ __pragma (pack (pop))
#endif

#define AAF_HEADER_BYTEORDER_LE 0x4949 // II
#define AAF_HEADER_BYTEORDER_BE 0x4D4D // MM

#define AAF_PROPERTIES_BYTEORDER_LE 0x4c          // L
#define AAF_PROPERTIES_BYTEORDER_BE 0x42          // B
#define AAF_PROPERTIES_BYTEORDER_UNSPECIFIED 0x55 // U

typedef enum aafStoredForm_e {
	SF_DATA                                   = 0x0082,
	SF_DATA_STREAM                            = 0x0042,
	SF_STRONG_OBJECT_REFERENCE                = 0x0022,
	SF_STRONG_OBJECT_REFERENCE_VECTOR         = 0x0032,
	SF_STRONG_OBJECT_REFERENCE_SET            = 0x003A,
	SF_WEAK_OBJECT_REFERENCE                  = 0x0002,
	SF_WEAK_OBJECT_REFERENCE_VECTOR           = 0x0012,
	SF_WEAK_OBJECT_REFERENCE_SET              = 0x001A,
	SF_WEAK_OBJECT_REFERENCE_STORED_OBJECT_ID = 0x0003,
	SF_UNIQUE_OBJECT_ID                       = 0x0086,
	SF_OPAQUE_STREAM                          = 0x0040

} aafStoredForm_e;

/*
typedef int32_t AAFTypeCategory_t;
typedef enum _eAAFTypeCategory_e
{
	AAFTypeCatUnknown       = 0,  // can only occur in damaged files
	AAFTypeCatInt           = 1,  // any integral type
	AAFTypeCatCharacter     = 2,  // any character type
	AAFTypeCatStrongObjRef  = 3,  // strong object reference
	AAFTypeCatWeakObjRef    = 4,  // weak object reference
	AAFTypeCatRename        = 5,  // renamed type
	AAFTypeCatEnum          = 6,  // enumerated type
	AAFTypeCatFixedArray    = 7,  // fixed-size array
	AAFTypeCatVariableArray = 8,  // variably-sized array
	AAFTypeCatSet           = 9,  // set of strong object references or
	                              // set of weak object references
	AAFTypeCatRecord        = 10, // a structured type
	AAFTypeCatStream        = 11, // potentially huge amount of data
	AAFTypeCatString        = 12, // null-terminated variably-sized
	                              // array of characters
	AAFTypeCatExtEnum       = 13, // extendible enumerated type
	AAFTypeCatIndirect      = 14, // type must be determined at runtime
	AAFTypeCatOpaque        = 15, // type can be determined at runtime
	AAFTypeCatEncrypted     = 16  // type can be determined at runtime
	                              // but bits are encrypted
} AAFTypeCategory_e;
*/

/*
 * :: Types Definition
 * see Git nevali/aaf/ref-impl/include/ref-api/AAFTypes.h
*/

typedef unsigned char aafByte_t;

typedef char* aafString_t;

typedef uint16_t aafPID_t;

typedef int64_t aafLength_t;

typedef uint8_t aafBoolean_t;

typedef int64_t aafPosition_t;

typedef uint32_t aafSlotID_t;

typedef struct _aafStream_t {
	uint64_t   size;
	aafByte_t* data;

} aafStream_t;

typedef int32_t aafJPEGTableID_t; /* for TIFF objects */

typedef struct _aafRational_t {
	int32_t numerator;
	int32_t denominator;

} aafRational_t;

typedef struct _aafDateStruct_t {
	int16_t year;  /* range -32,767 to +32767 */
	uint8_t month; /* range: 1-12, inclusive */
	uint8_t day;   /* range: 1-31, inclusive */

} aafDateStruct_t;

typedef struct _aafTimeStruct_t {
	uint8_t hour;     /* range 0-23 inclusive */
	uint8_t minute;   /* range 0-59 inclusive */
	uint8_t second;   /* range 0-59 inclusive */
	uint8_t fraction; /* range 0..99 inclusive; accuracy: .01 sec */

} aafTimeStruct_t;

typedef struct _aafTimeStamp_t {
	aafDateStruct_t date;
	aafTimeStruct_t time;

} aafTimeStamp_t;

// TODO is int32_t in the original AAFTypes.h, but does not match when parsing..
typedef int8_t aafProductReleaseType_t;

typedef enum _aafProductReleaseType_e {
	AAFVersionUnknown      = 0,
	AAFVersionReleased     = 1,
	AAFVersionDebug        = 2,
	AAFVersionPatched      = 3,
	AAFVersionBeta         = 4,
	AAFVersionPrivateBuild = 5

} aafProductReleaseType_e;

/* Version Format for ObjHeader->Version */
typedef PACK (struct _aafVersionType_t {
	int8_t major;
	int8_t minor;
}) aafVersionType_t;

/* Version Format for ObjIdentification->ProductVersion */
typedef PACK (struct _aafProductVersion_t {
	uint16_t major;
	uint16_t minor;
	uint16_t tertiary;
	uint16_t patchLevel;
	int8_t   type;
}) aafProductVersion_t;

/* aafFadeType_t: describes values for SCLP fadein and fadeout types  */
typedef int32_t aafFadeType_t;
typedef enum _aafFadeType_e {
	AAFFadeNone        = 0,
	AAFFadeLinearAmp   = 1,
	AAFFadeLinearPower = 2

} aafFadeType_e;

/* binary compatibility with GUID/CLSID and IID structures. */
typedef struct _aafUID_t {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t  Data4[8];

} aafUID_t;

#define AAFUID_PRINTED_LEN 35 // excluding NULL terminating char

static const aafUID_t AUID_NULL = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

static const aafUID_t AAFUID_NULL = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

typedef struct _aafMobID_t {
	uint8_t  SMPTELabel[12]; // 12-bytes of label prefix
	uint8_t  length;
	uint8_t  instanceHigh;
	uint8_t  instanceMid;
	uint8_t  instanceLow;
	aafUID_t material; // 16 bytes

} aafMobID_t; // 32 bytes total

static const aafMobID_t AAFMOBID_NULL = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00, 0x00, 0x00, 0x00, { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } } };

typedef struct _aafIndirect_t {
	/*
	 * byteOrder disabled for memory alignement and to avoid -Waddress-of-packed-member
	 * It is always little-endian (0x4c), or unspecified (0x55) but LE, anyway.
	 */
	// uint8_t           byteOrder; // 0x4c, 0x42, 0x55
	aafUID_t  TypeDef;
	aafByte_t Value[];

} /*__attribute__((packed))*/ aafIndirect_t;

typedef int32_t aafElectroSpatialFormulation_t;

typedef enum _aafElectroSpatialFormulation_e {
	AAFElectroSpatialFormulation_Default                                       = 0,
	AAFElectroSpatialFormulation_TwoChannelMode                                = 1,
	AAFElectroSpatialFormulation_SingleChannelMode                             = 2,
	AAFElectroSpatialFormulation_PrimarySecondaryMode                          = 3,
	AAFElectroSpatialFormulation_StereophonicMode                              = 4,
	AAFElectroSpatialFormulation_SingleChannelDoubleSamplingFrequencyMode      = 7,
	AAFElectroSpatialFormulation_StereoLeftChannelDoubleSamplingFrequencyMode  = 8,
	AAFElectroSpatialFormulation_StereoRightChannelDoubleSamplingFrequencyMode = 9,
	AAFElectroSpatialFormulation_MultiChannelMode                              = 15

} aafElectroSpatialFormulation_e;

typedef int32_t aafFrameLayout_t;

typedef enum _aafFrameLayout_e {
	AAFFullFrame      = 0,
	AAFSeparateFields = 1,
	AAFOneField       = 2,
	AAFMixedFields    = 3,
	AAFSegmentedFrame = 4

} aafFrameLayout_e;

typedef int32_t aafAlphaTransparency_t;

typedef enum _aafAlphaTransparency_e {
	AAFMinValueTransparent = 0,
	AAFMaxValueTransparent = 1

} aafAlphaTransparency_e;

typedef int32_t aafFieldNumber_t;

typedef enum _aafFieldNumber_e {
	AAFUnspecifiedField = 0,
	AAFFieldOne         = 1,
	AAFFieldTwo         = 2

} aafFieldNumber_e;

typedef int32_t aafSignalStandard_t;

typedef enum _aafSignalStandard_e {
	AAFSignalStandard_None      = 0,
	AAFSignalStandard_ITU601    = 1,
	AAFSignalStandard_ITU1358   = 2,
	AAFSignalStandard_SMPTE347M = 3,
	AAFSignalStandard_SMPTE274M = 4,
	AAFSignalStandard_SMPTE296M = 5,
	AAFSignalStandard_SMPTE349M = 6

} aafSignalStandard_e;

typedef int32_t aafContentScanningType_t;

typedef enum _aafContentScanningType_e {
	kAAFContentScanning_NotKnown    = 0,
	kAAFContentScanning_Progressive = 1,
	kAAFContentScanning_Interlace   = 2,
	kAAFContentScanning_Mixed       = 3

} aafContentScanningType_e;

typedef int32_t aafColorSiting_t;

typedef enum _aafColorSiting_e {
	AAFCoSiting      = 0,
	AAFAveraging     = 1,
	AAFThreeTap      = 2,
	AAFQuincunx      = 3,
	AAFRec601        = 4,
	AAFUnknownSiting = 255

} aafColorSiting_e;

typedef int32_t aafScanningDirection_t;

typedef enum _aafScanningDirection_e {
	AAFScanningDirection_LeftToRightTopToBottom = 0,
	AAFScanningDirection_RightToLeftTopToBottom = 1,
	AAFScanningDirection_LeftToRightBottomToTop = 2,
	AAFScanningDirection_RightToLeftBottomToTop = 3,
	AAFScanningDirection_TopToBottomLeftToRight = 4,
	AAFScanningDirection_TopToBottomRightToLeft = 5,
	AAFScanningDirection_BottomToTopLeftToRight = 6,
	AAFScanningDirection_BottomToTopRightToLeft = 7

} aafScanningDirection_e;

typedef int32_t aafFilmType_t;

typedef enum _aafFilmType_e {
	AAFFtNull = 0,
	AAFFt35MM = 1,
	AAFFt16MM = 2,
	AAFFt8MM  = 3,
	AAFFt65MM = 4

} aafFilmType_e;

typedef int32_t aafTapeCaseType_t;

typedef enum _aafTapeCaseType_e {
	AAFTapeCaseNull             = 0,
	AAFThreeFourthInchVideoTape = 1,
	AAFVHSVideoTape             = 2,
	AAF8mmVideoTape             = 3,
	AAFBetacamVideoTape         = 4,
	AAFCompactCassette          = 5,
	AAFDATCartridge             = 6,
	AAFNagraAudioTape           = 7

} aafTapeCaseType_e;

typedef int32_t aafVideoSignalType_t;

typedef enum _aafVideoSignalType_e {
	AAFVideoSignalNull = 0,
	AAFNTSCSignal      = 1,
	AAFPALSignal       = 2,
	AAFSECAMSignal     = 3

} aafVideoSignalType_e;

typedef int32_t aafTapeFormatType_t;

typedef enum _aafTapeFormatType_e {
	AAFTapeFormatNull  = 0,
	AAFBetacamFormat   = 1,
	AAFBetacamSPFormat = 2,
	AAFVHSFormat       = 3,
	AAFSVHSFormat      = 4,
	AAF8mmFormat       = 5,
	AAFHi8Format       = 6

} aafTapeFormatType_e;

typedef int32_t aafRGBAComponentKind_t;

typedef enum _aafRGBAComponentKind_e {
	AAFCompNone    = 0x30,
	AAFCompAlpha   = 0x41,
	AAFCompBlue    = 0x42,
	AAFCompFill    = 0x46,
	AAFCompGreen   = 0x47,
	AAFCompPalette = 0x50,
	AAFCompRed     = 0x52,
	AAFCompNull    = 0x00

} aafRGBAComponentKind_e;

typedef struct _aafRGBAComponent_t {
	aafRGBAComponentKind_t Code;
	uint8_t                Size;

} aafRGBAComponent_t;

//typedef aafRGBAComponent_t aafRGBALayout[8];

/**
 * This structure map the first bytes in a **properties** stream
 * node.
 *
 * This Header is followed by #_entryCount aafPropertyIndexEntry_t
 * structures, which are then followed by aafPropertyIndexHeader_t._entryCount variable
 * sized property values.
 */

typedef struct aafPropertyIndexHeader_t {
	/**
	 * The byte order of :
	 * - the remaining fields of the aafPropertyIndexHeader_t struct
	 * - the aafPropertyIndexEntry_t structs that follow
	 * - the actual property data
	 *
	 * Currently unused when parsing.
	 */

	uint8_t _byteOrder;

	/**
	 * The version number of the stored format. This allows
	 * for otherwise incompatible changes to the stored format.
	 *
	 * Currently unused when parsing.
	 */

	uint8_t _formatVersion;

	/**
	 * The number of aafPropertyIndexEntry_t structs that follow.
	 */

	uint16_t _entryCount;

} /* __attribute__((packed)) */ aafPropertyIndexHeader_t;

/**
 * This structure represents one property entry inside a
 * **properties** stream node. The actual property value
 * is located bellow all the property entries.
 *
 * The offset to the property values is calculated by :
 *
 * @code
 *
 * int offset = sizeof(aafPropertyIndexHeader_t) + (aafPropertyIndexHeader_t._entryCount * sizeof(aafPropertyIndexEntry_t))
 *
 * @endcode
 * The offset inside the property values is calculated by :
 *
 *
 * ```
 * for( PropEntry[i]; PropEntry[i] < i; i++ )
 * offset += PropEntry. _length;
 * ```
 */

typedef struct aafPropertyIndexEntry_t {
	/**
	 * The ID that describes the property.
	 *
	 * All the standard IDs can be found in AAFDefs/AAFPropertyIDs.h.
	 */

	aafPID_t _pid;

	/**
	 * Identifies the “type” of representation chosen for this
	 * property. Note that the stored form described here is not
	 * the data type of the property value, rather it is the type
	 * of external representation employed. The data type of a
	 * given property value is implied by the property ID.
	 *
	 * Can take one of the value from #aafStoredForm_e enum.
	 *
	 * Even though only 1 byte is needed, _storedForm is 2 bytes
	 * in size in order to keep each property index entry an even
	 * number of bytes in size.
	 */

	uint16_t _storedForm;

	/**
	 * The length, in bytes, of the property value in the property
	 * value stream.
	 */

	uint16_t _length;

} /* __attribute__((packed)) */ aafPropertyIndexEntry_t;

/**
 * An unordered collection of strongly referenced (contained)
 * uniquely identified objects, each of which can be :
 * - efficiently located by key - O(lg N)
 * - the target of a weak reference
 *
 * Each set index consists of an aafStrongRefSetHeader_t
 * followed by #_entryCount aafStrongRefSetEntry_t structs.
 */

typedef PACK (struct aafStrongRefSetHeader_t {
	/**
	 * The number of aafStrongRefSetEntry_t structs that follow.
	 */

	uint32_t _entryCount;

	/**
	 * The next local key that will be assigned in this set.
	 */

	uint32_t _firstFreeKey;

	/**
	 * The highest unassigned key above #_firstFreeKey. The keys
	 * between #_firstFreeKey and #_lastFreeKey are unassigned,
	 * while there may be other gaps in key assignement this
	 * represents the largest one.
	 */

	uint32_t _lastFreeKey;

	/**
	 * The property id of each aafStrongRefSetEntry_t._identification field
	 * @TODO Understand that field..
	 */

	aafPID_t _identificationPid;

	/**
	 * The length, in bytes, of each aafStrongRefSetEntry_t._identification
	 * field.
	 */

	uint8_t _identificationSize;
}) aafStrongRefSetHeader_t;

typedef struct aafStrongRefSetEntry_t {
	/**
	 * The #_localKey uniquely identifies this strong reference
	 * within this collection independently of its position
	 * within this collection. The #_localKey is used to form
	 * the name assigned to the element in this set at the
	 * corresponding ordinal position. That is, the #_localKey
	 * of the first aafStrongRefSetEntry_t is used to
	 * form the name of the first element in the set and so
	 * on. The #_localKey is an insertion key.
	*/

	uint32_t _localKey;

	/**
	 * The count of weak references to this object.
	 */

	uint32_t _referenceCount;

	/**
	 * The type of the #_identification field varies from one instance
	 * of a StrongReferenceSet to another. The value of the #_identification
	 * field uniquely identifies this object within the set. It is the
	 * search key.
	 */

	aafByte_t _identification[];

} /* __attribute__((packed)) */ aafStrongRefSetEntry_t;

/**
 * An ordered collection of strongly referenced (contained) objects.
 * Each vector index consists of an aafStrongRefVectorHeader_t
 * followed by #_entryCount aafStrongRefVectorEntry_t structs.
 */

typedef struct aafStrongRefVectorHeader_t {
	/**
	 * The number of aafStrongRefVectorEntry_t structs that follow.
	 */

	uint32_t _entryCount;

	/**
	 * The next local key that will be assigned in this vector.
	 */

	uint32_t _firstFreeKey;

	/**
	 * The highest unassigned key above #_firstFreeKey. The keys
	 * between #_firstFreeKey and #_lastFreeKey are unassigned,
	 * while there may be other gaps in key assignement this
	 * represents the largest one.
	 */

	uint32_t _lastFreeKey;

} /* __attribute__((packed)) */ aafStrongRefVectorHeader_t;

/**
 * An ordered collection of strongly referenced (contained) objects.
 * Each vector index consists of an aafStrongRefVectorHeader_t
 * followed by aafStrongRefVectorHeader_t._entryCount aafStrongRefVectorEntry_t structs.
 */

typedef struct aafStrongRefVectorEntry_t {
	/**
	 * The _localKey uniquely identifies this strong reference
	 * within this collection independently of its position
	 * within this collection. The #_localKey is used to form
	 * the name assigned to the element in this vector at the
	 * corresponding ordinal position. That is, the #_localKey
	 * of the first aafStrongRefVectorEntry_t is used to
	 * form the name of the first element in the vector and so
	 * on. The #_localKey is an insertion key.
	*/

	uint32_t _localKey;

} /* __attribute__((packed)) */ aafStrongRefVectorEntry_t;

/**
 * A weak object reference is a persistent data type that denotes
 * a weak reference to a uniquely identified object. In memory,
 * weak references are similar to pointers. When persisted, weak
 * references contain the unique identifier of the referenced object.
 *
 * An aafWeakRef_t can appears as a property value with the
 * stored form #SF_WEAK_OBJECT_REFERENCE, as an entry into
 * a weak reference vector index or set index.
 */

typedef struct _WeakObjectReference {
	/**
	 * The index into the referenced property table of
	 * the path to the property (a strong reference set)
	 * containing the referenced object.
	 */

	uint16_t _referencedPropertyIndex;

	/**
	 * The property id of the #_identification field
	 * @TODO Understand that field..
	 */

	aafPID_t _identificationPid;

	/**
	 * The length, in bytes, of the #_identification field.
	 */

	uint8_t _identificationSize;

	/**
	 * The type of the #_identification field varies from one instance
	 * of a WeakObjectReference to another. The #_identification field
	 * uniquely identifies the object within the target set.
	 */

	aafByte_t _identification[];

} /* __attribute__((packed)) */ aafWeakRef_t;

/**
 * An ordered collection of aafWeakRef_t. The aafWeakRefHeader_t
 * is common to weak reference Set and Vector.
 *
 */

typedef struct _WeakReferenceIndexHeader {
	/**
	 * The number of aafWeakRef_t structs that follow.
	 */

	uint32_t _entryCount;

	/**
	 * The index into the referenced property table of
	 * the path to the property (a strong reference set)
	 * containing the referenced object.
	 * @TODO Understand that field..
	 */

	uint16_t _referencedPropertyIndex;

	/**
	 * The property id of each aafStrongRefSetEntry_t._identification field.
	 * @TODO Understand that field..
	 */

	uint16_t _identificationPid;

	/**
	 * The length, in bytes, of each aafWeakRef_t._identification
	 * field.
	 */

	uint8_t _identificationSize;

} /* __attribute__((packed)) */ aafWeakRefHeader_t;

/* TODO : indirect vs opaque types ? */
/*
typedef struct _aafIndirect_t
{
	int                     type;
	size_t                  size;
	aafByte_t              *data;
} aafIndirect_t;

typedef struct _AAF_TaggedValueClass
{
	aafString_t             Name;
	aafIndirect_t           Value;
} AAF_ObjTaggedValue;
*/

#endif
