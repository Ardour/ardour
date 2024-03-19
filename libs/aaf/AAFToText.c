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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aaf/AAFDefs/AAFClassDefUIDs.h"
#include "aaf/AAFDefs/AAFCompressionDefs.h"
#include "aaf/AAFDefs/AAFContainerDefs.h"
#include "aaf/AAFDefs/AAFDataDefs.h"
#include "aaf/AAFDefs/AAFExtEnum.h"
#include "aaf/AAFDefs/AAFFileKinds.h"
#include "aaf/AAFDefs/AAFInterpolatorDefs.h"
#include "aaf/AAFDefs/AAFOPDefs.h"
#include "aaf/AAFDefs/AAFOperationDefs.h"
#include "aaf/AAFDefs/AAFParameterDefs.h"
#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"

#include "aaf/AAFToText.h"
#include "aaf/AAFTypes.h"

#include "aaf/AAFClass.h"
#include "aaf/utils.h"

#define debug(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_ERROR, __VA_ARGS__)

const char*
aaft_MobIDToText (aafMobID_t* mobid)
{
	size_t      strsz = 200;
	static char str[200];

	size_t   i      = 0;
	uint32_t offset = 0;
	int      rc     = 0;

	for (i = 0; i < sizeof (aafMobID_t); i++) {
		switch (i) {
			case 12:
			case 13:
			case 14:
			case 15:
				rc = snprintf (str + offset, strsz - offset, " - ");

				assert (rc > 0 && (size_t)rc < strsz - offset);

				offset += (uint32_t)rc;

				break;

			default:
				break;
		}

		rc = snprintf (str + offset, strsz - offset, "%02x", ((unsigned char*)mobid)[i]);

		assert (rc > 0 && (size_t)rc < strsz - offset);

		offset += (uint32_t)rc;

		if (i == 15) {
			rc = snprintf (str + offset, strsz - offset, " - ");

			assert (rc > 0 && (size_t)rc < strsz - offset);

			offset += (uint32_t)rc;

			break;
		}
	}

	aafUID_t material;

	memcpy (&material, ((unsigned char*)mobid) + i, sizeof (aafUID_t));

	rc = snprintf (str + offset, strsz - offset, "%s", AUIDToText (&material));

	assert (rc >= 0 && (size_t)rc < strsz - offset);

	return str;
}

const char*
aaft_TimestampToText (aafTimeStamp_t* ts)
{
	static char str[32];

	if (ts == NULL) {
		str[0] = 'n';
		str[1] = '/';
		str[2] = 'a';
		str[3] = '\0';
	} else {
		int rc = snprintf (str, sizeof (str), "%04i-%02u-%02u %02u:%02u:%02u.%02u",
		                   ts->date.year,
		                   ts->date.month,
		                   ts->date.day,
		                   ts->time.hour,
		                   ts->time.minute,
		                   ts->time.second,
		                   ts->time.fraction);

		assert (rc > 0 && (size_t)rc < sizeof (str));
	}

	return str;
}

const char*
aaft_VersionToText (aafVersionType_t* vers)
{
	static char str[16];

	if (vers == NULL) {
		str[0] = 'n';
		str[1] = '/';
		str[2] = 'a';
		str[3] = '\0';
	} else {
		int rc = snprintf (str, sizeof (str), "%i.%i",
		                   vers->major,
		                   vers->minor);

		assert (rc > 0 && (size_t)rc < sizeof (str));
	}

	return str;
}

const char*
aaft_ProductVersionToText (aafProductVersion_t* vers)
{
	static char str[64];

	if (vers == NULL) {
		str[0] = 'n';
		str[1] = '/';
		str[2] = 'a';
		str[3] = '\0';
	} else {
		int rc = snprintf (str, sizeof (str), "%u.%u.%u.%u %s (%i)",
		                   vers->major,
		                   vers->minor,
		                   vers->tertiary,
		                   vers->patchLevel,
		                   aaft_ProductReleaseTypeToText (vers->type),
		                   vers->type);

		assert (rc > 0 && (size_t)rc < sizeof (str));
	}

	return str;
}

const char*
aaft_FileKindToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	/* NOTE: AAFUID_NULL = AAFFileKind_DontCare */
	if (aafUIDCmp (auid, &AAFFileKind_DontCare))
		return "AAFFileKind_DontCare";
	if (aafUIDCmp (auid, &AAFFileKind_Aaf512Binary))
		return "AAFFileKind_Aaf512Binary";
	if (aafUIDCmp (auid, &AAFFileKind_Aaf4KBinary))
		return "AAFFileKind_Aaf4KBinary";
	if (aafUIDCmp (auid, &AAFFileKind_AafXmlText))
		return "AAFFileKind_AafXmlText";
	if (aafUIDCmp (auid, &AAFFileKind_AafKlvBinary))
		return "AAFFileKind_AafKlvBinary";
	if (aafUIDCmp (auid, &AAFFileKind_AafM512Binary))
		return "AAFFileKind_AafM512Binary";
	if (aafUIDCmp (auid, &AAFFileKind_AafS512Binary))
		return "AAFFileKind_AafS512Binary";
	if (aafUIDCmp (auid, &AAFFileKind_AafG512Binary))
		return "AAFFileKind_AafG512Binary";
	if (aafUIDCmp (auid, &AAFFileKind_AafM4KBinary))
		return "AAFFileKind_AafM4KBinary";
	if (aafUIDCmp (auid, &AAFFileKind_AafS4KBinary))
		return "AAFFileKind_AafS4KBinary";
	if (aafUIDCmp (auid, &AAFFileKind_AafG4KBinary))
		return "AAFFileKind_AafG4KBinary";
	if (aafUIDCmp (auid, &AAFFileKind_Pathological))
		return "AAFFileKind_Pathological";

	return "Unknown AAFFileKind";
}

const char*
aaft_TapeCaseTypeToText (aafTapeCaseType_t t)
{
	switch (t) {
		case AAFTapeCaseNull:
			return "AAFTapeCaseNull";
		case AAFThreeFourthInchVideoTape:
			return "AAFThreeFourthInchVideoTape";
		case AAFVHSVideoTape:
			return "AAFVHSVideoTape";
		case AAF8mmVideoTape:
			return "AAF8mmVideoTape";
		case AAFBetacamVideoTape:
			return "AAFBetacamVideoTape";
		case AAFCompactCassette:
			return "AAFCompactCassette";
		case AAFDATCartridge:
			return "AAFDATCartridge";
		case AAFNagraAudioTape:
			return "AAFNagraAudioTape";
	}

	return "Unknown TapeCaseType";
}

const char*
aaft_VideoSignalTypeToText (aafVideoSignalType_t v)
{
	switch (v) {
		case AAFVideoSignalNull:
			return "AAFVideoSignalNull";
		case AAFNTSCSignal:
			return "AAFNTSCSignal";
		case AAFPALSignal:
			return "AAFPALSignal";
		case AAFSECAMSignal:
			return "AAFSECAMSignal";
	}

	return "Unknown VideoSignalType";
}

const char*
aaft_TapeFormatTypeToText (aafTapeFormatType_t t)
{
	switch (t) {
		case AAFTapeFormatNull:
			return "AAFTapeFormatNull";
		case AAFBetacamFormat:
			return "AAFBetacamFormat";
		case AAFBetacamSPFormat:
			return "AAFBetacamSPFormat";
		case AAFVHSFormat:
			return "AAFVHSFormat";
		case AAFSVHSFormat:
			return "AAFSVHSFormat";
		case AAF8mmFormat:
			return "AAF8mmFormat";
		case AAFHi8Format:
			return "AAFHi8Format";
	}

	return "Unknown TapeFormatType";
}

const char*
aaft_FilmTypeToText (aafFilmType_t f)
{
	switch (f) {
		case AAFFtNull:
			return "AAFFtNull";
		case AAFFt35MM:
			return "AAFFt35MM";
		case AAFFt16MM:
			return "AAFFt16MM";
		case AAFFt8MM:
			return "AAFFt8MM";
		case AAFFt65MM:
			return "AAFFt65MM";
	}

	return "Unknown FilmType";
}

const char*
aaft_SignalStandardToText (aafSignalStandard_t s)
{
	switch (s) {
		case AAFSignalStandard_None:
			return "AAFSignalStandard_None";
		case AAFSignalStandard_ITU601:
			return "AAFSignalStandard_ITU601";
		case AAFSignalStandard_ITU1358:
			return "AAFSignalStandard_ITU1358";
		case AAFSignalStandard_SMPTE347M:
			return "AAFSignalStandard_SMPTE347M";
		case AAFSignalStandard_SMPTE274M:
			return "AAFSignalStandard_SMPTE274M";
		case AAFSignalStandard_SMPTE296M:
			return "AAFSignalStandard_SMPTE296M";
		case AAFSignalStandard_SMPTE349M:
			return "AAFSignalStandard_SMPTE349M";
	}

	return "Unknown SignalStandard";
}

const char*
aaft_FieldNumberToText (aafFieldNumber_t f)
{
	switch (f) {
		case AAFUnspecifiedField:
			return "AAFUnspecifiedField";
		case AAFFieldOne:
			return "AAFFieldOne";
		case AAFFieldTwo:
			return "AAFFieldTwo";
	}

	return "Unknown FieldNumber";
}

const char*
aaft_AlphaTransparencyToText (aafAlphaTransparency_t a)
{
	switch (a) {
		case AAFMinValueTransparent:
			return "AAFMinValueTransparent";
		case AAFMaxValueTransparent:
			return "AAFMaxValueTransparent";
	}

	return "Unknown AlphaTransparency";
}

const char*
aaft_FrameLayoutToText (aafFrameLayout_t f)
{
	switch (f) {
		case AAFFullFrame:
			return "AAFFullFrame";
		case AAFSeparateFields:
			return "AAFSeparateFields";
		case AAFOneField:
			return "AAFOneField";
		case AAFMixedFields:
			return "AAFMixedFields";
		case AAFSegmentedFrame:
			return "AAFSegmentedFrame";
	}

	return "Unknown FrameLayout";
}

const char*
aaft_ColorSitingToText (aafColorSiting_t c)
{
	switch (c) {
		case AAFCoSiting:
			return "AAFCoSiting";
		case AAFAveraging:
			return "AAFAveraging";
		case AAFThreeTap:
			return "AAFThreeTap";
		case AAFQuincunx:
			return "AAFQuincunx";
		case AAFRec601:
			return "AAFRec601";
		case AAFUnknownSiting:
			return "AAFUnknownSiting";
	}

	return "Unknown ColorSiting";
}

const char*
aaft_ProductReleaseTypeToText (aafProductReleaseType_t t)
{
	switch (t) {
		case AAFVersionUnknown:
			return "AAFVersionUnknown";
		case AAFVersionReleased:
			return "AAFVersionReleased";
		case AAFVersionDebug:
			return "AAFVersionDebug";
		case AAFVersionPatched:
			return "AAFVersionPatched";
		case AAFVersionBeta:
			return "AAFVersionBeta";
		case AAFVersionPrivateBuild:
			return "AAFVersionPrivateBuild";
	}

	return "Unknown ProductReleaseType";
}

const char*
aaft_FadeTypeToText (aafFadeType_t f)
{
	switch (f) {
		case AAFFadeNone:
			return "AAFFadeNone";
		case AAFFadeLinearAmp:
			return "AAFFadeLinearAmp";
		case AAFFadeLinearPower:
			return "AAFFadeLinearPower";
	}

	return "Unknown FadeType";
}

const char*
aaft_BoolToText (aafBoolean_t b)
{
	switch (b) {
		case 1:
			return "True";
		case 0:
			return "False";
	}

	return "Unknown Boolean";
}

const char*
aaft_OperationCategoryToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFOperationCategory_Effect))
		return "AAFOperationCategory_Effect";

	return "Unknown AAFOperationCategory";
}

const char*
aaft_PluginCategoryToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFPluginCategory_Effect))
		return "AAFPluginCategory_Effect";
	if (aafUIDCmp (auid, &AAFPluginCategory_Codec))
		return "AAFPluginCategory_Codec";
	if (aafUIDCmp (auid, &AAFPluginCategory_Interpolation))
		return "AAFPluginCategory_Interpolation";

	return "Unknown AAFPluginCategory";
}

const char*
aaft_ScanningDirectionToText (aafScanningDirection_t s)
{
	switch (s) {
		case AAFScanningDirection_LeftToRightTopToBottom:
			return "AAFScanningDirection_LeftToRightTopToBottom";
		case AAFScanningDirection_RightToLeftTopToBottom:
			return "AAFScanningDirection_RightToLeftTopToBottom";
		case AAFScanningDirection_LeftToRightBottomToTop:
			return "AAFScanningDirection_LeftToRightBottomToTop";
		case AAFScanningDirection_RightToLeftBottomToTop:
			return "AAFScanningDirection_RightToLeftBottomToTop";
		case AAFScanningDirection_TopToBottomLeftToRight:
			return "AAFScanningDirection_TopToBottomLeftToRight";
		case AAFScanningDirection_TopToBottomRightToLeft:
			return "AAFScanningDirection_TopToBottomRightToLeft";
		case AAFScanningDirection_BottomToTopLeftToRight:
			return "AAFScanningDirection_BottomToTopLeftToRight";
		case AAFScanningDirection_BottomToTopRightToLeft:
			return "AAFScanningDirection_BottomToTopRightToLeft";
	}

	return "Unknown AAFScanningDirection";
}

const char*
aaft_ByteOrderToText (int16_t bo)
{
	switch (bo) {
		case AAF_HEADER_BYTEORDER_LE:
		case AAF_PROPERTIES_BYTEORDER_LE:
			return "Little-Endian";

		case AAF_HEADER_BYTEORDER_BE:
		case AAF_PROPERTIES_BYTEORDER_BE:
			return "Big-Endian";
	}

	return "Unknown ByteOrder";
}

