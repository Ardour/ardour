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

#include <stdio.h>
#include <string.h>

#include "aaf/AAFDump.h"
#include "aaf/AAFToText.h"
#include "aaf/AAFTypes.h"

#include "aaf/AAFClass.h"
#include "aaf/utils.h"

void
aaf_dump_Header (AAF_Data* aafd)
{
	struct dbg* dbg = aafd->dbg;

	DBG_BUFFER_WRITE (dbg, " ByteOrder            : %ls (0x%04x)\n", aaft_ByteOrderToText (aafd->Header.ByteOrder), aafd->Header.ByteOrder);
	DBG_BUFFER_WRITE (dbg, " LastModified         : %ls\n", aaft_TimestampToText (aafd->Header.LastModified));
	DBG_BUFFER_WRITE (dbg, " AAF ObjSpec Version  : %ls\n", aaft_VersionToText (aafd->Header.Version));
	DBG_BUFFER_WRITE (dbg, " ObjectModel Version  : %u\n", aafd->Header.ObjectModelVersion);
	DBG_BUFFER_WRITE (dbg, " Operational Pattern  : %ls\n", aaft_OPDefToText (aafd->Header.OperationalPattern));

	DBG_BUFFER_WRITE (dbg, "\n\n");

	dbg->debug_callback (dbg, (void*)aafd, DEBUG_SRC_ID_DUMP, 0, "", "", 0, dbg->_dbg_msg, dbg->user);
}

void
aaf_dump_Identification (AAF_Data* aafd)
{
	struct dbg* dbg = aafd->dbg;

	DBG_BUFFER_WRITE (dbg, " CompanyName          : %ls\n", (aafd->Identification.CompanyName) ? aafd->Identification.CompanyName : L"n/a");
	DBG_BUFFER_WRITE (dbg, " ProductName          : %ls\n", (aafd->Identification.ProductName) ? aafd->Identification.ProductName : L"n/a");
	DBG_BUFFER_WRITE (dbg, " ProductVersion       : %ls\n", aaft_ProductVersionToText (aafd->Identification.ProductVersion));
	DBG_BUFFER_WRITE (dbg, " ProductVersionString : %ls\n", (aafd->Identification.ProductVersionString) ? aafd->Identification.ProductVersionString : L"n/a");
	DBG_BUFFER_WRITE (dbg, " ProductID            : %ls\n", AUIDToText (aafd->Identification.ProductID));
	DBG_BUFFER_WRITE (dbg, " Date                 : %ls\n", aaft_TimestampToText (aafd->Identification.Date));
	DBG_BUFFER_WRITE (dbg, " ToolkitVersion       : %ls\n", aaft_ProductVersionToText (aafd->Identification.ToolkitVersion));
	DBG_BUFFER_WRITE (dbg, " Platform             : %ls\n", (aafd->Identification.Platform) ? aafd->Identification.Platform : L"n/a");
	DBG_BUFFER_WRITE (dbg, " GenerationAUID       : %ls\n", AUIDToText (aafd->Identification.GenerationAUID));

	DBG_BUFFER_WRITE (dbg, "\n\n");

	dbg->debug_callback (dbg, (void*)aafd, DEBUG_SRC_ID_DUMP, 0, "", "", 0, dbg->_dbg_msg, dbg->user);
}

void
aaf_dump_ObjectProperty (AAF_Data* aafd, aafProperty* Prop)
{
	struct dbg* dbg = aafd->dbg;

	if (Prop->def->meta) {
		DBG_BUFFER_WRITE (dbg, " :.: %s(0x%04x) %ls%s (%ls)\n", ANSI_COLOR_YELLOW (dbg), Prop->pid, aaft_PIDToText (aafd, Prop->pid), ANSI_COLOR_RESET (dbg), aaft_StoredFormToText (Prop->sf) /*AUIDToText( &Prop->def->type ),*/ /*aaft_TypeIDToText( &(Prop->def->type) )*/);
	} else {
		DBG_BUFFER_WRITE (dbg, " :.: (0x%04x) %ls (%ls)\n", Prop->pid, aaft_PIDToText (aafd, Prop->pid), aaft_StoredFormToText (Prop->sf) /*AUIDToText( &Prop->def->type ),*/ /*aaft_TypeIDToText( &(Prop->def->type) )*/);
	}

	// WARNING : Wont print strong references (set/vector) corectly.
	aafd->dbg->_dbg_msg_pos += laaf_util_dump_hex (Prop->val, Prop->len, &aafd->dbg->_dbg_msg, &aafd->dbg->_dbg_msg_size, aafd->dbg->_dbg_msg_pos);

	dbg->debug_callback (dbg, (void*)aafd, DEBUG_SRC_ID_DUMP, 0, "", "", 0, dbg->_dbg_msg, dbg->user);
}

void
aaf_dump_ObjectProperties (AAF_Data* aafd, aafObject* Obj)
{
	/*
	 *  List the properties once they have been parsed and interpreted by AAFCore.
	 */

	aafProperty* Prop = NULL;

	for (Prop = Obj->Properties; Prop != NULL; Prop = Prop->next) {
		aaf_dump_ObjectProperty (aafd, Prop);
	}
}

