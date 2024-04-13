/*
 * Copyright (C) 2023-2024 Adrien Gesta-Fline
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

#include <stdint.h>
#include <stdio.h>

#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"
#include "aaf/AAFIParser.h"
#include "aaf/AAFToText.h"

#include "aaf/libaaf.h"
#include "aaf/log.h"

#define debug(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafi->log, aafi, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

int
resolve_AAF (struct AAF_Iface* aafi)
{
	int probe = 0;

	if (aafi->aafd->Identification.CompanyName && strncmp (aafi->aafd->Identification.CompanyName, "Blackmagic Design", strlen ("Blackmagic Design")) == 0) {
		probe++;
	}

	if (aafi->aafd->Identification.ProductName && strncmp (aafi->aafd->Identification.ProductName, "DaVinci Resolve", strlen ("DaVinci Resolve")) == 0) {
		probe++;
	}

	if (probe == 2) {
		return 1;
	}

	return 0;
}