const char*
aaft_ElectroSpatialToText (aafElectroSpatialFormulation_t e)
{
	switch (e) {
		case AAFElectroSpatialFormulation_Default:
			return "AAFElectroSpatialFormulation_Default";
		case AAFElectroSpatialFormulation_TwoChannelMode:
			return "AAFElectroSpatialFormulation_TwoChannelMode";
		case AAFElectroSpatialFormulation_SingleChannelMode:
			return "AAFElectroSpatialFormulation_SingleChannelMode";
		case AAFElectroSpatialFormulation_PrimarySecondaryMode:
			return "AAFElectroSpatialFormulation_PrimarySecondaryMode";
		case AAFElectroSpatialFormulation_StereophonicMode:
			return "AAFElectroSpatialFormulation_StereophonicMode";
		case AAFElectroSpatialFormulation_SingleChannelDoubleSamplingFrequencyMode:
			return "AAFElectroSpatialFormulation_SingleChannelDoubleSamplingFrequencyMode";
		case AAFElectroSpatialFormulation_StereoLeftChannelDoubleSamplingFrequencyMode:
			return "AAFElectroSpatialFormulation_StereoLeftChannelDoubleSamplingFrequencyMode";
		case AAFElectroSpatialFormulation_StereoRightChannelDoubleSamplingFrequencyMode:
			return "AAFElectroSpatialFormulation_StereoRightChannelDoubleSamplingFrequencyMode";
		case AAFElectroSpatialFormulation_MultiChannelMode:
			return "AAFElectroSpatialFormulation_MultiChannelMode";
	}

	return "Unknown AAFElectroSpatialFormulation";
}

const char*
aaft_StoredFormToText (enum aafStoredForm_e sf)
{
	switch (sf) {
		case SF_DATA:
			return "SF_DATA";
		case SF_DATA_STREAM:
			return "SF_DATA_STREAM";
		case SF_STRONG_OBJECT_REFERENCE:
			return "SF_STRONG_OBJECT_REFERENCE";
		case SF_STRONG_OBJECT_REFERENCE_VECTOR:
			return "SF_STRONG_OBJECT_REFERENCE_VECTOR";
		case SF_STRONG_OBJECT_REFERENCE_SET:
			return "SF_STRONG_OBJECT_REFERENCE_SET";
		case SF_WEAK_OBJECT_REFERENCE:
			return "SF_WEAK_OBJECT_REFERENCE";
		case SF_WEAK_OBJECT_REFERENCE_VECTOR:
			return "SF_WEAK_OBJECT_REFERENCE_VECTOR";
		case SF_WEAK_OBJECT_REFERENCE_SET:
			return "SF_WEAK_OBJECT_REFERENCE_SET";
		case SF_WEAK_OBJECT_REFERENCE_STORED_OBJECT_ID:
			return "SF_WEAK_OBJECT_REFERENCE_STORED_OBJECT_ID";
		case SF_UNIQUE_OBJECT_ID:
			return "SF_UNIQUE_OBJECT_ID";
		case SF_OPAQUE_STREAM:
			return "SF_OPAQUE_STREAM";
	}

	return "Unknown StoredForm";
}

const char*
aaft_OPDefToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFOPDef_EditProtocol))
		return "AAFOPDef_EditProtocol";
	if (aafUIDCmp (auid, &AAFOPDef_Unconstrained))
		return "AAFOPDef_Unconstrained";

	return "Unknown AAFOPDef";
}

const char*
aaft_TypeIDToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFTypeID_UInt8))
		return "AAFTypeID_UInt8";
	if (aafUIDCmp (auid, &AAFTypeID_UInt16))
		return "AAFTypeID_UInt16";
	if (aafUIDCmp (auid, &AAFTypeID_UInt32))
		return "AAFTypeID_UInt32";
	if (aafUIDCmp (auid, &AAFTypeID_UInt64))
		return "AAFTypeID_UInt64";
	if (aafUIDCmp (auid, &AAFTypeID_Int8))
		return "AAFTypeID_Int8";
	if (aafUIDCmp (auid, &AAFTypeID_Int16))
		return "AAFTypeID_Int16";
	if (aafUIDCmp (auid, &AAFTypeID_Int32))
		return "AAFTypeID_Int32";
	if (aafUIDCmp (auid, &AAFTypeID_Int64))
		return "AAFTypeID_Int64";
	if (aafUIDCmp (auid, &AAFTypeID_PositionType))
		return "AAFTypeID_PositionType";
	if (aafUIDCmp (auid, &AAFTypeID_LengthType))
		return "AAFTypeID_LengthType";
	if (aafUIDCmp (auid, &AAFTypeID_JPEGTableIDType))
		return "AAFTypeID_JPEGTableIDType";
	if (aafUIDCmp (auid, &AAFTypeID_PhaseFrameType))
		return "AAFTypeID_PhaseFrameType";
	if (aafUIDCmp (auid, &AAFTypeID_AUID))
		return "AAFTypeID_AUID";
	if (aafUIDCmp (auid, &AAFTypeID_MobIDType))
		return "AAFTypeID_MobIDType";
	if (aafUIDCmp (auid, &AAFTypeID_Boolean))
		return "AAFTypeID_Boolean";
	if (aafUIDCmp (auid, &AAFTypeID_Character))
		return "AAFTypeID_Character";
	if (aafUIDCmp (auid, &AAFTypeID_String))
		return "AAFTypeID_String";
	if (aafUIDCmp (auid, &AAFTypeID_ProductReleaseType))
		return "AAFTypeID_ProductReleaseType";
	if (aafUIDCmp (auid, &AAFTypeID_TapeFormatType))
		return "AAFTypeID_TapeFormatType";
	if (aafUIDCmp (auid, &AAFTypeID_VideoSignalType))
		return "AAFTypeID_VideoSignalType";
	if (aafUIDCmp (auid, &AAFTypeID_TapeCaseType))
		return "AAFTypeID_TapeCaseType";
	if (aafUIDCmp (auid, &AAFTypeID_ColorSitingType))
		return "AAFTypeID_ColorSitingType";
	if (aafUIDCmp (auid, &AAFTypeID_EditHintType))
		return "AAFTypeID_EditHintType";
	if (aafUIDCmp (auid, &AAFTypeID_FadeType))
		return "AAFTypeID_FadeType";
	if (aafUIDCmp (auid, &AAFTypeID_LayoutType))
		return "AAFTypeID_LayoutType";
	if (aafUIDCmp (auid, &AAFTypeID_TCSource))
		return "AAFTypeID_TCSource";
	if (aafUIDCmp (auid, &AAFTypeID_PulldownDirectionType))
		return "AAFTypeID_PulldownDirectionType";
	if (aafUIDCmp (auid, &AAFTypeID_PulldownKindType))
		return "AAFTypeID_PulldownKindType";
	if (aafUIDCmp (auid, &AAFTypeID_EdgeType))
		return "AAFTypeID_EdgeType";
	if (aafUIDCmp (auid, &AAFTypeID_FilmType))
		return "AAFTypeID_FilmType";
	if (aafUIDCmp (auid, &AAFTypeID_RGBAComponentKind))
		return "AAFTypeID_RGBAComponentKind";
	if (aafUIDCmp (auid, &AAFTypeID_ReferenceType))
		return "AAFTypeID_ReferenceType";
	if (aafUIDCmp (auid, &AAFTypeID_AlphaTransparencyType))
		return "AAFTypeID_AlphaTransparencyType";
	if (aafUIDCmp (auid, &AAFTypeID_FieldNumber))
		return "AAFTypeID_FieldNumber";
	if (aafUIDCmp (auid, &AAFTypeID_ElectroSpatialFormulation))
		return "AAFTypeID_ElectroSpatialFormulation";
	if (aafUIDCmp (auid, &AAFTypeID_EmphasisType))
		return "AAFTypeID_EmphasisType";
	if (aafUIDCmp (auid, &AAFTypeID_AuxBitsModeType))
		return "AAFTypeID_AuxBitsModeType";
	if (aafUIDCmp (auid, &AAFTypeID_ChannelStatusModeType))
		return "AAFTypeID_ChannelStatusModeType";
	if (aafUIDCmp (auid, &AAFTypeID_UserDataModeType))
		return "AAFTypeID_UserDataModeType";
	if (aafUIDCmp (auid, &AAFTypeID_SignalStandardType))
		return "AAFTypeID_SignalStandardType";
	if (aafUIDCmp (auid, &AAFTypeID_ScanningDirectionType))
		return "AAFTypeID_ScanningDirectionType";
	if (aafUIDCmp (auid, &AAFTypeID_ContentScanningType))
		return "AAFTypeID_ContentScanningType";
	if (aafUIDCmp (auid, &AAFTypeID_TitleAlignmentType))
		return "AAFTypeID_TitleAlignmentType";
	if (aafUIDCmp (auid, &AAFTypeID_OperationCategoryType))
		return "AAFTypeID_OperationCategoryType";
	if (aafUIDCmp (auid, &AAFTypeID_TransferCharacteristicType))
		return "AAFTypeID_TransferCharacteristicType";
	if (aafUIDCmp (auid, &AAFTypeID_PluginCategoryType))
		return "AAFTypeID_PluginCategoryType";
	if (aafUIDCmp (auid, &AAFTypeID_UsageType))
		return "AAFTypeID_UsageType";
	if (aafUIDCmp (auid, &AAFTypeID_ColorPrimariesType))
		return "AAFTypeID_ColorPrimariesType";
	if (aafUIDCmp (auid, &AAFTypeID_CodingEquationsType))
		return "AAFTypeID_CodingEquationsType";
	if (aafUIDCmp (auid, &AAFTypeID_Rational))
		return "AAFTypeID_Rational";
	if (aafUIDCmp (auid, &AAFTypeID_ProductVersion))
		return "AAFTypeID_ProductVersion";
	if (aafUIDCmp (auid, &AAFTypeID_VersionType))
		return "AAFTypeID_VersionType";
	if (aafUIDCmp (auid, &AAFTypeID_RGBAComponent))
		return "AAFTypeID_RGBAComponent";
	if (aafUIDCmp (auid, &AAFTypeID_DateStruct))
		return "AAFTypeID_DateStruct";
	if (aafUIDCmp (auid, &AAFTypeID_TimeStruct))
		return "AAFTypeID_TimeStruct";
	if (aafUIDCmp (auid, &AAFTypeID_TimeStamp))
		return "AAFTypeID_TimeStamp";
	if (aafUIDCmp (auid, &AAFTypeID_UInt8Array))
		return "AAFTypeID_UInt8Array";
	if (aafUIDCmp (auid, &AAFTypeID_UInt8Array12))
		return "AAFTypeID_UInt8Array12";
	if (aafUIDCmp (auid, &AAFTypeID_Int32Array))
		return "AAFTypeID_Int32Array";
	if (aafUIDCmp (auid, &AAFTypeID_Int64Array))
		return "AAFTypeID_Int64Array";
	if (aafUIDCmp (auid, &AAFTypeID_StringArray))
		return "AAFTypeID_StringArray";
	if (aafUIDCmp (auid, &AAFTypeID_AUIDArray))
		return "AAFTypeID_AUIDArray";
	if (aafUIDCmp (auid, &AAFTypeID_PositionArray))
		return "AAFTypeID_PositionArray";
	if (aafUIDCmp (auid, &AAFTypeID_UInt8Array8))
		return "AAFTypeID_UInt8Array8";
	if (aafUIDCmp (auid, &AAFTypeID_UInt32Array))
		return "AAFTypeID_UInt32Array";
	if (aafUIDCmp (auid, &AAFTypeID_ChannelStatusModeArray))
		return "AAFTypeID_ChannelStatusModeArray";
	if (aafUIDCmp (auid, &AAFTypeID_UserDataModeArray))
		return "AAFTypeID_UserDataModeArray";
	if (aafUIDCmp (auid, &AAFTypeID_RGBALayout))
		return "AAFTypeID_RGBALayout";
	if (aafUIDCmp (auid, &AAFTypeID_AUIDSet))
		return "AAFTypeID_AUIDSet";
	if (aafUIDCmp (auid, &AAFTypeID_UInt32Set))
		return "AAFTypeID_UInt32Set";
	if (aafUIDCmp (auid, &AAFTypeID_DataValue))
		return "AAFTypeID_DataValue";
	if (aafUIDCmp (auid, &AAFTypeID_Stream))
		return "AAFTypeID_Stream";
	if (aafUIDCmp (auid, &AAFTypeID_Indirect))
		return "AAFTypeID_Indirect";
	if (aafUIDCmp (auid, &AAFTypeID_Opaque))
		return "AAFTypeID_Opaque";
	if (aafUIDCmp (auid, &AAFTypeID_ClassDefinitionWeakReference))
		return "AAFTypeID_ClassDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_ContainerDefinitionWeakReference))
		return "AAFTypeID_ContainerDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_DataDefinitionWeakReference))
		return "AAFTypeID_DataDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_InterpolationDefinitionWeakReference))
		return "AAFTypeID_InterpolationDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_MobWeakReference))
		return "AAFTypeID_MobWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_OperationDefinitionWeakReference))
		return "AAFTypeID_OperationDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_ParameterDefinitionWeakReference))
		return "AAFTypeID_ParameterDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_TypeDefinitionWeakReference))
		return "AAFTypeID_TypeDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_PluginDefinitionWeakReference))
		return "AAFTypeID_PluginDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_CodecDefinitionWeakReference))
		return "AAFTypeID_CodecDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_PropertyDefinitionWeakReference))
		return "AAFTypeID_PropertyDefinitionWeakReference";
	if (aafUIDCmp (auid, &AAFTypeID_ContentStorageStrongReference))
		return "AAFTypeID_ContentStorageStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_DictionaryStrongReference))
		return "AAFTypeID_DictionaryStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_EssenceDescriptorStrongReference))
		return "AAFTypeID_EssenceDescriptorStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_NetworkLocatorStrongReference))
		return "AAFTypeID_NetworkLocatorStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_OperationGroupStrongReference))
		return "AAFTypeID_OperationGroupStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_SegmentStrongReference))
		return "AAFTypeID_SegmentStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_SourceClipStrongReference))
		return "AAFTypeID_SourceClipStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_SourceReferenceStrongReference))
		return "AAFTypeID_SourceReferenceStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_ClassDefinitionStrongReference))
		return "AAFTypeID_ClassDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_CodecDefinitionStrongReference))
		return "AAFTypeID_CodecDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_ComponentStrongReference))
		return "AAFTypeID_ComponentStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_ContainerDefinitionStrongReference))
		return "AAFTypeID_ContainerDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_ControlPointStrongReference))
		return "AAFTypeID_ControlPointStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_DataDefinitionStrongReference))
		return "AAFTypeID_DataDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_EssenceDataStrongReference))
		return "AAFTypeID_EssenceDataStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_IdentificationStrongReference))
		return "AAFTypeID_IdentificationStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_InterpolationDefinitionStrongReference))
		return "AAFTypeID_InterpolationDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_LocatorStrongReference))
		return "AAFTypeID_LocatorStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_MobStrongReference))
		return "AAFTypeID_MobStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_MobSlotStrongReference))
		return "AAFTypeID_MobSlotStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_OperationDefinitionStrongReference))
		return "AAFTypeID_OperationDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_ParameterStrongReference))
		return "AAFTypeID_ParameterStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_ParameterDefinitionStrongReference))
		return "AAFTypeID_ParameterDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_PluginDefinitionStrongReference))
		return "AAFTypeID_PluginDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_PropertyDefinitionStrongReference))
		return "AAFTypeID_PropertyDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_TaggedValueStrongReference))
		return "AAFTypeID_TaggedValueStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_TypeDefinitionStrongReference))
		return "AAFTypeID_TypeDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_KLVDataStrongReference))
		return "AAFTypeID_KLVDataStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_FileDescriptorStrongReference))
		return "AAFTypeID_FileDescriptorStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_RIFFChunkStrongReference))
		return "AAFTypeID_RIFFChunkStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_DescriptiveFrameworkStrongReference))
		return "AAFTypeID_DescriptiveFrameworkStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_KLVDataDefinitionStrongReference))
		return "AAFTypeID_KLVDataDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_TaggedValueDefinitionStrongReference))
		return "AAFTypeID_TaggedValueDefinitionStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_DescriptiveObjectStrongReference))
		return "AAFTypeID_DescriptiveObjectStrongReference";
	if (aafUIDCmp (auid, &AAFTypeID_DataDefinitionWeakReferenceSet))
		return "AAFTypeID_DataDefinitionWeakReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_ParameterDefinitionWeakReferenceSet))
		return "AAFTypeID_ParameterDefinitionWeakReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_PluginDefinitionWeakReferenceSet))
		return "AAFTypeID_PluginDefinitionWeakReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_PropertyDefinitionWeakReferenceSet))
		return "AAFTypeID_PropertyDefinitionWeakReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_OperationDefinitionWeakReferenceVector))
		return "AAFTypeID_OperationDefinitionWeakReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_TypeDefinitionWeakReferenceVector))
		return "AAFTypeID_TypeDefinitionWeakReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_DataDefinitionWeakReferenceVector))
		return "AAFTypeID_DataDefinitionWeakReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_ClassDefinitionStrongReferenceSet))
		return "AAFTypeID_ClassDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_CodecDefinitionStrongReferenceSet))
		return "AAFTypeID_CodecDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_ContainerDefinitionStrongReferenceSet))
		return "AAFTypeID_ContainerDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_DataDefinitionStrongReferenceSet))
		return "AAFTypeID_DataDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_EssenceDataStrongReferenceSet))
		return "AAFTypeID_EssenceDataStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_InterpolationDefinitionStrongReferenceSet))
		return "AAFTypeID_InterpolationDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_MobStrongReferenceSet))
		return "AAFTypeID_MobStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_OperationDefinitionStrongReferenceSet))
		return "AAFTypeID_OperationDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_ParameterDefinitionStrongReferenceSet))
		return "AAFTypeID_ParameterDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_PluginDefinitionStrongReferenceSet))
		return "AAFTypeID_PluginDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_PropertyDefinitionStrongReferenceSet))
		return "AAFTypeID_PropertyDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_TypeDefinitionStrongReferenceSet))
		return "AAFTypeID_TypeDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_KLVDataDefinitionStrongReferenceSet))
		return "AAFTypeID_KLVDataDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_TaggedValueDefinitionStrongReferenceSet))
		return "AAFTypeID_TaggedValueDefinitionStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_DescriptiveObjectStrongReferenceSet))
		return "AAFTypeID_DescriptiveObjectStrongReferenceSet";
	if (aafUIDCmp (auid, &AAFTypeID_ComponentStrongReferenceVector))
		return "AAFTypeID_ComponentStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_ControlPointStrongReferenceVector))
		return "AAFTypeID_ControlPointStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_IdentificationStrongReferenceVector))
		return "AAFTypeID_IdentificationStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_LocatorStrongReferenceVector))
		return "AAFTypeID_LocatorStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_MobSlotStrongReferenceVector))
		return "AAFTypeID_MobSlotStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_SegmentStrongReferenceVector))
		return "AAFTypeID_SegmentStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_SourceReferenceStrongReferenceVector))
		return "AAFTypeID_SourceReferenceStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_TaggedValueStrongReferenceVector))
		return "AAFTypeID_TaggedValueStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_KLVDataStrongReferenceVector))
		return "AAFTypeID_KLVDataStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_ParameterStrongReferenceVector))
		return "AAFTypeID_ParameterStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_FileDescriptorStrongReferenceVector))
		return "AAFTypeID_FileDescriptorStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_RIFFChunkStrongReferenceVector))
		return "AAFTypeID_RIFFChunkStrongReferenceVector";
	if (aafUIDCmp (auid, &AAFTypeID_DescriptiveObjectStrongReferenceVector))
		return "AAFTypeID_DescriptiveObjectStrongReferenceVector";

	return "Unknown AAFTypeID";
}

