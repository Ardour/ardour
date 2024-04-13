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

#include <stdio.h>
#include <string.h>

#include "aaf/AAFDefs/AAFClassDefUIDs.h"
#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"
#include "aaf/AAFDump.h"
#include "aaf/AAFToText.h"
#include "aaf/AAFTypes.h"

#include "aaf/utils.h"

#include "aaf/AAFClass.h"

void
aaf_dump_Header (AAF_Data* aafd, const char* padding)
{
	struct aafLog* log = aafd->log;

	LOG_BUFFER_WRITE (log, "%sByteOrder            : %s%s (0x%04x)%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_ByteOrderToText (aafd->Header.ByteOrder), aafd->Header.ByteOrder, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sLastModified         : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_TimestampToText (aafd->Header.LastModified), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sAAF ObjSpec Version  : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_VersionToText (aafd->Header.Version), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sObjectModel Version  : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), aafd->Header.ObjectModelVersion, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sOperational Pattern  : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_OPDefToText (aafd->Header.OperationalPattern), ANSI_COLOR_RESET (log));

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
aaf_dump_Identification (AAF_Data* aafd, const char* padding)
{
	struct aafLog* log = aafd->log;

	LOG_BUFFER_WRITE (log, "%sCompanyName          : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), (aafd->Identification.CompanyName) ? aafd->Identification.CompanyName : "n/a", ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sProductName          : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), (aafd->Identification.ProductName) ? aafd->Identification.ProductName : "n/a", ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sProductVersion       : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_ProductVersionToText (aafd->Identification.ProductVersion), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sProductVersionString : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), (aafd->Identification.ProductVersionString) ? aafd->Identification.ProductVersionString : "n/a", ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sProductID            : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), AUIDToText (aafd->Identification.ProductID), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sDate                 : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_TimestampToText (aafd->Identification.Date), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sToolkitVersion       : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), aaft_ProductVersionToText (aafd->Identification.ToolkitVersion), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sPlatform             : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), (aafd->Identification.Platform) ? aafd->Identification.Platform : "n/a", ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%sGenerationAUID       : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), AUIDToText (aafd->Identification.GenerationAUID), ANSI_COLOR_RESET (log));

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
aaf_dump_ObjectProperty (AAF_Data* aafd, aafProperty* Prop, const char* padding)
{
	struct aafLog* log = aafd->log;

	if (Prop->def->meta) {
		LOG_BUFFER_WRITE (log, "%s%s[%s0x%04x%s] %s (%s)\n",
		                  padding,
		                  ANSI_COLOR_RESET (log),
		                  ANSI_COLOR_MAGENTA (log),
		                  Prop->pid,
		                  ANSI_COLOR_RESET (log),
		                  aaft_PIDToText (aafd, Prop->pid),
		                  aaft_StoredFormToText (Prop->sf));
	} else {
		LOG_BUFFER_WRITE (log, "%s%s[%s0x%04x%s] %s (%s)\n",
		                  padding,
		                  ANSI_COLOR_RESET (log),
		                  ANSI_COLOR_DARKGREY (log),
		                  Prop->pid,
		                  ANSI_COLOR_RESET (log),
		                  aaft_PIDToText (aafd, Prop->pid),
		                  aaft_StoredFormToText (Prop->sf));
	}

	int rc = laaf_util_dump_hex (Prop->val, Prop->len, &aafd->log->_msg, &aafd->log->_msg_size, aafd->log->_msg_pos, padding);

	if (rc > 0) {
		aafd->log->_msg_pos += (size_t)rc;
	}

	log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
aaf_dump_TaggedValueSet (AAF_Data* aafd, aafObject* ObjCollection, const char* padding)
{
	struct aafLog* log = aafd->log;

	aafObject* Obj = NULL;

	int i = 0;
	AAF_foreach_ObjectInSet (&Obj, ObjCollection, NULL)
	{
		i++;

		if (!aafUIDCmp (Obj->Class->ID, &AAFClassID_TaggedValue)) {
			LOG_BUFFER_WRITE (log, "%s%sObject > %s\n",
			                  padding,
			                  ANSI_COLOR_RESET (log),
			                  aaft_ClassIDToText (aafd, Obj->Class->ID));
			continue;
		}

		char*          name     = aaf_get_propertyValue (Obj, PID_TaggedValue_Name, &AAFTypeID_String);
		aafIndirect_t* indirect = aaf_get_propertyValue (Obj, PID_TaggedValue_Value, &AAFTypeID_Indirect);

		LOG_BUFFER_WRITE (log, "%s%sTagged > Name: %s%s%s%*s      Value: %s(%s)%s %s%s%s%s%s\n",
		                  padding,
		                  ANSI_COLOR_RESET (log),
		                  ANSI_COLOR_DARKGREY (log),
		                  (name) ? name : "<unknown>",
		                  ANSI_COLOR_RESET (log),
		                  (name) ? (size_t) (34 - (int)strlen (name)) : (size_t) (34 - strlen ("<unknown>")), " ",
		                  ANSI_COLOR_DARKGREY (log),
		                  aaft_TypeIDToText (&indirect->TypeDef),
		                  ANSI_COLOR_RESET (log),
		                  ANSI_COLOR_DARKGREY (log),
		                  (aafUIDCmp (&indirect->TypeDef, &AAFTypeID_String)) ? "\"" : "",
		                  aaft_IndirectValueToText (aafd, indirect),
		                  (aafUIDCmp (&indirect->TypeDef, &AAFTypeID_String)) ? "\"" : "",
		                  ANSI_COLOR_RESET (log));

		log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);

		free (name);
	}
}

void
aaf_dump_ObjectProperties (AAF_Data* aafd, aafObject* Obj, const char* padding)
{
	/*
	 *  List the properties once they have been parsed and interpreted by AAFCore.
	 */

	aafProperty* Prop = NULL;

	for (Prop = Obj->Properties; Prop != NULL; Prop = Prop->next) {
		aaf_dump_ObjectProperty (aafd, Prop, padding);
	}
}

void
aaf_dump_rawProperties (AAF_Data* aafd, aafByte_t* propStream, const char* padding)
{
	struct aafLog* log = aafd->log;

	if (propStream == NULL) {
		LOG_BUFFER_WRITE (log,
		                  "%s## Property_Header____________________________________________________\n\n"
		                  "%saafPropertyIndexHeader_t is NULL\n"
		                  "%s======================================================================\n\n",
		                  padding,
		                  padding,
		                  padding);
		return;
	}

	aafPropertyIndexHeader_t Header;
	aafPropertyIndexEntry_t  Prop;
	aafByte_t*               value = NULL;

	memcpy (&Header, propStream, sizeof (aafPropertyIndexHeader_t));

	uint32_t i           = 0;
	size_t   valueOffset = 0;

	LOG_BUFFER_WRITE (log,
	                  "%s## Property_Header____________________________________________________\n\n"
	                  "%s_byteOrder     : %s0x%02x%s\n"
	                  "%s_formatVersion : %s0x%02x%s\n"
	                  "%s_entryCount    : %s%u%s\n\n"
	                  "%s======================================================================\n\n",
	                  padding,
	                  padding, ANSI_COLOR_DARKGREY (log), Header._byteOrder, ANSI_COLOR_RESET (log),
	                  padding, ANSI_COLOR_DARKGREY (log), Header._formatVersion, ANSI_COLOR_RESET (log),
	                  padding, ANSI_COLOR_DARKGREY (log), Header._entryCount, ANSI_COLOR_RESET (log),
	                  padding);

	LOG_BUFFER_WRITE (log, "\n\n");

	/*
	 * Since the following for-loop macro is not intended to be user
	 * accessible, it has been defined as a local macro in AAFCore.c.
	 */
	foreachPropertyEntry (propStream, Header, Prop, value, valueOffset, i)
	{
		LOG_BUFFER_WRITE (log,
		                  "%s#%u Property_Entry_____________________________________________________\n"
		                  "%s_pid        : %s0x%04x (%s)%s\n"
		                  "%s_storedForm : %s%s%s\n"
		                  "%s_length     : %s%u bytes%s\n",
		                  padding, i,
		                  padding, ANSI_COLOR_DARKGREY (log), Prop._pid, aaft_PIDToText (aafd, Prop._pid), ANSI_COLOR_RESET (log),
		                  padding, ANSI_COLOR_DARKGREY (log), aaft_StoredFormToText (Prop._storedForm), ANSI_COLOR_RESET (log),
		                  padding, ANSI_COLOR_DARKGREY (log), Prop._length, ANSI_COLOR_RESET (log));

		int rc = laaf_util_dump_hex (value, Prop._length, &aafd->log->_msg, &aafd->log->_msg_size, aafd->log->_msg_pos, padding);

		if (rc > 0) {
			aafd->log->_msg_pos += (size_t)rc;
		}

		LOG_BUFFER_WRITE (log, "\n");
	}

	log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
aaf_dump_nodeStreamProperties (AAF_Data* aafd, cfbNode* node, const char* padding)
{
	/*
	 *  List the raw properties directly from a CFB Node's stream.
	 */

	aafByte_t* propStream = NULL;

	cfb_getStream (aafd->cfbd, node, &propStream, NULL);

	aaf_dump_rawProperties (aafd, propStream, padding);

	free (propStream);
}

void
aaf_dump_MetaDictionary (AAF_Data* aafd, const char* padding)
{
	/*
	 *  NOTE Only dumps the "custom" classes/properties, since those are the only
	 *  ones we register when parsing. That is, all standard classes/properties
	 *  wont be printed out.
	 */

	struct aafLog* log = aafd->log;

	aafClass* Class = NULL;

	foreachClass (Class, aafd->Classes)
	{
		int print = 0;

		aafPropertyDef* PDef = NULL;

		foreachPropertyDefinition (PDef, Class->Properties)
		{
			if (Class->meta) {
				LOG_BUFFER_WRITE (log, "%s%s%s::%s (0x%04x)%s\n",
				                  padding,
				                  ANSI_COLOR_MAGENTA (log),
				                  Class->name,
				                  PDef->name,
				                  PDef->pid,
				                  ANSI_COLOR_RESET (log));

				print++;
			} else if (PDef->meta) {
				LOG_BUFFER_WRITE (log, "%s%s::%s%s (0x%04x)%s\n",
				                  padding,
				                  aaft_ClassIDToText (aafd, Class->ID),
				                  ANSI_COLOR_MAGENTA (log),
				                  PDef->name,
				                  PDef->pid,
				                  ANSI_COLOR_RESET (log));

				print++;
			}
		}

		if (print) {
			LOG_BUFFER_WRITE (log, "\n");
		}
	}

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
aaf_dump_Classes (AAF_Data* aafd, const char* padding)
{
	struct aafLog* log = aafd->log;

	aafClass* ConcreteClass = NULL;
	aafClass* Class         = NULL;

	foreachClass (ConcreteClass, aafd->Classes)
	{
		foreachClassInheritance (Class, ConcreteClass)
		{
			LOG_BUFFER_WRITE (log, "%s%s%s%s",
			                  padding,
			                  (Class->meta) ? ANSI_COLOR_MAGENTA (log) : "",
			                  aaft_ClassIDToText (aafd, Class->ID),
			                  (Class->meta) ? ANSI_COLOR_RESET (log) : "");

			if (Class->Parent != NULL) {
				LOG_BUFFER_WRITE (log, " > ");
			}
		}

		LOG_BUFFER_WRITE (log, "\n");
	}

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)aafd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}
