/*
 * Copyright (C) 2017-2023 Adrien Gesta-Fline
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

#include <stdio.h>
#include <wchar.h>

#include <libaaf/AAFCore.h>
#include <libaaf/AAFTypes.h>
#include <libaaf/LibCFB.h>

#define AUIDToText(auid) cfb_CLSIDToText((const cfbCLSID_t *)auid)

const wchar_t *aaft_MobIDToText(aafMobID_t *mobid);

const wchar_t *aaft_TimestampToText(aafTimeStamp_t *ts);

const wchar_t *aaft_VersionToText(aafVersionType_t *vers);

const wchar_t *aaft_ProductVersionToText(aafProductVersion_t *vers);

const wchar_t *aaft_FileKindToText(const aafUID_t *auid);

const wchar_t *aaft_TapeCaseTypeToText(aafTapeCaseType_t t);

const wchar_t *aaft_VideoSignalTypeToText(aafVideoSignalType_t v);

const wchar_t *aaft_TapeFormatTypeToText(aafTapeFormatType_t t);

const wchar_t *aaft_FilmTypeToText(aafFilmType_t f);

const wchar_t *aaft_SignalStandardToText(aafSignalStandard_t s);

const wchar_t *aaft_FieldNumberToText(aafFieldNumber_t f);

const wchar_t *aaft_AlphaTransparencyToText(aafAlphaTransparency_t a);

const wchar_t *aaft_FrameLayoutToText(aafFrameLayout_t f);

const wchar_t *aaft_ColorSitingToText(aafColorSiting_t c);

const wchar_t *aaft_ProductReleaseTypeToText(aafProductReleaseType_t t);

const wchar_t *aaft_FadeTypeToText(aafFadeType_t f);

const wchar_t *aaft_BoolToText(aafBoolean_t b);

const wchar_t *aaft_OperationCategoryToText(const aafUID_t *auid);

const wchar_t *aaft_PluginCategoryToText(const aafUID_t *auid);

const wchar_t *aaft_ScanningDirectionToText(aafScanningDirection_t s);

const wchar_t *aaft_ByteOrderToText(int16_t e);

const wchar_t *aaft_ElectroSpatialToText(aafElectroSpatialFormulation_t e);

const wchar_t *aaft_TypeIDToText(const aafUID_t *auid);

const wchar_t *aaft_StoredFormToText(enum aafStoredForm_e sf);

const wchar_t *aaft_OPDefToText(const aafUID_t *auid);

const wchar_t *aaft_DataDefToText(AAF_Data *aafd, const aafUID_t *auid);

const wchar_t *aaft_OperationDefToText(AAF_Data *aafd, const aafUID_t *auid);

const wchar_t *aaft_InterpolationToText(const aafUID_t *auid);

const wchar_t *aaft_ParameterToText(AAF_Data *aafd, const aafUID_t *auid);

const wchar_t *aaft_TransferCharacteristicToText(const aafUID_t *auid);

const wchar_t *aaft_CodingEquationsToText(const aafUID_t *auid);

const wchar_t *aaft_ColorPrimariesToText(const aafUID_t *auid);

const wchar_t *aaft_UsageCodeToText(const aafUID_t *auid);

const wchar_t *aaft_PIDToText(AAF_Data *aafd, aafPID_t pid);

const wchar_t *aaft_ClassIDToText(AAF_Data *aafd, const aafUID_t *auid);

const wchar_t *aaft_ContainerToText(const aafUID_t *auid);

const wchar_t *aaft_CompressionToText(const aafUID_t *auid);

#endif // !__AAFToText_h__