const char*
aaft_DataDefToText (AAF_Data* aafd, const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFDataDef_Picture))
		return "AAFDataDef_Picture";
	if (aafUIDCmp (auid, &AAFDataDef_LegacyPicture))
		return "AAFDataDef_LegacyPicture";
	if (aafUIDCmp (auid, &AAFDataDef_Matte))
		return "AAFDataDef_Matte";
	if (aafUIDCmp (auid, &AAFDataDef_PictureWithMatte))
		return "AAFDataDef_PictureWithMatte";
	if (aafUIDCmp (auid, &AAFDataDef_Sound))
		return "AAFDataDef_Sound";
	if (aafUIDCmp (auid, &AAFDataDef_LegacySound))
		return "AAFDataDef_LegacySound";
	if (aafUIDCmp (auid, &AAFDataDef_Timecode))
		return "AAFDataDef_Timecode";
	if (aafUIDCmp (auid, &AAFDataDef_LegacyTimecode))
		return "AAFDataDef_LegacyTimecode";
	if (aafUIDCmp (auid, &AAFDataDef_Edgecode))
		return "AAFDataDef_Edgecode";
	if (aafUIDCmp (auid, &AAFDataDef_DescriptiveMetadata))
		return "AAFDataDef_DescriptiveMetadata";
	if (aafUIDCmp (auid, &AAFDataDef_Auxiliary))
		return "AAFDataDef_Auxiliary";
	if (aafUIDCmp (auid, &AAFDataDef_Unknown))
		return "AAFDataDef_Unknown";

	static char TEXTDataDef[1024];

	aafObject* DataDefinitions = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_DataDefinitions, &AAFTypeID_DataDefinitionStrongReferenceSet);
	aafObject* DataDefinition  = NULL;

	while (_aaf_foreach_ObjectInSet (&DataDefinition, DataDefinitions, NULL)) {
		aafUID_t* DataDefIdent = aaf_get_propertyValue (DataDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

		if (DataDefIdent && aafUIDCmp (DataDefIdent, auid)) {
			char* name = aaf_get_propertyValue (DataDefinition, PID_DefinitionObject_Name, &AAFTypeID_String);

			if (!name) {
				error ("Could not retrieve DataDefinition::Name");
				return NULL;
			}

			int rc = snprintf (TEXTDataDef, sizeof (TEXTDataDef), "%s", name);

			assert (rc >= 0 && (size_t)rc < sizeof (TEXTDataDef));

			free (name);

			return TEXTDataDef;
		}
	}

	return "Unknown AAFDataDef";
}

const char*
aaft_OperationDefToText (AAF_Data* aafd, const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoDissolve))
		return "AAFOperationDef_VideoDissolve";
	if (aafUIDCmp (auid, &AAFOperationDef_SMPTEVideoWipe))
		return "AAFOperationDef_SMPTEVideoWipe";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoSpeedControl))
		return "AAFOperationDef_VideoSpeedControl";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoRepeat))
		return "AAFOperationDef_VideoRepeat";
	if (aafUIDCmp (auid, &AAFOperationDef_Flip))
		return "AAFOperationDef_Flip";
	if (aafUIDCmp (auid, &AAFOperationDef_Flop))
		return "AAFOperationDef_Flop";
	if (aafUIDCmp (auid, &AAFOperationDef_FlipFlop))
		return "AAFOperationDef_FlipFlop";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoPosition))
		return "AAFOperationDef_VideoPosition";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoCrop))
		return "AAFOperationDef_VideoCrop";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoScale))
		return "AAFOperationDef_VideoScale";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoRotate))
		return "AAFOperationDef_VideoRotate";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoCornerPinning))
		return "AAFOperationDef_VideoCornerPinning";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoAlphaWithinVideoKey))
		return "AAFOperationDef_VideoAlphaWithinVideoKey";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoSeparateAlphaKey))
		return "AAFOperationDef_VideoSeparateAlphaKey";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoLuminanceKey))
		return "AAFOperationDef_VideoLuminanceKey";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoChromaKey))
		return "AAFOperationDef_VideoChromaKey";
	if (aafUIDCmp (auid, &AAFOperationDef_MonoAudioGain))
		return "AAFOperationDef_MonoAudioGain";
	if (aafUIDCmp (auid, &AAFOperationDef_MonoAudioPan))
		return "AAFOperationDef_MonoAudioPan";
	if (aafUIDCmp (auid, &AAFOperationDef_MonoAudioDissolve))
		return "AAFOperationDef_MonoAudioDissolve";
	if (aafUIDCmp (auid, &AAFOperationDef_TwoParameterMonoAudioDissolve))
		return "AAFOperationDef_TwoParameterMonoAudioDissolve";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoOpacity))
		return "AAFOperationDef_VideoOpacity";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoTitle))
		return "AAFOperationDef_VideoTitle";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoColor))
		return "AAFOperationDef_VideoColor";
	if (aafUIDCmp (auid, &AAFOperationDef_Unknown))
		return "AAFOperationDef_Unknown";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoFadeToBlack))
		return "AAFOperationDef_VideoFadeToBlack";
	if (aafUIDCmp (auid, &AAFOperationDef_PictureWithMate))
		return "AAFOperationDef_PictureWithMate";
	if (aafUIDCmp (auid, &AAFOperationDef_VideoFrameToMask))
		return "AAFOperationDef_VideoFrameToMask";
	if (aafUIDCmp (auid, &AAFOperationDef_StereoAudioDissolve))
		return "AAFOperationDef_StereoAudioDissolve";
	if (aafUIDCmp (auid, &AAFOperationDef_StereoAudioGain))
		return "AAFOperationDef_StereoAudioGain";
	if (aafUIDCmp (auid, &AAFOperationDef_MonoAudioMixdown))
		return "AAFOperationDef_MonoAudioMixdown";
	if (aafUIDCmp (auid, &AAFOperationDef_AudioChannelCombiner))
		return "AAFOperationDef_AudioChannelCombiner";

	static char TEXTOperationDef[1024];

	aafObject* OperationDefinitions = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_OperationDefinitions, &AAFTypeID_OperationDefinitionStrongReferenceSet);
	aafObject* OperationDefinition  = NULL;

	while (_aaf_foreach_ObjectInSet (&OperationDefinition, OperationDefinitions, NULL)) {
		aafUID_t* OpDefIdent = aaf_get_propertyValue (OperationDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

		if (OpDefIdent && aafUIDCmp (OpDefIdent, auid)) {
			char* name = aaf_get_propertyValue (OperationDefinition, PID_DefinitionObject_Name, &AAFTypeID_String);

			if (!name) {
				error ("Could not retrieve OperationDefinition::Name");
				return NULL;
			}

			int rc = snprintf (TEXTOperationDef, sizeof (TEXTOperationDef), "%s", name);

			assert (rc >= 0 && (size_t)rc < sizeof (TEXTOperationDef));

			free (name);

			return TEXTOperationDef;
		}
	}

	return "Unknown AAFOperationDef";
}

const char*
aaft_InterpolationToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFInterpolationDef_None))
		return "AAFInterpolationDef_None";
	if (aafUIDCmp (auid, &AAFInterpolationDef_Linear))
		return "AAFInterpolationDef_Linear";
	if (aafUIDCmp (auid, &AAFInterpolationDef_Constant))
		return "AAFInterpolationDef_Constant";
	if (aafUIDCmp (auid, &AAFInterpolationDef_BSpline))
		return "AAFInterpolationDef_BSpline";
	if (aafUIDCmp (auid, &AAFInterpolationDef_Log))
		return "AAFInterpolationDef_Log";
	if (aafUIDCmp (auid, &AAFInterpolationDef_Power))
		return "AAFInterpolationDef_Power";

	return "Unknown AAFInterpolationDef";
}