void
aaf_dump_rawProperties (AAF_Data* aafd, aafByte_t* propStream)
{
	struct dbg* dbg = aafd->dbg;

	if (propStream == NULL) {
		DBG_BUFFER_WRITE (dbg,
		                  " ## Property_Header____________________________________________________\n\n"
		                  " aafPropertyIndexHeader_t is NULL\n"
		                  " ======================================================================\n\n");
		return;
	}

	aafPropertyIndexHeader_t Header;
	aafPropertyIndexEntry_t  Prop;
	aafByte_t*               value = NULL;

	memcpy (&Header, propStream, sizeof (aafPropertyIndexHeader_t));

	uint32_t i           = 0;
	uint32_t valueOffset = 0;

	DBG_BUFFER_WRITE (dbg,
	                  " ## Property_Header____________________________________________________\n\n"
	                  " _byteOrder     : 0x%02x\n"
	                  " _formatVersion : 0x%02x\n"
	                  " _entryCount    : %u\n\n"
	                  " ======================================================================\n\n",
	                  Header._byteOrder,
	                  Header._formatVersion,
	                  Header._entryCount);

	DBG_BUFFER_WRITE (dbg, "\n\n");

	/*
	 * Since the following for-loop macro is not intended to be user
	 * accessible, it has been defined as a local macro in AAFCore.c.
	 */

	// foreachPropertyEntry( Header, Prop, value, i )
	for (valueOffset = sizeof (aafPropertyIndexHeader_t) + (Header._entryCount * sizeof (aafPropertyIndexEntry_t)),
	    i            = 0;
	     i < Header._entryCount &&
	     memcpy (&Prop, (propStream + ((sizeof (aafPropertyIndexHeader_t)) + (sizeof (aafPropertyIndexEntry_t) * i))), sizeof (aafPropertyIndexEntry_t)) &&
	     (value = propStream + valueOffset);
	     valueOffset += Prop._length,
	    i++) {
		DBG_BUFFER_WRITE (dbg,
		                  " #%u Property_Entry_____________________________________________________\n"
		                  " _pid        : 0x%04x (%ls)\n"
		                  " _storedForm : %ls\n"
		                  " _length     : %u bytes\n",
		                  i,
		                  Prop._pid, aaft_PIDToText (aafd, Prop._pid),
		                  aaft_StoredFormToText (Prop._storedForm),
		                  Prop._length);

		aafd->dbg->_dbg_msg_pos += laaf_util_dump_hex (value, Prop._length, &aafd->dbg->_dbg_msg, &aafd->dbg->_dbg_msg_size, aafd->dbg->_dbg_msg_pos);

		DBG_BUFFER_WRITE (dbg, "\n");
	}

	dbg->debug_callback (dbg, (void*)aafd, DEBUG_SRC_ID_DUMP, 0, "", "", 0, dbg->_dbg_msg, dbg->user);
}

void
aaf_dump_nodeStreamProperties (AAF_Data* aafd, cfbNode* node)
{
	/*
	 *  List the raw properties directly from a CFB Node's stream.
	 */

	aafByte_t* propStream = NULL;

	cfb_getStream (aafd->cfbd, node, &propStream, NULL);

	aaf_dump_rawProperties (aafd, propStream);

	free (propStream);
}

void
aaf_dump_MetaDictionary (AAF_Data* aafd)
{
	/*
	 *  NOTE Only dumps the "custom" classes/properties, since those are the only
	 *  ones we register when parsing. That is, all standard classes/properties
	 *  wont be printed out.
	 */

	struct dbg* dbg = aafd->dbg;

	aafClass* Class = NULL;

	foreachClass (Class, aafd->Classes)
	{
		int print = 0;

		aafPropertyDef* PDef = NULL;

		foreachPropertyDefinition (PDef, Class->Properties)
		{
			if (Class->meta) {
				DBG_BUFFER_WRITE (dbg, "%s%ls::%ls (0x%04x)%s\n",
				                  ANSI_COLOR_YELLOW (dbg),
				                  Class->name,
				                  PDef->name,
				                  PDef->pid,
				                  ANSI_COLOR_RESET (dbg));

				print++;
			} else if (PDef->meta) {
				DBG_BUFFER_WRITE (dbg, "%ls::%s%ls (0x%04x)%s\n",
				                  aaft_ClassIDToText (aafd, Class->ID),
				                  ANSI_COLOR_YELLOW (dbg),
				                  PDef->name,
				                  PDef->pid,
				                  ANSI_COLOR_RESET (dbg));

				print++;
			}
		}

		if (print) {
			DBG_BUFFER_WRITE (dbg, "\n");
		}

		print = 1;
	}

	DBG_BUFFER_WRITE (dbg, "\n\n");

	dbg->debug_callback (dbg, (void*)aafd, DEBUG_SRC_ID_DUMP, 0, "", "", 0, dbg->_dbg_msg, dbg->user);
}

void
aaf_dump_Classes (AAF_Data* aafd)
{
	struct dbg* dbg = aafd->dbg;

	aafClass* ConcreteClass = NULL;
	aafClass* Class         = NULL;

	foreachClass (ConcreteClass, aafd->Classes)
	{
		foreachClassInheritance (Class, ConcreteClass)
		{
			DBG_BUFFER_WRITE (dbg, "%s%ls%s",
			                  (Class->meta) ? ANSI_COLOR_YELLOW (dbg) : "",
			                  aaft_ClassIDToText (aafd, Class->ID),
			                  (Class->meta) ? ANSI_COLOR_RESET (dbg) : "");

			if (Class->Parent != NULL)
				DBG_BUFFER_WRITE (dbg, " > ");
		}

		DBG_BUFFER_WRITE (dbg, "\n");
	}

	DBG_BUFFER_WRITE (dbg, "\n\n");

	dbg->debug_callback (dbg, (void*)aafd, DEBUG_SRC_ID_DUMP, 0, "", "", 0, dbg->_dbg_msg, dbg->user);
}
