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

#ifndef __AAFToText_h__
#define __AAFToText_h__

#include "aaf/AAFCore.h"
#include "aaf/AAFTypes.h"
#include "aaf/LibCFB.h"

#define AUIDToText(auid) \
	cfb_CLSIDToText ((const cfbCLSID_t*)auid)

const char*
aaft_MobIDToText (aafMobID_t* mobid);

const char*
aaft_TimestampToText (aafTimeStamp_t* ts);

const char*
aaft_VersionToText (aafVersionType_t* vers);

const char*
aaft_ProductVersionToText (aafProductVersion_t* vers);

const char*
aaft_FileKindToText (const aafUID_t* auid);

const char*
aaft_TapeCaseTypeToText (aafTapeCaseType_t t);

const char*
aaft_VideoSignalTypeToText (aafVideoSignalType_t v);

const char*
aaft_TapeFormatTypeToText (aafTapeFormatType_t t);

const char*
aaft_FilmTypeToText (aafFilmType_t f);

const char*
aaft_SignalStandardToText (aafSignalStandard_t s);

const char*
aaft_FieldNumberToText (aafFieldNumber_t f);

const char*
aaft_AlphaTransparencyToText (aafAlphaTransparency_t a);

const char*
aaft_FrameLayoutToText (aafFrameLayout_t f);

const char*
aaft_ColorSitingToText (aafColorSiting_t c);

const char*
aaft_ProductReleaseTypeToText (aafProductReleaseType_t t);

const char*
aaft_FadeTypeToText (aafFadeType_t f);

const char*
aaft_BoolToText (aafBoolean_t b);

const char*
aaft_OperationCategoryToText (const aafUID_t* auid);

const char*
aaft_PluginCategoryToText (const aafUID_t* auid);

const char*
aaft_ScanningDirectionToText (aafScanningDirection_t s);

const char*
aaft_ByteOrderToText (int16_t e);

const char*
aaft_ElectroSpatialToText (aafElectroSpatialFormulation_t e);

const char*
aaft_TypeIDToText (const aafUID_t* auid);

const char*
aaft_StoredFormToText (enum aafStoredForm_e sf);

const char*
aaft_OPDefToText (const aafUID_t* auid);

const char*
aaft_DataDefToText (AAF_Data* aafd, const aafUID_t* auid);

const char*
aaft_OperationDefToText (AAF_Data* aafd, const aafUID_t* auid);

const char*
aaft_InterpolationToText (const aafUID_t* auid);

const char*
aaft_ParameterToText (AAF_Data* aafd, const aafUID_t* auid);

const char*
aaft_TransferCharacteristicToText (const aafUID_t* auid);

const char*
aaft_CodingEquationsToText (const aafUID_t* auid);

const char*
aaft_ColorPrimariesToText (const aafUID_t* auid);

const char*
aaft_UsageCodeToText (const aafUID_t* auid);

const char*
aaft_PIDToText (AAF_Data* aafd, aafPID_t pid);

const char*
aaft_ClassIDToText (AAF_Data* aafd, const aafUID_t* auid);

const char*
aaft_ContainerToText (const aafUID_t* auid);

const char*
aaft_IndirectValueToText (AAF_Data* aafd, aafIndirect_t* Indirect);

const char*
aaft_CompressionToText (const aafUID_t* auid);

#endif // !__AAFToText_h__