const char*
aaft_ParameterToText (AAF_Data* aafd, const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFParameterDef_Level))
		return "AAFParameterDef_Level";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEWipeNumber))
		return "AAFParameterDef_SMPTEWipeNumber";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEReverse))
		return "AAFParameterDef_SMPTEReverse";
	if (aafUIDCmp (auid, &AAFParameterDef_SpeedRatio))
		return "AAFParameterDef_SpeedRatio";
	if (aafUIDCmp (auid, &AAFParameterDef_PositionOffsetX))
		return "AAFParameterDef_PositionOffsetX";
	if (aafUIDCmp (auid, &AAFParameterDef_PositionOffsetY))
		return "AAFParameterDef_PositionOffsetY";
	if (aafUIDCmp (auid, &AAFParameterDef_CropLeft))
		return "AAFParameterDef_CropLeft";
	if (aafUIDCmp (auid, &AAFParameterDef_CropRight))
		return "AAFParameterDef_CropRight";
	if (aafUIDCmp (auid, &AAFParameterDef_CropTop))
		return "AAFParameterDef_CropTop";
	if (aafUIDCmp (auid, &AAFParameterDef_CropBottom))
		return "AAFParameterDef_CropBottom";
	if (aafUIDCmp (auid, &AAFParameterDef_ScaleX))
		return "AAFParameterDef_ScaleX";
	if (aafUIDCmp (auid, &AAFParameterDef_ScaleY))
		return "AAFParameterDef_ScaleY";
	if (aafUIDCmp (auid, &AAFParameterDef_Rotation))
		return "AAFParameterDef_Rotation";
	if (aafUIDCmp (auid, &AAFParameterDef_PinTopLeftX))
		return "AAFParameterDef_PinTopLeftX";
	if (aafUIDCmp (auid, &AAFParameterDef_PinTopLeftY))
		return "AAFParameterDef_PinTopLeftY";
	if (aafUIDCmp (auid, &AAFParameterDef_PinTopRightX))
		return "AAFParameterDef_PinTopRightX";
	if (aafUIDCmp (auid, &AAFParameterDef_PinTopRightY))
		return "AAFParameterDef_PinTopRightY";
	if (aafUIDCmp (auid, &AAFParameterDef_PinBottomLeftX))
		return "AAFParameterDef_PinBottomLeftX";
	if (aafUIDCmp (auid, &AAFParameterDef_PinBottomLeftY))
		return "AAFParameterDef_PinBottomLeftY";
	if (aafUIDCmp (auid, &AAFParameterDef_PinBottomRightX))
		return "AAFParameterDef_PinBottomRightX";
	if (aafUIDCmp (auid, &AAFParameterDef_PinBottomRightY))
		return "AAFParameterDef_PinBottomRightY";
	if (aafUIDCmp (auid, &AAFParameterDef_AlphaKeyInvertAlpha))
		return "AAFParameterDef_AlphaKeyInvertAlpha";
	if (aafUIDCmp (auid, &AAFParameterDef_LumKeyLevel))
		return "AAFParameterDef_LumKeyLevel";
	if (aafUIDCmp (auid, &AAFParameterDef_LumKeyClip))
		return "AAFParameterDef_LumKeyClip";
	if (aafUIDCmp (auid, &AAFParameterDef_Amplitude))
		return "AAFParameterDef_Amplitude";
	if (aafUIDCmp (auid, &AAFParameterDef_Pan))
		return "AAFParameterDef_Pan";
	if (aafUIDCmp (auid, &AAFParameterDef_OutgoingLevel))
		return "AAFParameterDef_OutgoingLevel";
	if (aafUIDCmp (auid, &AAFParameterDef_IncomingLevel))
		return "AAFParameterDef_IncomingLevel";
	if (aafUIDCmp (auid, &AAFParameterDef_OpacityLevel))
		return "AAFParameterDef_OpacityLevel";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleText))
		return "AAFParameterDef_TitleText";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleFontName))
		return "AAFParameterDef_TitleFontName";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleFontSize))
		return "AAFParameterDef_TitleFontSize";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleFontColorR))
		return "AAFParameterDef_TitleFontColorR";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleFontColorG))
		return "AAFParameterDef_TitleFontColorG";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleFontColorB))
		return "AAFParameterDef_TitleFontColorB";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleAlignment))
		return "AAFParameterDef_TitleAlignment";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleBold))
		return "AAFParameterDef_TitleBold";
	if (aafUIDCmp (auid, &AAFParameterDef_TitleItalic))
		return "AAFParameterDef_TitleItalic";
	if (aafUIDCmp (auid, &AAFParameterDef_TitlePositionX))
		return "AAFParameterDef_TitlePositionX";
	if (aafUIDCmp (auid, &AAFParameterDef_TitlePositionY))
		return "AAFParameterDef_TitlePositionY";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorSlopeR))
		return "AAFParameterDef_ColorSlopeR";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorSlopeG))
		return "AAFParameterDef_ColorSlopeG";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorSlopeB))
		return "AAFParameterDef_ColorSlopeB";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorOffsetR))
		return "AAFParameterDef_ColorOffsetR";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorOffsetG))
		return "AAFParameterDef_ColorOffsetG";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorOffsetB))
		return "AAFParameterDef_ColorOffsetB";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorPowerR))
		return "AAFParameterDef_ColorPowerR";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorPowerG))
		return "AAFParameterDef_ColorPowerG";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorPowerB))
		return "AAFParameterDef_ColorPowerB";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorSaturation))
		return "AAFParameterDef_ColorSaturation";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorCorrectionDescription))
		return "AAFParameterDef_ColorCorrectionDescription";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorInputDescription))
		return "AAFParameterDef_ColorInputDescription";
	if (aafUIDCmp (auid, &AAFParameterDef_ColorViewingDescription))
		return "AAFParameterDef_ColorViewingDescription";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTESoft))
		return "AAFParameterDef_SMPTESoft";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEBorder))
		return "AAFParameterDef_SMPTEBorder";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEPosition))
		return "AAFParameterDef_SMPTEPosition";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEModulator))
		return "AAFParameterDef_SMPTEModulator";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEShadow))
		return "AAFParameterDef_SMPTEShadow";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTETumble))
		return "AAFParameterDef_SMPTETumble";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTESpotlight))
		return "AAFParameterDef_SMPTESpotlight";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEReplicationH))
		return "AAFParameterDef_SMPTEReplicationH";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTEReplicationV))
		return "AAFParameterDef_SMPTEReplicationV";
	if (aafUIDCmp (auid, &AAFParameterDef_SMPTECheckerboard))
		return "AAFParameterDef_SMPTECheckerboard";
	if (aafUIDCmp (auid, &AAFParameterDef_PhaseOffset))
		return "AAFParameterDef_PhaseOffset";

	/* NOTE: Seen in Avid MC and PT files : PanVol_IsTrimGainEffect */

	static char TEXTParameterDef[1024];

	aafObject* ParameterDefinitions = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_ParameterDefinitions, &AAFTypeID_ParameterDefinitionStrongReferenceSet);
	aafObject* ParameterDefinition  = NULL;

	while (_aaf_foreach_ObjectInSet (&ParameterDefinition, ParameterDefinitions, NULL)) {
		aafUID_t* ParamDefIdent = aaf_get_propertyValue (ParameterDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

		if (ParamDefIdent && aafUIDCmp (ParamDefIdent, auid)) {
			char* name = aaf_get_propertyValue (ParameterDefinition, PID_DefinitionObject_Name, &AAFTypeID_String);

			if (!name) {
				error ("Could not retrieve ParameterDefinition::Name");
				return NULL;
			}

			int rc = snprintf (TEXTParameterDef, sizeof (TEXTParameterDef), "%s", name);

			assert (rc >= 0 && (size_t)rc < sizeof (TEXTParameterDef));

			free (name);

			return TEXTParameterDef;
		}
	}

	return "Unknown AAFParameterDef";
}

const char*
aaft_TransferCharacteristicToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFTransferCharacteristic_ITU470_PAL))
		return "AAFTransferCharacteristic_ITU470_PAL";
	if (aafUIDCmp (auid, &AAFTransferCharacteristic_ITU709))
		return "AAFTransferCharacteristic_ITU709";
	if (aafUIDCmp (auid, &AAFTransferCharacteristic_SMPTE240M))
		return "AAFTransferCharacteristic_SMPTE240M";
	if (aafUIDCmp (auid, &AAFTransferCharacteristic_274M_296M))
		return "AAFTransferCharacteristic_274M_296M";
	if (aafUIDCmp (auid, &AAFTransferCharacteristic_ITU1361))
		return "AAFTransferCharacteristic_ITU1361";
	if (aafUIDCmp (auid, &AAFTransferCharacteristic_linear))
		return "AAFTransferCharacteristic_linear";

	return "Unknown AAFTransferCharacteristic";
}

const char*
aaft_CodingEquationsToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFCodingEquations_ITU601))
		return "AAFCodingEquations_ITU601";
	if (aafUIDCmp (auid, &AAFCodingEquations_ITU709))
		return "AAFCodingEquations_ITU709";
	if (aafUIDCmp (auid, &AAFCodingEquations_SMPTE240M))
		return "AAFCodingEquations_SMPTE240M";

	return "Unknown AAFCodingEquations";
}

const char*
aaft_ColorPrimariesToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFColorPrimaries_SMPTE170M))
		return "AAFColorPrimaries_SMPTE170M";
	if (aafUIDCmp (auid, &AAFColorPrimaries_ITU470_PAL))
		return "AAFColorPrimaries_ITU470_PAL";
	if (aafUIDCmp (auid, &AAFColorPrimaries_ITU709))
		return "AAFColorPrimaries_ITU709";

	return "Unknown AAFColorPrimaries";
}

const char*
aaft_UsageCodeToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AAFUID_NULL))
		return "AAFUID_NULL";
	if (aafUIDCmp (auid, &AAFUsage_SubClip))
		return "AAFUsage_SubClip";
	if (aafUIDCmp (auid, &AAFUsage_AdjustedClip))
		return "AAFUsage_AdjustedClip";
	if (aafUIDCmp (auid, &AAFUsage_TopLevel))
		return "AAFUsage_TopLevel";
	if (aafUIDCmp (auid, &AAFUsage_LowerLevel))
		return "AAFUsage_LowerLevel";
	if (aafUIDCmp (auid, &AAFUsage_Template))
		return "AAFUsage_Template";

	return "Unknown AAFUsage";
}

