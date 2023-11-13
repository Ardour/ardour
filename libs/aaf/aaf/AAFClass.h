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

#ifndef __AAFClass_h__
#define __AAFClass_h__

/**
 * @brief AAF core functions.
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 04 october 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/AAFCore.h"
#include "aaf/AAFTypes.h"

#define foreachClass(Class, Classes) \
	for (Class = Classes; Class != NULL; Class = Class->next)

#define foreachClassInheritance(Class, Classes) \
	for (Class = Classes; Class != NULL; Class = Class->Parent)

#define foreachPropertyDefinition(PDef, PDefs) \
	for (PDef = PDefs; PDef != NULL; PDef = PDef->next)

int
aafclass_classExists (AAF_Data* aafd, aafUID_t* ClassID);

aafClass*
aafclass_defineNewClass (AAF_Data* aafd, const aafUID_t* id, uint8_t isConcrete, aafClass* parent);

aafClass*
aafclass_getClassByID (AAF_Data* aafd, const aafUID_t* id);

aafPropertyDef*
aafclass_getPropertyDefinitionByID (aafClass* Classes, aafPID_t PID);

void
aafclass_printClasses (aafClass* Class, int depth); // TODO move to AAFDump ?

int
aafclass_setDefaultClasses (AAF_Data* aafd);

#endif // ! __AAFClass_h__