const char*
aaft_PIDToText (AAF_Data* aafd, aafPID_t pid)
{
	switch (pid) {
		case PID_Root_MetaDictionary:
			return "PID_Root_MetaDictionary";
		case PID_Root_Header:
			return "PID_Root_Header";
		case PID_InterchangeObject_ObjClass:
			return "PID_InterchangeObject_ObjClass";
		case PID_InterchangeObject_Generation:
			return "PID_InterchangeObject_Generation";
		case PID_Component_DataDefinition:
			return "PID_Component_DataDefinition";
		case PID_Component_Length:
			return "PID_Component_Length";
		case PID_Component_KLVData:
			return "PID_Component_KLVData";
		case PID_Component_UserComments:
			return "PID_Component_UserComments";
		case PID_Component_Attributes:
			return "PID_Component_Attributes";
		case PID_EdgeCode_Start:
			return "PID_EdgeCode_Start";
		case PID_EdgeCode_FilmKind:
			return "PID_EdgeCode_FilmKind";
		case PID_EdgeCode_CodeFormat:
			return "PID_EdgeCode_CodeFormat";
		case PID_EdgeCode_Header:
			return "PID_EdgeCode_Header";
		case PID_EssenceGroup_Choices:
			return "PID_EssenceGroup_Choices";
		case PID_EssenceGroup_StillFrame:
			return "PID_EssenceGroup_StillFrame";
		case PID_Event_Position:
			return "PID_Event_Position";
		case PID_Event_Comment:
			return "PID_Event_Comment";
		case PID_GPITrigger_ActiveState:
			return "PID_GPITrigger_ActiveState";
		case PID_CommentMarker_Annotation:
			return "PID_CommentMarker_Annotation";
		case PID_OperationGroup_Operation:
			return "PID_OperationGroup_Operation";
		case PID_OperationGroup_InputSegments:
			return "PID_OperationGroup_InputSegments";
		case PID_OperationGroup_Parameters:
			return "PID_OperationGroup_Parameters";
		case PID_OperationGroup_BypassOverride:
			return "PID_OperationGroup_BypassOverride";
		case PID_OperationGroup_Rendering:
			return "PID_OperationGroup_Rendering";
		case PID_NestedScope_Slots:
			return "PID_NestedScope_Slots";
		case PID_Pulldown_InputSegment:
			return "PID_Pulldown_InputSegment";
		case PID_Pulldown_PulldownKind:
			return "PID_Pulldown_PulldownKind";
		case PID_Pulldown_PulldownDirection:
			return "PID_Pulldown_PulldownDirection";
		case PID_Pulldown_PhaseFrame:
			return "PID_Pulldown_PhaseFrame";
		case PID_ScopeReference_RelativeScope:
			return "PID_ScopeReference_RelativeScope";
		case PID_ScopeReference_RelativeSlot:
			return "PID_ScopeReference_RelativeSlot";
		case PID_Selector_Selected:
			return "PID_Selector_Selected";
		case PID_Selector_Alternates:
			return "PID_Selector_Alternates";
		case PID_Sequence_Components:
			return "PID_Sequence_Components";
		case PID_SourceReference_SourceID:
			return "PID_SourceReference_SourceID";
		case PID_SourceReference_SourceMobSlotID:
			return "PID_SourceReference_SourceMobSlotID";
		case PID_SourceReference_ChannelIDs:
			return "PID_SourceReference_ChannelIDs";
		case PID_SourceReference_MonoSourceSlotIDs:
			return "PID_SourceReference_MonoSourceSlotIDs";
		case PID_SourceClip_StartTime:
			return "PID_SourceClip_StartTime";
		case PID_SourceClip_FadeInLength:
			return "PID_SourceClip_FadeInLength";
		case PID_SourceClip_FadeInType:
			return "PID_SourceClip_FadeInType";
		case PID_SourceClip_FadeOutLength:
			return "PID_SourceClip_FadeOutLength";
		case PID_SourceClip_FadeOutType:
			return "PID_SourceClip_FadeOutType";
		case PID_HTMLClip_BeginAnchor:
			return "PID_HTMLClip_BeginAnchor";
		case PID_HTMLClip_EndAnchor:
			return "PID_HTMLClip_EndAnchor";
		case PID_Timecode_Start:
			return "PID_Timecode_Start";
		case PID_Timecode_FPS:
			return "PID_Timecode_FPS";
		case PID_Timecode_Drop:
			return "PID_Timecode_Drop";
		case PID_TimecodeStream_SampleRate:
			return "PID_TimecodeStream_SampleRate";
		case PID_TimecodeStream_Source:
			return "PID_TimecodeStream_Source";
		case PID_TimecodeStream_SourceType:
			return "PID_TimecodeStream_SourceType";
		case PID_TimecodeStream12M_IncludeSync:
			return "PID_TimecodeStream12M_IncludeSync";
		case PID_Transition_OperationGroup:
			return "PID_Transition_OperationGroup";
		case PID_Transition_CutPoint:
			return "PID_Transition_CutPoint";
		case PID_ContentStorage_Mobs:
			return "PID_ContentStorage_Mobs";
		case PID_ContentStorage_EssenceData:
			return "PID_ContentStorage_EssenceData";
		case PID_ControlPoint_Value:
			return "PID_ControlPoint_Value";
		case PID_ControlPoint_Time:
			return "PID_ControlPoint_Time";
		case PID_ControlPoint_EditHint:
			return "PID_ControlPoint_EditHint";
		case PID_DefinitionObject_Identification:
			return "PID_DefinitionObject_Identification";
		case PID_DefinitionObject_Name:
			return "PID_DefinitionObject_Name";
		case PID_DefinitionObject_Description:
			return "PID_DefinitionObject_Description";
		case PID_OperationDefinition_DataDefinition:
			return "PID_OperationDefinition_DataDefinition";
		case PID_OperationDefinition_IsTimeWarp:
			return "PID_OperationDefinition_IsTimeWarp";
		case PID_OperationDefinition_DegradeTo:
			return "PID_OperationDefinition_DegradeTo";
		case PID_OperationDefinition_OperationCategory:
			return "PID_OperationDefinition_OperationCategory";
		case PID_OperationDefinition_NumberInputs:
			return "PID_OperationDefinition_NumberInputs";
		case PID_OperationDefinition_Bypass:
			return "PID_OperationDefinition_Bypass";
		case PID_OperationDefinition_ParametersDefined:
			return "PID_OperationDefinition_ParametersDefined";
		case PID_ParameterDefinition_Type:
			return "PID_ParameterDefinition_Type";
		case PID_ParameterDefinition_DisplayUnits:
			return "PID_ParameterDefinition_DisplayUnits";
		case PID_PluginDefinition_PluginCategory:
			return "PID_PluginDefinition_PluginCategory";
		case PID_PluginDefinition_VersionNumber:
			return "PID_PluginDefinition_VersionNumber";
		case PID_PluginDefinition_VersionString:
			return "PID_PluginDefinition_VersionString";
		case PID_PluginDefinition_Manufacturer:
			return "PID_PluginDefinition_Manufacturer";
		case PID_PluginDefinition_ManufacturerInfo:
			return "PID_PluginDefinition_ManufacturerInfo";
		case PID_PluginDefinition_ManufacturerID:
			return "PID_PluginDefinition_ManufacturerID";
		case PID_PluginDefinition_Platform:
			return "PID_PluginDefinition_Platform";
		case PID_PluginDefinition_MinPlatformVersion:
			return "PID_PluginDefinition_MinPlatformVersion";
		case PID_PluginDefinition_MaxPlatformVersion:
			return "PID_PluginDefinition_MaxPlatformVersion";
		case PID_PluginDefinition_Engine:
			return "PID_PluginDefinition_Engine";
		case PID_PluginDefinition_MinEngineVersion:
			return "PID_PluginDefinition_MinEngineVersion";
		case PID_PluginDefinition_MaxEngineVersion:
			return "PID_PluginDefinition_MaxEngineVersion";
		case PID_PluginDefinition_PluginAPI:
			return "PID_PluginDefinition_PluginAPI";
		case PID_PluginDefinition_MinPluginAPI:
			return "PID_PluginDefinition_MinPluginAPI";
		case PID_PluginDefinition_MaxPluginAPI:
			return "PID_PluginDefinition_MaxPluginAPI";
		case PID_PluginDefinition_SoftwareOnly:
			return "PID_PluginDefinition_SoftwareOnly";
		case PID_PluginDefinition_Accelerator:
			return "PID_PluginDefinition_Accelerator";
		case PID_PluginDefinition_Locators:
			return "PID_PluginDefinition_Locators";
		case PID_PluginDefinition_Authentication:
			return "PID_PluginDefinition_Authentication";
		case PID_PluginDefinition_DefinitionObject:
			return "PID_PluginDefinition_DefinitionObject";
		case PID_CodecDefinition_FileDescriptorClass:
			return "PID_CodecDefinition_FileDescriptorClass";
		case PID_CodecDefinition_DataDefinitions:
			return "PID_CodecDefinition_DataDefinitions";
		case PID_ContainerDefinition_EssenceIsIdentified:
			return "PID_ContainerDefinition_EssenceIsIdentified";
		case PID_Dictionary_OperationDefinitions:
			return "PID_Dictionary_OperationDefinitions";
		case PID_Dictionary_ParameterDefinitions:
			return "PID_Dictionary_ParameterDefinitions";
		case PID_Dictionary_DataDefinitions:
			return "PID_Dictionary_DataDefinitions";
		case PID_Dictionary_PluginDefinitions:
			return "PID_Dictionary_PluginDefinitions";
		case PID_Dictionary_CodecDefinitions:
			return "PID_Dictionary_CodecDefinitions";
		case PID_Dictionary_ContainerDefinitions:
			return "PID_Dictionary_ContainerDefinitions";
		case PID_Dictionary_InterpolationDefinitions:
			return "PID_Dictionary_InterpolationDefinitions";
		case PID_Dictionary_KLVDataDefinitions:
			return "PID_Dictionary_KLVDataDefinitions";
		case PID_Dictionary_TaggedValueDefinitions:
			return "PID_Dictionary_TaggedValueDefinitions";
		case PID_EssenceData_MobID:
			return "PID_EssenceData_MobID";
		case PID_EssenceData_Data:
			return "PID_EssenceData_Data";
		case PID_EssenceData_SampleIndex:
			return "PID_EssenceData_SampleIndex";
		case PID_EssenceDescriptor_Locator:
			return "PID_EssenceDescriptor_Locator";
		case PID_FileDescriptor_SampleRate:
			return "PID_FileDescriptor_SampleRate";
		case PID_FileDescriptor_Length:
			return "PID_FileDescriptor_Length";
		case PID_FileDescriptor_ContainerFormat:
			return "PID_FileDescriptor_ContainerFormat";
		case PID_FileDescriptor_CodecDefinition:
			return "PID_FileDescriptor_CodecDefinition";
		case PID_FileDescriptor_LinkedSlotID:
			return "PID_FileDescriptor_LinkedSlotID";
		case PID_AIFCDescriptor_Summary:
			return "PID_AIFCDescriptor_Summary";
		case PID_DigitalImageDescriptor_Compression:
			return "PID_DigitalImageDescriptor_Compression";
		case PID_DigitalImageDescriptor_StoredHeight:
			return "PID_DigitalImageDescriptor_StoredHeight";
		case PID_DigitalImageDescriptor_StoredWidth:
			return "PID_DigitalImageDescriptor_StoredWidth";
		case PID_DigitalImageDescriptor_SampledHeight:
			return "PID_DigitalImageDescriptor_SampledHeight";
		case PID_DigitalImageDescriptor_SampledWidth:
			return "PID_DigitalImageDescriptor_SampledWidth";
		case PID_DigitalImageDescriptor_SampledXOffset:
			return "PID_DigitalImageDescriptor_SampledXOffset";
		case PID_DigitalImageDescriptor_SampledYOffset:
			return "PID_DigitalImageDescriptor_SampledYOffset";
		case PID_DigitalImageDescriptor_DisplayHeight:
			return "PID_DigitalImageDescriptor_DisplayHeight";
		case PID_DigitalImageDescriptor_DisplayWidth:
			return "PID_DigitalImageDescriptor_DisplayWidth";
		case PID_DigitalImageDescriptor_DisplayXOffset:
			return "PID_DigitalImageDescriptor_DisplayXOffset";
		case PID_DigitalImageDescriptor_DisplayYOffset:
			return "PID_DigitalImageDescriptor_DisplayYOffset";
		case PID_DigitalImageDescriptor_FrameLayout:
			return "PID_DigitalImageDescriptor_FrameLayout";
		case PID_DigitalImageDescriptor_VideoLineMap:
			return "PID_DigitalImageDescriptor_VideoLineMap";
		case PID_DigitalImageDescriptor_ImageAspectRatio:
			return "PID_DigitalImageDescriptor_ImageAspectRatio";
		case PID_DigitalImageDescriptor_AlphaTransparency:
			return "PID_DigitalImageDescriptor_AlphaTransparency";
		case PID_DigitalImageDescriptor_TransferCharacteristic:
			return "PID_DigitalImageDescriptor_TransferCharacteristic";
		case PID_DigitalImageDescriptor_ColorPrimaries:
			return "PID_DigitalImageDescriptor_ColorPrimaries";
		case PID_DigitalImageDescriptor_CodingEquations:
			return "PID_DigitalImageDescriptor_CodingEquations";
		case PID_DigitalImageDescriptor_ImageAlignmentFactor:
			return "PID_DigitalImageDescriptor_ImageAlignmentFactor";
		case PID_DigitalImageDescriptor_FieldDominance:
			return "PID_DigitalImageDescriptor_FieldDominance";
		case PID_DigitalImageDescriptor_FieldStartOffset:
			return "PID_DigitalImageDescriptor_FieldStartOffset";
		case PID_DigitalImageDescriptor_FieldEndOffset:
			return "PID_DigitalImageDescriptor_FieldEndOffset";
		case PID_DigitalImageDescriptor_SignalStandard:
			return "PID_DigitalImageDescriptor_SignalStandard";
		case PID_DigitalImageDescriptor_StoredF2Offset:
			return "PID_DigitalImageDescriptor_StoredF2Offset";
		case PID_DigitalImageDescriptor_DisplayF2Offset:
			return "PID_DigitalImageDescriptor_DisplayF2Offset";
		case PID_DigitalImageDescriptor_ActiveFormatDescriptor:
			return "PID_DigitalImageDescriptor_ActiveFormatDescriptor";
		case PID_CDCIDescriptor_ComponentWidth:
			return "PID_CDCIDescriptor_ComponentWidth";
		case PID_CDCIDescriptor_HorizontalSubsampling:
			return "PID_CDCIDescriptor_HorizontalSubsampling";
		case PID_CDCIDescriptor_ColorSiting:
			return "PID_CDCIDescriptor_ColorSiting";
		case PID_CDCIDescriptor_BlackReferenceLevel:
			return "PID_CDCIDescriptor_BlackReferenceLevel";
		case PID_CDCIDescriptor_WhiteReferenceLevel:
			return "PID_CDCIDescriptor_WhiteReferenceLevel";
		case PID_CDCIDescriptor_ColorRange:
			return "PID_CDCIDescriptor_ColorRange";
		case PID_CDCIDescriptor_PaddingBits:
			return "PID_CDCIDescriptor_PaddingBits";
		case PID_CDCIDescriptor_VerticalSubsampling:
			return "PID_CDCIDescriptor_VerticalSubsampling";
		case PID_CDCIDescriptor_AlphaSamplingWidth:
			return "PID_CDCIDescriptor_AlphaSamplingWidth";
		case PID_CDCIDescriptor_ReversedByteOrder:
			return "PID_CDCIDescriptor_ReversedByteOrder";
		case PID_RGBADescriptor_PixelLayout:
			return "PID_RGBADescriptor_PixelLayout";
		case PID_RGBADescriptor_Palette:
			return "PID_RGBADescriptor_Palette";
		case PID_RGBADescriptor_PaletteLayout:
			return "PID_RGBADescriptor_PaletteLayout";
		case PID_RGBADescriptor_ScanningDirection:
			return "PID_RGBADescriptor_ScanningDirection";
		case PID_RGBADescriptor_ComponentMaxRef:
			return "PID_RGBADescriptor_ComponentMaxRef";
		case PID_RGBADescriptor_ComponentMinRef:
			return "PID_RGBADescriptor_ComponentMinRef";
		case PID_RGBADescriptor_AlphaMaxRef:
			return "PID_RGBADescriptor_AlphaMaxRef";
		case PID_RGBADescriptor_AlphaMinRef:
			return "PID_RGBADescriptor_AlphaMinRef";
		case PID_TIFFDescriptor_IsUniform:
			return "PID_TIFFDescriptor_IsUniform";
		case PID_TIFFDescriptor_IsContiguous:
			return "PID_TIFFDescriptor_IsContiguous";
		case PID_TIFFDescriptor_LeadingLines:
			return "PID_TIFFDescriptor_LeadingLines";
		case PID_TIFFDescriptor_TrailingLines:
			return "PID_TIFFDescriptor_TrailingLines";
		case PID_TIFFDescriptor_JPEGTableID:
			return "PID_TIFFDescriptor_JPEGTableID";
		case PID_TIFFDescriptor_Summary:
			return "PID_TIFFDescriptor_Summary";
		case PID_WAVEDescriptor_Summary:
			return "PID_WAVEDescriptor_Summary";
		case PID_FilmDescriptor_FilmFormat:
			return "PID_FilmDescriptor_FilmFormat";
		case PID_FilmDescriptor_FrameRate:
			return "PID_FilmDescriptor_FrameRate";
		case PID_FilmDescriptor_PerforationsPerFrame:
			return "PID_FilmDescriptor_PerforationsPerFrame";
		case PID_FilmDescriptor_FilmAspectRatio:
			return "PID_FilmDescriptor_FilmAspectRatio";
		case PID_FilmDescriptor_Manufacturer:
			return "PID_FilmDescriptor_Manufacturer";
		case PID_FilmDescriptor_Model:
			return "PID_FilmDescriptor_Model";
		case PID_FilmDescriptor_FilmGaugeFormat:
			return "PID_FilmDescriptor_FilmGaugeFormat";
		case PID_FilmDescriptor_FilmBatchNumber:
			return "PID_FilmDescriptor_FilmBatchNumber";
		case PID_TapeDescriptor_FormFactor:
			return "PID_TapeDescriptor_FormFactor";
		case PID_TapeDescriptor_VideoSignal:
			return "PID_TapeDescriptor_VideoSignal";
		case PID_TapeDescriptor_TapeFormat:
			return "PID_TapeDescriptor_TapeFormat";
		case PID_TapeDescriptor_Length:
			return "PID_TapeDescriptor_Length";
		case PID_TapeDescriptor_ManufacturerID:
			return "PID_TapeDescriptor_ManufacturerID";
		case PID_TapeDescriptor_Model:
			return "PID_TapeDescriptor_Model";
		case PID_TapeDescriptor_TapeBatchNumber:
			return "PID_TapeDescriptor_TapeBatchNumber";
		case PID_TapeDescriptor_TapeStock:
			return "PID_TapeDescriptor_TapeStock";
		case PID_Header_ByteOrder:
			return "PID_Header_ByteOrder";
		case PID_Header_LastModified:
			return "PID_Header_LastModified";
		case PID_Header_Content:
			return "PID_Header_Content";
		case PID_Header_Dictionary:
			return "PID_Header_Dictionary";
		case PID_Header_Version:
			return "PID_Header_Version";
		case PID_Header_IdentificationList:
			return "PID_Header_IdentificationList";
		case PID_Header_ObjectModelVersion:
			return "PID_Header_ObjectModelVersion";
		case PID_Header_OperationalPattern:
			return "PID_Header_OperationalPattern";
		case PID_Header_EssenceContainers:
			return "PID_Header_EssenceContainers";
		case PID_Header_DescriptiveSchemes:
			return "PID_Header_DescriptiveSchemes";
		case PID_Identification_CompanyName:
			return "PID_Identification_CompanyName";
		case PID_Identification_ProductName:
			return "PID_Identification_ProductName";
		case PID_Identification_ProductVersion:
			return "PID_Identification_ProductVersion";
		case PID_Identification_ProductVersionString:
			return "PID_Identification_ProductVersionString";
		case PID_Identification_ProductID:
			return "PID_Identification_ProductID";
		case PID_Identification_Date:
			return "PID_Identification_Date";
		case PID_Identification_ToolkitVersion:
			return "PID_Identification_ToolkitVersion";
		case PID_Identification_Platform:
			return "PID_Identification_Platform";
		case PID_Identification_GenerationAUID:
			return "PID_Identification_GenerationAUID";
		case PID_NetworkLocator_URLString:
			return "PID_NetworkLocator_URLString";
		case PID_TextLocator_Name:
			return "PID_TextLocator_Name";
		case PID_Mob_MobID:
			return "PID_Mob_MobID";
		case PID_Mob_Name:
			return "PID_Mob_Name";
		case PID_Mob_Slots:
			return "PID_Mob_Slots";
		case PID_Mob_LastModified:
			return "PID_Mob_LastModified";
		case PID_Mob_CreationTime:
			return "PID_Mob_CreationTime";
		case PID_Mob_UserComments:
			return "PID_Mob_UserComments";
		case PID_Mob_KLVData:
			return "PID_Mob_KLVData";
		case PID_Mob_Attributes:
			return "PID_Mob_Attributes";
		case PID_Mob_UsageCode:
			return "PID_Mob_UsageCode";
		case PID_CompositionMob_DefaultFadeLength:
			return "PID_CompositionMob_DefaultFadeLength";
		case PID_CompositionMob_DefFadeType:
			return "PID_CompositionMob_DefFadeType";
		case PID_CompositionMob_DefFadeEditUnit:
			return "PID_CompositionMob_DefFadeEditUnit";
		case PID_CompositionMob_Rendering:
			return "PID_CompositionMob_Rendering";
		case PID_SourceMob_EssenceDescription:
			return "PID_SourceMob_EssenceDescription";
		case PID_MobSlot_SlotID:
			return "PID_MobSlot_SlotID";
		case PID_MobSlot_SlotName:
			return "PID_MobSlot_SlotName";
		case PID_MobSlot_Segment:
			return "PID_MobSlot_Segment";
		case PID_MobSlot_PhysicalTrackNumber:
			return "PID_MobSlot_PhysicalTrackNumber";
		case PID_EventMobSlot_EditRate:
			return "PID_EventMobSlot_EditRate";
		case PID_EventMobSlot_EventSlotOrigin:
			return "PID_EventMobSlot_EventSlotOrigin";
		case PID_TimelineMobSlot_EditRate:
			return "PID_TimelineMobSlot_EditRate";
		case PID_TimelineMobSlot_Origin:
			return "PID_TimelineMobSlot_Origin";
		case PID_TimelineMobSlot_MarkIn:
			return "PID_TimelineMobSlot_MarkIn";
		case PID_TimelineMobSlot_MarkOut:
			return "PID_TimelineMobSlot_MarkOut";
		case PID_TimelineMobSlot_UserPos:
			return "PID_TimelineMobSlot_UserPos";
		case PID_Parameter_Definition:
			return "PID_Parameter_Definition";
		case PID_ConstantValue_Value:
			return "PID_ConstantValue_Value";
		case PID_VaryingValue_Interpolation:
			return "PID_VaryingValue_Interpolation";
		case PID_VaryingValue_PointList:
			return "PID_VaryingValue_PointList";
		case PID_TaggedValue_Name:
			return "PID_TaggedValue_Name";
		case PID_TaggedValue_Value:
			return "PID_TaggedValue_Value";
		case PID_KLVData_Value:
			return "PID_KLVData_Value";
		case PID_DescriptiveMarker_DescribedSlots:
			return "PID_DescriptiveMarker_DescribedSlots";
		case PID_DescriptiveMarker_Description:
			return "PID_DescriptiveMarker_Description";
		case PID_SoundDescriptor_AudioSamplingRate:
			return "PID_SoundDescriptor_AudioSamplingRate";
		case PID_SoundDescriptor_Locked:
			return "PID_SoundDescriptor_Locked";
		case PID_SoundDescriptor_AudioRefLevel:
			return "PID_SoundDescriptor_AudioRefLevel";
		case PID_SoundDescriptor_ElectroSpatial:
			return "PID_SoundDescriptor_ElectroSpatial";
		case PID_SoundDescriptor_Channels:
			return "PID_SoundDescriptor_Channels";
		case PID_SoundDescriptor_QuantizationBits:
			return "PID_SoundDescriptor_QuantizationBits";
		case PID_SoundDescriptor_DialNorm:
			return "PID_SoundDescriptor_DialNorm";
		case PID_SoundDescriptor_Compression:
			return "PID_SoundDescriptor_Compression";
		case PID_DataEssenceDescriptor_DataEssenceCoding:
			return "PID_DataEssenceDescriptor_DataEssenceCoding";
		case PID_MultipleDescriptor_FileDescriptors:
			return "PID_MultipleDescriptor_FileDescriptors";
		case PID_DescriptiveClip_DescribedSlotIDs:
			return "PID_DescriptiveClip_DescribedSlotIDs";
		case PID_AES3PCMDescriptor_Emphasis:
			return "PID_AES3PCMDescriptor_Emphasis";
		case PID_AES3PCMDescriptor_BlockStartOffset:
			return "PID_AES3PCMDescriptor_BlockStartOffset";
		case PID_AES3PCMDescriptor_AuxBitsMode:
			return "PID_AES3PCMDescriptor_AuxBitsMode";
		case PID_AES3PCMDescriptor_ChannelStatusMode:
			return "PID_AES3PCMDescriptor_ChannelStatusMode";
		case PID_AES3PCMDescriptor_FixedChannelStatusData:
			return "PID_AES3PCMDescriptor_FixedChannelStatusData";
		case PID_AES3PCMDescriptor_UserDataMode:
			return "PID_AES3PCMDescriptor_UserDataMode";
		case PID_AES3PCMDescriptor_FixedUserData:
			return "PID_AES3PCMDescriptor_FixedUserData";
		case PID_PCMDescriptor_BlockAlign:
			return "PID_PCMDescriptor_BlockAlign";
		case PID_PCMDescriptor_SequenceOffset:
			return "PID_PCMDescriptor_SequenceOffset";
		case PID_PCMDescriptor_AverageBPS:
			return "PID_PCMDescriptor_AverageBPS";
		case PID_PCMDescriptor_ChannelAssignment:
			return "PID_PCMDescriptor_ChannelAssignment";
		case PID_PCMDescriptor_PeakEnvelopeVersion:
			return "PID_PCMDescriptor_PeakEnvelopeVersion";
		case PID_PCMDescriptor_PeakEnvelopeFormat:
			return "PID_PCMDescriptor_PeakEnvelopeFormat";
		case PID_PCMDescriptor_PointsPerPeakValue:
			return "PID_PCMDescriptor_PointsPerPeakValue";
		case PID_PCMDescriptor_PeakEnvelopeBlockSize:
			return "PID_PCMDescriptor_PeakEnvelopeBlockSize";
		case PID_PCMDescriptor_PeakChannels:
			return "PID_PCMDescriptor_PeakChannels";
		case PID_PCMDescriptor_PeakFrames:
			return "PID_PCMDescriptor_PeakFrames";
		case PID_PCMDescriptor_PeakOfPeaksPosition:
			return "PID_PCMDescriptor_PeakOfPeaksPosition";
		case PID_PCMDescriptor_PeakEnvelopeTimestamp:
			return "PID_PCMDescriptor_PeakEnvelopeTimestamp";
		case PID_PCMDescriptor_PeakEnvelopeData:
			return "PID_PCMDescriptor_PeakEnvelopeData";
		case PID_KLVDataDefinition_KLVDataType:
			return "PID_KLVDataDefinition_KLVDataType";
		case PID_AuxiliaryDescriptor_MimeType:
			return "PID_AuxiliaryDescriptor_MimeType";
		case PID_AuxiliaryDescriptor_CharSet:
			return "PID_AuxiliaryDescriptor_CharSet";
		case PID_RIFFChunk_ChunkID:
			return "PID_RIFFChunk_ChunkID";
		case PID_RIFFChunk_ChunkLength:
			return "PID_RIFFChunk_ChunkLength";
		case PID_RIFFChunk_ChunkData:
			return "PID_RIFFChunk_ChunkData";
		case PID_BWFImportDescriptor_QltyFileSecurityReport:
			return "PID_BWFImportDescriptor_QltyFileSecurityReport";
		case PID_BWFImportDescriptor_QltyFileSecurityWave:
			return "PID_BWFImportDescriptor_QltyFileSecurityWave";
		case PID_BWFImportDescriptor_BextCodingHistory:
			return "PID_BWFImportDescriptor_BextCodingHistory";
		case PID_BWFImportDescriptor_QltyBasicData:
			return "PID_BWFImportDescriptor_QltyBasicData";
		case PID_BWFImportDescriptor_QltyStartOfModulation:
			return "PID_BWFImportDescriptor_QltyStartOfModulation";
		case PID_BWFImportDescriptor_QltyQualityEvent:
			return "PID_BWFImportDescriptor_QltyQualityEvent";
		case PID_BWFImportDescriptor_QltyEndOfModulation:
			return "PID_BWFImportDescriptor_QltyEndOfModulation";
		case PID_BWFImportDescriptor_QltyQualityParameter:
			return "PID_BWFImportDescriptor_QltyQualityParameter";
		case PID_BWFImportDescriptor_QltyOperatorComment:
			return "PID_BWFImportDescriptor_QltyOperatorComment";
		case PID_BWFImportDescriptor_QltyCueSheet:
			return "PID_BWFImportDescriptor_QltyCueSheet";
		case PID_BWFImportDescriptor_UnknownBWFChunks:
			return "PID_BWFImportDescriptor_UnknownBWFChunks";

			/* the following is marked as "dynamic" in ref implementation :
		 * AAF/ref-impl/include/ref-api/AAFTypes.h
		 *
		 * case PID_MPEGVideoDescriptor_SingleSequence:
		 * case PID_MPEGVideoDescriptor_ConstantBPictureCount:
		 * case PID_MPEGVideoDescriptor_CodedContentScanning:
		 * case PID_MPEGVideoDescriptor_LowDelay:
		 * case PID_MPEGVideoDescriptor_ClosedGOP:
		 * case PID_MPEGVideoDescriptor_IdenticalGOP:
		 * case PID_MPEGVideoDescriptor_MaxGOP:
		 * case PID_MPEGVideoDescriptor_MaxBPictureCount:
		 * case PID_MPEGVideoDescriptor_BitRate:
		 * case PID_MPEGVideoDescriptor_ProfileAndLevel:
		 */

		case PID_ClassDefinition_ParentClass:
			return "PID_ClassDefinition_ParentClass";
		case PID_ClassDefinition_Properties:
			return "PID_ClassDefinition_Properties";
		case PID_ClassDefinition_IsConcrete:
			return "PID_ClassDefinition_IsConcrete";
		case PID_PropertyDefinition_Type:
			return "PID_PropertyDefinition_Type";
		case PID_PropertyDefinition_IsOptional:
			return "PID_PropertyDefinition_IsOptional";
		case PID_PropertyDefinition_LocalIdentification:
			return "PID_PropertyDefinition_LocalIdentification";
		case PID_PropertyDefinition_IsUniqueIdentifier:
			return "PID_PropertyDefinition_IsUniqueIdentifier";
		case PID_TypeDefinitionInteger_Size:
			return "PID_TypeDefinitionInteger_Size";
		case PID_TypeDefinitionInteger_IsSigned:
			return "PID_TypeDefinitionInteger_IsSigned";
		case PID_TypeDefinitionStrongObjectReference_ReferencedType:
			return "PID_TypeDefinitionStrongObjectReference_ReferencedType";
		case PID_TypeDefinitionWeakObjectReference_ReferencedType:
			return "PID_TypeDefinitionWeakObjectReference_ReferencedType";
		case PID_TypeDefinitionWeakObjectReference_TargetSet:
			return "PID_TypeDefinitionWeakObjectReference_TargetSet";
		case PID_TypeDefinitionEnumeration_ElementType:
			return "PID_TypeDefinitionEnumeration_ElementType";
		case PID_TypeDefinitionEnumeration_ElementNames:
			return "PID_TypeDefinitionEnumeration_ElementNames";
		case PID_TypeDefinitionEnumeration_ElementValues:
			return "PID_TypeDefinitionEnumeration_ElementValues";
		case PID_TypeDefinitionFixedArray_ElementType:
			return "PID_TypeDefinitionFixedArray_ElementType";
		case PID_TypeDefinitionFixedArray_ElementCount:
			return "PID_TypeDefinitionFixedArray_ElementCount";
		case PID_TypeDefinitionVariableArray_ElementType:
			return "PID_TypeDefinitionVariableArray_ElementType";
		case PID_TypeDefinitionSet_ElementType:
			return "PID_TypeDefinitionSet_ElementType";
		case PID_TypeDefinitionString_ElementType:
			return "PID_TypeDefinitionString_ElementType";
		case PID_TypeDefinitionRecord_MemberTypes:
			return "PID_TypeDefinitionRecord_MemberTypes";
		case PID_TypeDefinitionRecord_MemberNames:
			return "PID_TypeDefinitionRecord_MemberNames";
		case PID_TypeDefinitionRename_RenamedType:
			return "PID_TypeDefinitionRename_RenamedType";
		case PID_TypeDefinitionExtendibleEnumeration_ElementNames:
			return "PID_TypeDefinitionExtendibleEnumeration_ElementNames";
		case PID_TypeDefinitionExtendibleEnumeration_ElementValues:
			return "PID_TypeDefinitionExtendibleEnumeration_ElementValues";
		case PID_MetaDefinition_Identification:
			return "PID_MetaDefinition_Identification";
		case PID_MetaDefinition_Name:
			return "PID_MetaDefinition_Name";
		case PID_MetaDefinition_Description:
			return "PID_MetaDefinition_Description";
		case PID_MetaDictionary_ClassDefinitions:
			return "PID_MetaDictionary_ClassDefinitions";
		case PID_MetaDictionary_TypeDefinitions:
			return "PID_MetaDictionary_TypeDefinitions";
	}

	static char PIDText[1024];

	aafClass* Class = NULL;

	foreachClass (Class, aafd->Classes)
	{
		aafPropertyDef* PDef = NULL;

		foreachPropertyDefinition (PDef, Class->Properties)
		{
			if (PDef->pid == pid) {
				int rc = snprintf (PIDText, sizeof (PIDText), "%s%s%s",
				                   (PDef->meta) ? ANSI_COLOR_MAGENTA (aafd->log) : "",
				                   PDef->name,
				                   (PDef->meta) ? ANSI_COLOR_RESET (aafd->log) : "");

				assert (rc >= 0 && (size_t)rc < sizeof (PIDText));

				return PIDText;
			}
		}
	}

	return "Unknown PID_MetaDictionary";
}

const char*
aaft_ClassIDToText (AAF_Data* aafd, const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AUID_NULL))
		return "AUID_NULL";
	if (aafUIDCmp (auid, &AAFClassID_Root))
		return "AAFClassID_Root";
	if (aafUIDCmp (auid, &AAFClassID_InterchangeObject))
		return "AAFClassID_InterchangeObject";
	if (aafUIDCmp (auid, &AAFClassID_Component))
		return "AAFClassID_Component";
	if (aafUIDCmp (auid, &AAFClassID_Segment))
		return "AAFClassID_Segment";
	if (aafUIDCmp (auid, &AAFClassID_EdgeCode))
		return "AAFClassID_EdgeCode";
	if (aafUIDCmp (auid, &AAFClassID_EssenceGroup))
		return "AAFClassID_EssenceGroup";
	if (aafUIDCmp (auid, &AAFClassID_Event))
		return "AAFClassID_Event";
	if (aafUIDCmp (auid, &AAFClassID_GPITrigger))
		return "AAFClassID_GPITrigger";
	if (aafUIDCmp (auid, &AAFClassID_CommentMarker))
		return "AAFClassID_CommentMarker";
	if (aafUIDCmp (auid, &AAFClassID_Filler))
		return "AAFClassID_Filler";
	if (aafUIDCmp (auid, &AAFClassID_OperationGroup))
		return "AAFClassID_OperationGroup";
	if (aafUIDCmp (auid, &AAFClassID_NestedScope))
		return "AAFClassID_NestedScope";
	if (aafUIDCmp (auid, &AAFClassID_Pulldown))
		return "AAFClassID_Pulldown";
	if (aafUIDCmp (auid, &AAFClassID_ScopeReference))
		return "AAFClassID_ScopeReference";
	if (aafUIDCmp (auid, &AAFClassID_Selector))
		return "AAFClassID_Selector";
	if (aafUIDCmp (auid, &AAFClassID_Sequence))
		return "AAFClassID_Sequence";
	if (aafUIDCmp (auid, &AAFClassID_SourceReference))
		return "AAFClassID_SourceReference";
	if (aafUIDCmp (auid, &AAFClassID_SourceClip))
		return "AAFClassID_SourceClip";
	if (aafUIDCmp (auid, &AAFClassID_TextClip))
		return "AAFClassID_TextClip";
	if (aafUIDCmp (auid, &AAFClassID_HTMLClip))
		return "AAFClassID_HTMLClip";
	if (aafUIDCmp (auid, &AAFClassID_Timecode))
		return "AAFClassID_Timecode";
	if (aafUIDCmp (auid, &AAFClassID_TimecodeStream))
		return "AAFClassID_TimecodeStream";
	if (aafUIDCmp (auid, &AAFClassID_TimecodeStream12M))
		return "AAFClassID_TimecodeStream12M";
	if (aafUIDCmp (auid, &AAFClassID_Transition))
		return "AAFClassID_Transition";
	if (aafUIDCmp (auid, &AAFClassID_ContentStorage))
		return "AAFClassID_ContentStorage";
	if (aafUIDCmp (auid, &AAFClassID_ControlPoint))
		return "AAFClassID_ControlPoint";
	if (aafUIDCmp (auid, &AAFClassID_DefinitionObject))
		return "AAFClassID_DefinitionObject";
	if (aafUIDCmp (auid, &AAFClassID_DataDefinition))
		return "AAFClassID_DataDefinition";
	if (aafUIDCmp (auid, &AAFClassID_OperationDefinition))
		return "AAFClassID_OperationDefinition";
	if (aafUIDCmp (auid, &AAFClassID_ParameterDefinition))
		return "AAFClassID_ParameterDefinition";
	if (aafUIDCmp (auid, &AAFClassID_PluginDefinition))
		return "AAFClassID_PluginDefinition";
	if (aafUIDCmp (auid, &AAFClassID_CodecDefinition))
		return "AAFClassID_CodecDefinition";
	if (aafUIDCmp (auid, &AAFClassID_ContainerDefinition))
		return "AAFClassID_ContainerDefinition";
	if (aafUIDCmp (auid, &AAFClassID_InterpolationDefinition))
		return "AAFClassID_InterpolationDefinition";
	if (aafUIDCmp (auid, &AAFClassID_Dictionary))
		return "AAFClassID_Dictionary";
	if (aafUIDCmp (auid, &AAFClassID_EssenceData))
		return "AAFClassID_EssenceData";
	if (aafUIDCmp (auid, &AAFClassID_EssenceDescriptor))
		return "AAFClassID_EssenceDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_FileDescriptor))
		return "AAFClassID_FileDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_AIFCDescriptor))
		return "AAFClassID_AIFCDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_DigitalImageDescriptor))
		return "AAFClassID_DigitalImageDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_CDCIDescriptor))
		return "AAFClassID_CDCIDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_RGBADescriptor))
		return "AAFClassID_RGBADescriptor";
	if (aafUIDCmp (auid, &AAFClassID_HTMLDescriptor))
		return "AAFClassID_HTMLDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_TIFFDescriptor))
		return "AAFClassID_TIFFDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_WAVEDescriptor))
		return "AAFClassID_WAVEDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_FilmDescriptor))
		return "AAFClassID_FilmDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_TapeDescriptor))
		return "AAFClassID_TapeDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_Header))
		return "AAFClassID_Header";
	if (aafUIDCmp (auid, &AAFClassID_Identification))
		return "AAFClassID_Identification";
	if (aafUIDCmp (auid, &AAFClassID_Locator))
		return "AAFClassID_Locator";
	if (aafUIDCmp (auid, &AAFClassID_NetworkLocator))
		return "AAFClassID_NetworkLocator";
	if (aafUIDCmp (auid, &AAFClassID_TextLocator))
		return "AAFClassID_TextLocator";
	if (aafUIDCmp (auid, &AAFClassID_Mob))
		return "AAFClassID_Mob";
	if (aafUIDCmp (auid, &AAFClassID_CompositionMob))
		return "AAFClassID_CompositionMob";
	if (aafUIDCmp (auid, &AAFClassID_MasterMob))
		return "AAFClassID_MasterMob";
	if (aafUIDCmp (auid, &AAFClassID_SourceMob))
		return "AAFClassID_SourceMob";
	if (aafUIDCmp (auid, &AAFClassID_MobSlot))
		return "AAFClassID_MobSlot";
	if (aafUIDCmp (auid, &AAFClassID_EventMobSlot))
		return "AAFClassID_EventMobSlot";
	if (aafUIDCmp (auid, &AAFClassID_StaticMobSlot))
		return "AAFClassID_StaticMobSlot";
	if (aafUIDCmp (auid, &AAFClassID_TimelineMobSlot))
		return "AAFClassID_TimelineMobSlot";
	if (aafUIDCmp (auid, &AAFClassID_Parameter))
		return "AAFClassID_Parameter";
	if (aafUIDCmp (auid, &AAFClassID_ConstantValue))
		return "AAFClassID_ConstantValue";
	if (aafUIDCmp (auid, &AAFClassID_VaryingValue))
		return "AAFClassID_VaryingValue";
	if (aafUIDCmp (auid, &AAFClassID_TaggedValue))
		return "AAFClassID_TaggedValue";
	if (aafUIDCmp (auid, &AAFClassID_KLVData))
		return "AAFClassID_KLVData";
	if (aafUIDCmp (auid, &AAFClassID_DescriptiveMarker))
		return "AAFClassID_DescriptiveMarker";
	if (aafUIDCmp (auid, &AAFClassID_SoundDescriptor))
		return "AAFClassID_SoundDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_DataEssenceDescriptor))
		return "AAFClassID_DataEssenceDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_MultipleDescriptor))
		return "AAFClassID_MultipleDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_DescriptiveClip))
		return "AAFClassID_DescriptiveClip";
	if (aafUIDCmp (auid, &AAFClassID_AES3PCMDescriptor))
		return "AAFClassID_AES3PCMDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_PCMDescriptor))
		return "AAFClassID_PCMDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_PhysicalDescriptor))
		return "AAFClassID_PhysicalDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_ImportDescriptor))
		return "AAFClassID_ImportDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_RecordingDescriptor))
		return "AAFClassID_RecordingDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_TaggedValueDefinition))
		return "AAFClassID_TaggedValueDefinition";
	if (aafUIDCmp (auid, &AAFClassID_KLVDataDefinition))
		return "AAFClassID_KLVDataDefinition";
	if (aafUIDCmp (auid, &AAFClassID_AuxiliaryDescriptor))
		return "AAFClassID_AuxiliaryDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_RIFFChunk))
		return "AAFClassID_RIFFChunk";
	if (aafUIDCmp (auid, &AAFClassID_BWFImportDescriptor))
		return "AAFClassID_BWFImportDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_MPEGVideoDescriptor))
		return "AAFClassID_MPEGVideoDescriptor";
	if (aafUIDCmp (auid, &AAFClassID_ClassDefinition))
		return "AAFClassID_ClassDefinition";
	if (aafUIDCmp (auid, &AAFClassID_PropertyDefinition))
		return "AAFClassID_PropertyDefinition";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinition))
		return "AAFClassID_TypeDefinition";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionInteger))
		return "AAFClassID_TypeDefinitionInteger";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionStrongObjectReference))
		return "AAFClassID_TypeDefinitionStrongObjectReference";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionWeakObjectReference))
		return "AAFClassID_TypeDefinitionWeakObjectReference";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionEnumeration))
		return "AAFClassID_TypeDefinitionEnumeration";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionFixedArray))
		return "AAFClassID_TypeDefinitionFixedArray";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionVariableArray))
		return "AAFClassID_TypeDefinitionVariableArray";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionSet))
		return "AAFClassID_TypeDefinitionSet";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionString))
		return "AAFClassID_TypeDefinitionString";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionStream))
		return "AAFClassID_TypeDefinitionStream";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionRecord))
		return "AAFClassID_TypeDefinitionRecord";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionRename))
		return "AAFClassID_TypeDefinitionRename";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionExtendibleEnumeration))
		return "AAFClassID_TypeDefinitionExtendibleEnumeration";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionIndirect))
		return "AAFClassID_TypeDefinitionIndirect";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionOpaque))
		return "AAFClassID_TypeDefinitionOpaque";
	if (aafUIDCmp (auid, &AAFClassID_TypeDefinitionCharacter))
		return "AAFClassID_TypeDefinitionCharacter";
	if (aafUIDCmp (auid, &AAFClassID_MetaDefinition))
		return "AAFClassID_MetaDefinition";
	if (aafUIDCmp (auid, &AAFClassID_MetaDictionary))
		return "AAFClassID_MetaDictionary";
	if (aafUIDCmp (auid, &AAFClassID_DescriptiveObject))
		return "AAFClassID_DescriptiveObject";
	if (aafUIDCmp (auid, &AAFClassID_DescriptiveFramework))
		return "AAFClassID_DescriptiveFramework";

	static char ClassIDText[1024];

	ClassIDText[0] = '\0';

	aafClass* Class = NULL;

	foreachClass (Class, aafd->Classes)
	{
		if (aafUIDCmp (Class->ID, auid)) {
			int rc = snprintf (ClassIDText, sizeof (ClassIDText), "%s%s%s",
			                   (Class->meta) ? ANSI_COLOR_MAGENTA (aafd->log) : "",
			                   Class->name,
			                   (Class->meta) ? ANSI_COLOR_RESET (aafd->log) : "");

			assert (rc >= 0 && (size_t)rc < sizeof (ClassIDText));

			return ClassIDText;
		}
	}

	return "Unknown AAFClassID";
}

const char*
aaft_IndirectValueToText (AAF_Data* aafd, aafIndirect_t* Indirect)
{
	static char buf[4096];

	memset (buf, 0x00, sizeof (buf));

	void* indirectValue = aaf_get_indirectValue (aafd, Indirect, NULL);

	if (!indirectValue) {
		return NULL;
	}

	int rc = 0;

	if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Boolean)) {
		rc = snprintf (buf, sizeof (buf), "%c", *(uint8_t*)indirectValue);
	} else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Rational)) {
		rc = snprintf (buf, sizeof (buf), "%i/%i", ((aafRational_t*)indirectValue)->numerator, ((aafRational_t*)indirectValue)->denominator);
	}

	else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Int8)) {
		rc = snprintf (buf, sizeof (buf), "%c", *(int8_t*)indirectValue);
	} else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Int16)) {
		rc = snprintf (buf, sizeof (buf), "%i", *(int16_t*)indirectValue);
	} else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Int32)) {
		rc = snprintf (buf, sizeof (buf), "%i", *(int32_t*)indirectValue);
	} else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_Int64)) {
		rc = snprintf (buf, sizeof (buf), "%" PRIi64, *(int64_t*)indirectValue);
	}

	else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_UInt16)) {
		rc = snprintf (buf, sizeof (buf), "%u", *(uint16_t*)indirectValue);
	} else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_UInt32)) {
		rc = snprintf (buf, sizeof (buf), "%u", *(uint32_t*)indirectValue);
	} else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_UInt64)) {
		rc = snprintf (buf, sizeof (buf), "%" PRIu64, *(uint64_t*)indirectValue);
	}

	else if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_String)) {
		char* str = aaf_get_indirectValue (aafd, Indirect, &AAFTypeID_String);

		if (!str) {
			error ("Could not retrieve Indirect value");
			return NULL;
		}

		rc = snprintf (buf, sizeof (buf), "%s", str);

		free (str);
	} else {
		error ("Unsupported Indirect value type ID : %s", aaft_TypeIDToText (&Indirect->TypeDef));
		return NULL;
	}

	assert (rc >= 0 && (size_t)rc < sizeof (buf));

	return buf;
}

const char*
aaft_ContainerToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AUID_NULL))
		return "AUID_NULL";
	if (aafUIDCmp (auid, &AAFContainerDef_External))
		return "AAFContainerDef_External";
	if (aafUIDCmp (auid, &AAFContainerDef_OMF))
		return "AAFContainerDef_OMF";
	if (aafUIDCmp (auid, &AAFContainerDef_AAF))
		return "AAFContainerDef_AAF";
	if (aafUIDCmp (auid, &AAFContainerDef_AAFMSS))
		return "AAFContainerDef_AAFMSS";
	if (aafUIDCmp (auid, &AAFContainerDef_AAFKLV))
		return "AAFContainerDef_AAFKLV";
	if (aafUIDCmp (auid, &AAFContainerDef_AAFXML))
		return "AAFContainerDef_AAFXML";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_DefinedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_DefinedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_ExtendedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_ExtendedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_PictureOnly))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_PictureOnly";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_DefinedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_DefinedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_ExtendedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_ExtendedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_PictureOnly))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_PictureOnly";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_DefinedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_DefinedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_ExtendedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_ExtendedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_PictureOnly))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_PictureOnly";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_DefinedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_DefinedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_ExtendedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_ExtendedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_PictureOnly))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_PictureOnly";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_DefinedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_DefinedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_ExtendedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_ExtendedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_PictureOnly))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_PictureOnly";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_DefinedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_DefinedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_ExtendedTemplate))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_ExtendedTemplate";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_PictureOnly))
		return "AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_PictureOnly";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_IECDV_525x5994I_25Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_IECDV_525x5994I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_IECDV_525x5994I_25Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_IECDV_525x5994I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_IECDV_625x50I_25Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_IECDV_625x50I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_IECDV_625x50I_25Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_IECDV_625x50I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_IECDV_525x5994I_25Mbps_SMPTE322M))
		return "AAFContainerDef_MXFGC_Framewrapped_IECDV_525x5994I_25Mbps_SMPTE322M";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_IECDV_525x5994I_25Mbps_SMPTE322M))
		return "AAFContainerDef_MXFGC_Clipwrapped_IECDV_525x5994I_25Mbps_SMPTE322M";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_IECDV_625x50I_25Mbps_SMPTE322M))
		return "AAFContainerDef_MXFGC_Framewrapped_IECDV_625x50I_25Mbps_SMPTE322M";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_IECDV_625x50I_25Mbps_SMPTE322M))
		return "AAFContainerDef_MXFGC_Clipwrapped_IECDV_625x50I_25Mbps_SMPTE322M";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_IECDV_UndefinedSource_25Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_IECDV_UndefinedSource_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_IECDV_UndefinedSource_25Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_IECDV_UndefinedSource_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_525x5994I_25Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_525x5994I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_525x5994I_25Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_525x5994I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_625x50I_25Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_625x50I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_625x50I_25Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_625x50I_25Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_525x5994I_50Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_525x5994I_50Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_525x5994I_50Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_525x5994I_50Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_625x50I_50Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_625x50I_50Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_625x50I_50Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_625x50I_50Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_1080x5994I_100Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_1080x5994I_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_1080x5994I_100Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_1080x5994I_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_1080x50I_100Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_1080x50I_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_1080x50I_100Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_1080x50I_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_720x5994P_100Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_720x5994P_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_720x5994P_100Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_720x5994P_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_720x50P_100Mbps))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_720x50P_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_720x50P_100Mbps))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_720x50P_100Mbps";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_DVbased_UndefinedSource))
		return "AAFContainerDef_MXFGC_Framewrapped_DVbased_UndefinedSource";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_DVbased_UndefinedSource))
		return "AAFContainerDef_MXFGC_Clipwrapped_DVbased_UndefinedSource";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_MPEGES_VideoStream0_SID))
		return "AAFContainerDef_MXFGC_Framewrapped_MPEGES_VideoStream0_SID";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_CustomClosedGOPwrapped_MPEGES_VideoStream1_SID))
		return "AAFContainerDef_MXFGC_CustomClosedGOPwrapped_MPEGES_VideoStream1_SID";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_Uncompressed_525x5994I_720_422))
		return "AAFContainerDef_MXFGC_Framewrapped_Uncompressed_525x5994I_720_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_525x5994I_720_422))
		return "AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_525x5994I_720_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Linewrapped_Uncompressed_525x5994I_720_422))
		return "AAFContainerDef_MXFGC_Linewrapped_Uncompressed_525x5994I_720_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_Uncompressed_625x50I_720_422))
		return "AAFContainerDef_MXFGC_Framewrapped_Uncompressed_625x50I_720_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_625x50I_720_422))
		return "AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_625x50I_720_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Linewrapped_Uncompressed_625x50I_720_422))
		return "AAFContainerDef_MXFGC_Linewrapped_Uncompressed_625x50I_720_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_Uncompressed_525x5994P_960_422))
		return "AAFContainerDef_MXFGC_Framewrapped_Uncompressed_525x5994P_960_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_525x5994P_960_422))
		return "AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_525x5994P_960_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Linewrapped_Uncompressed_525x5994P_960_422))
		return "AAFContainerDef_MXFGC_Linewrapped_Uncompressed_525x5994P_960_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_Uncompressed_625x50P_960_422))
		return "AAFContainerDef_MXFGC_Framewrapped_Uncompressed_625x50P_960_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_625x50P_960_422))
		return "AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_625x50P_960_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Linewrapped_Uncompressed_625x50P_960_422))
		return "AAFContainerDef_MXFGC_Linewrapped_Uncompressed_625x50P_960_422";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_Broadcast_Wave_audio_data))
		return "AAFContainerDef_MXFGC_Framewrapped_Broadcast_Wave_audio_data";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_Broadcast_Wave_audio_data))
		return "AAFContainerDef_MXFGC_Clipwrapped_Broadcast_Wave_audio_data";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_AES3_audio_data))
		return "AAFContainerDef_MXFGC_Framewrapped_AES3_audio_data";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_AES3_audio_data))
		return "AAFContainerDef_MXFGC_Clipwrapped_AES3_audio_data";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_Alaw_Audio))
		return "AAFContainerDef_MXFGC_Framewrapped_Alaw_Audio";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_Alaw_Audio))
		return "AAFContainerDef_MXFGC_Clipwrapped_Alaw_Audio";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Customwrapped_Alaw_Audio))
		return "AAFContainerDef_MXFGC_Customwrapped_Alaw_Audio";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_AVCbytestream_VideoStream0_SID))
		return "AAFContainerDef_MXFGC_Clipwrapped_AVCbytestream_VideoStream0_SID";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_VC3))
		return "AAFContainerDef_MXFGC_Framewrapped_VC3";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_VC3))
		return "AAFContainerDef_MXFGC_Clipwrapped_VC3";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Framewrapped_VC1))
		return "AAFContainerDef_MXFGC_Framewrapped_VC1";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Clipwrapped_VC1))
		return "AAFContainerDef_MXFGC_Clipwrapped_VC1";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Generic_Essence_Multiple_Mappings))
		return "AAFContainerDef_MXFGC_Generic_Essence_Multiple_Mappings";
	if (aafUIDCmp (auid, &AAFContainerDef_RIFFWAVE))
		return "AAFContainerDef_RIFFWAVE";
	if (aafUIDCmp (auid, &AAFContainerDef_JFIF))
		return "AAFContainerDef_JFIF";
	if (aafUIDCmp (auid, &AAFContainerDef_AIFFAIFC))
		return "AAFContainerDef_AIFFAIFC";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_220X_1080p))
		return "AAFContainerDef_MXFGC_Avid_DNX_220X_1080p";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_145_1080p))
		return "AAFContainerDef_MXFGC_Avid_DNX_145_1080p";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_220_1080p))
		return "AAFContainerDef_MXFGC_Avid_DNX_220_1080p";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_36_1080p))
		return "AAFContainerDef_MXFGC_Avid_DNX_36_1080p";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_220X_1080i))
		return "AAFContainerDef_MXFGC_Avid_DNX_220X_1080i";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_145_1080i))
		return "AAFContainerDef_MXFGC_Avid_DNX_145_1080i";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_220_1080i))
		return "AAFContainerDef_MXFGC_Avid_DNX_220_1080i";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_145_1440_1080i))
		return "AAFContainerDef_MXFGC_Avid_DNX_145_1440_1080i";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_220X_720p))
		return "AAFContainerDef_MXFGC_Avid_DNX_220X_720p";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_220_720p))
		return "AAFContainerDef_MXFGC_Avid_DNX_220_720p";
	if (aafUIDCmp (auid, &AAFContainerDef_MXFGC_Avid_DNX_145_720p))
		return "AAFContainerDef_MXFGC_Avid_DNX_145_720p";

	return "Unknown AAFContainerDef";
}

const char*
aaft_CompressionToText (const aafUID_t* auid)
{
	if (auid == NULL)
		return "n/a";

	if (aafUIDCmp (auid, &AUID_NULL))
		return "AUID_NULL";
	if (aafUIDCmp (auid, &AAFCompressionDef_AAF_CMPR_FULL_JPEG))
		return "AAFCompressionDef_AAF_CMPR_FULL_JPEG";
	if (aafUIDCmp (auid, &AAFCompressionDef_AAF_CMPR_AUNC422))
		return "AAFCompressionDef_AAF_CMPR_AUNC422";
	if (aafUIDCmp (auid, &AAFCompressionDef_LegacyDV))
		return "AAFCompressionDef_LegacyDV";
	if (aafUIDCmp (auid, &AAFCompressionDef_SMPTE_D10_50Mbps_625x50I))
		return "AAFCompressionDef_SMPTE_D10_50Mbps_625x50I";
	if (aafUIDCmp (auid, &AAFCompressionDef_SMPTE_D10_50Mbps_525x5994I))
		return "AAFCompressionDef_SMPTE_D10_50Mbps_525x5994I";
	if (aafUIDCmp (auid, &AAFCompressionDef_SMPTE_D10_40Mbps_625x50I))
		return "AAFCompressionDef_SMPTE_D10_40Mbps_625x50I";
	if (aafUIDCmp (auid, &AAFCompressionDef_SMPTE_D10_40Mbps_525x5994I))
		return "AAFCompressionDef_SMPTE_D10_40Mbps_525x5994I";
	if (aafUIDCmp (auid, &AAFCompressionDef_SMPTE_D10_30Mbps_625x50I))
		return "AAFCompressionDef_SMPTE_D10_30Mbps_625x50I";
	if (aafUIDCmp (auid, &AAFCompressionDef_SMPTE_D10_30Mbps_525x5994I))
		return "AAFCompressionDef_SMPTE_D10_30Mbps_525x5994I";
	if (aafUIDCmp (auid, &AAFCompressionDef_IEC_DV_525_60))
		return "AAFCompressionDef_IEC_DV_525_60";
	if (aafUIDCmp (auid, &AAFCompressionDef_IEC_DV_625_50))
		return "AAFCompressionDef_IEC_DV_625_50";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_25Mbps_525_60))
		return "AAFCompressionDef_DV_Based_25Mbps_525_60";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_25Mbps_625_50))
		return "AAFCompressionDef_DV_Based_25Mbps_625_50";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_50Mbps_525_60))
		return "AAFCompressionDef_DV_Based_50Mbps_525_60";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_50Mbps_625_50))
		return "AAFCompressionDef_DV_Based_50Mbps_625_50";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_100Mbps_1080x5994I))
		return "AAFCompressionDef_DV_Based_100Mbps_1080x5994I";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_100Mbps_1080x50I))
		return "AAFCompressionDef_DV_Based_100Mbps_1080x50I";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_100Mbps_720x5994P))
		return "AAFCompressionDef_DV_Based_100Mbps_720x5994P";
	if (aafUIDCmp (auid, &AAFCompressionDef_DV_Based_100Mbps_720x50P))
		return "AAFCompressionDef_DV_Based_100Mbps_720x50P";
	if (aafUIDCmp (auid, &AAFCompressionDef_VC3_1))
		return "AAFCompressionDef_VC3_1";
	if (aafUIDCmp (auid, &AAFCompressionDef_Avid_DNxHD_Legacy))
		return "AAFCompressionDef_Avid_DNxHD_Legacy";

	return "Unknown AAFCompressionDef";
}
