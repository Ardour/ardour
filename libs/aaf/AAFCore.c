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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/AAFCore.h"
#include "aaf/AAFDump.h"
#include "aaf/AAFToText.h"
#include "aaf/AAFTypes.h"

#include "aaf/AAFDefs/AAFClassDefUIDs.h"
#include "aaf/AAFDefs/AAFFileKinds.h"
#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"

#include "aaf/log.h"

#include "aaf/AAFClass.h"
#include "aaf/utils.h"

#define debug(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_ERROR, __VA_ARGS__)

/**
 * Loops through each aafStrongRefSetEntry_t of a StrongRefSet Index node stream.
 *
 * @param Header Pointer to the stream's aafStrongRefSetHeader_t struct.
 * @param Entry  Pointer that will receive each aafStrongRefSetEntry_t struct.
 * @param i      uint32_t iterator.
 */

#define foreachStrongRefSetEntry(Header, Entry, i)                                                                                                                                                                         \
	for (i = 0;                                                                                                                                                                                                        \
	     i < Header->_entryCount &&                                                                                                                                                                                    \
	     memcpy (&Entry, ((char*)(Header)) + (sizeof (aafStrongRefSetHeader_t) + (Header->_identificationSize + sizeof (aafStrongRefSetEntry_t)) * i), sizeof (aafStrongRefSetEntry_t) + Header->_identificationSize); \
	     i++)

/**
 * Loops through each aafStrongRefVectorEntry_t of a StrongRefVector Index node stream.
 *
 * @param Header Pointer to the stream's aafStrongRefVectorHeader_t struct.
 * @param Entry  Pointer that will receive each aafStrongRefVectorEntry_t struct.
 * @param i      uint32_t iterator.
 */
#define foreachStrongRefVectorEntry(vectorStream, Header, Entry, i)                                                                                                  \
	for (i = 0;                                                                                                                                                  \
	     i < Header._entryCount &&                                                                                                                               \
	     memcpy (&Entry, (vectorStream + (sizeof (aafStrongRefVectorHeader_t) + (sizeof (aafStrongRefVectorEntry_t) * i))), sizeof (aafStrongRefVectorEntry_t)); \
	     i++)

#define attachNewProperty(Class, PDef, Pid, IsReq)  \
	PDef = calloc (1, sizeof (aafPropertyDef)); \
	if (!PDef) {                                \
		error ("Out of memory");            \
		return NULL;                        \
	}                                           \
	PDef->pid         = Pid;                    \
	PDef->isReq       = IsReq;                  \
	PDef->meta        = 0;                      \
	PDef->name        = NULL;                   \
	PDef->next        = Class->Properties;      \
	Class->Properties = PDef;

/*
 * Retrieves useful file informations out of Header Object.
 *
 * @param  aafd  Pointer to the AAF_Data structure.
 *
 * @return        0 on success\n
 *               -1 on error.
 */

static int
parse_Header (AAF_Data* aafd);

/*
 * Retrieves useful file informations out of Identification Object.
 *
 * @param  aafd  Pointer to the AAF_Data structure.
 *
 * @return        0 on success\n
 *               -1 on error.
 */

static int
parse_Identification (AAF_Data* aafd);

/*
 * Tests the CFB_Data.hdr._clsid field for a valid AAF file.
 *
 * @note The spec says that the AAFFileKind signature should be retrieved from the CLSID
 * of the Root IStorage. In practice, this CLSID holds the #AAFClassID_Root value, and
 * the AAFFileKind is (sometimes) found in the CLSID of the CFB_Header, which according
 * to the CFB spec should be zero. All of this has been observed in AAF files built with
 * the official AAFSDK.
 *
 * As a conclusion, the way to test for a valid AAF file is not a valid test itself.. or
 * not sufficiently documented. Thus, this function shall not be trusted until further
 * knowledge improvement.
 *
 * @param  aafd  Pointer to the AAF_Data structure.
 *
 * @return       1 if the file *looks like* a valid AAF\n
 *               0 otherwise.
 */

// static int isValidAAF( AAF_Data *aafd );

/**
 * Sets the AAF_Data structure's pointers to the main AAF Tree objects. These pointers
 * can then be used for quick conveniant objects access.
 *
 * @param  aafd   Pointer to the AAF_Data structure.
 */

static void
setObjectShortcuts (AAF_Data* aafd);

/**
 * Parses the entire Compound File Binary Tree and retrieves Objets and Properties.
 *
 * This function first parses the Root Object, then follows the Root::MetaDictionary to
 * retrieve potential custom Classes and Properties with retrieveMetaDictionaryClass(),
 * and then parses the rest of the Tree starting at Root::Header.
 *
 * This function should be called after the AAF Classes has been defined by
 * #aafclass_setDefaultClasses(). This function is called by aaf_load_file().
 *
 * @param  aafd   Pointer to the AAF_Data structure.
 */

static int
retrieveObjectTree (AAF_Data* aafd);

/**
 * Parses the entire MetaDictionary, retrieving potential custom classes and properties.
 * This function is to be called within a loop that iterates through the
 * MetaDictionary::ClassDefinitions Objects, and by itself when looking for parent
 * Classes.
 *
 * @param  aafd           Pointer to the AAF_Data structure.
 * @param  TargetClassDef Pointer to the current ClassDefinition Object.
 *
 * @return                A pointer to the retrieved Class.
 */

static aafClass*
retrieveMetaDictionaryClass (AAF_Data* aafd, aafObject* TargetClassDef);

/**
 * Allocates a new aafObject structure, and adds it to the AAF_Data.Objects list.
 *
 * @param  aafd   Pointer to the AAF_Data structure.
 * @param  node   Pointer to the corresponding cfbNode structure.
 * @param  Class  Pointer to the corresponding class definition aafClass structure.
 * @param  parent Pointer to the new Object's parent object, that is the one that has an
 *                ownership reference (Strong Ref Set/Vector) to the new Object.
 *
 * @return        A pointer to the newly created aafObject.
 */

static aafObject*
newObject (AAF_Data* aafd, cfbNode* node, aafClass* Class, aafObject* parent);

/**
 * Allocates a new aafProperty structure.
 *
 * @param  Def  Pointer to the corresponding property definition aafPropertyDef structure.
 *
 * @return      A pointer to the newly created aafProperty.
 */

static aafProperty*
newProperty (AAF_Data* aafd, aafPropertyDef* Def);

/**
 * Test whether or not, a property ID (property definition) was already retrieved for a given Class.
 *
 * @param  Class  Pointer to the aafClass.
 * @param  Pid    Property ID.
 *
 * @return        A pointer to the retrieved property definition\n
 *                 NULL otherwise.
 */

static aafPropertyDef*
propertyIdExistsInClass (aafClass* Class, aafPID_t Pid);

/**
 * Sets the aafStrongRefSetHeader_t Obj->Header and aafStrongRefSetEntry_t Obj->Entry,
 * when parsing an Object from a StrongReferenceSet. This function is called by the
 * #retrieveStrongReferenceSet() function.
 *
 * @param  Obj    Pointer to an aafObject structure.
 * @param  Header Pointer to an aafStrongRefSetHeader_t structure.
 * @param  Entry  Pointer to an aafStrongRefSetEntry_t structure.
 */

static int
setObjectStrongRefSet (aafObject* Obj, aafStrongRefSetHeader_t* Header, aafStrongRefSetEntry_t* Entry);

/**
 * Sets the aafStrongRefVectorHeader_t Obj->Header and aafStrongRefVectorEntry_t
 * Obj->Entry, when parsing an Object from a StrongReferenceSet. This function is called
 * by the retrieveStrongReferenceVector() function.
 *
 * @param  Obj    Pointer to an aafObject structure.
 * @param  Header Pointer to an aafStrongRefVectorHeader_t structure.
 * @param  Entry  Pointer to an aafStrongRefVectorEntry_t structure.
 */

static int
setObjectStrongRefVector (aafObject* Obj, aafStrongRefVectorHeader_t* Header, aafStrongRefVectorEntry_t* Entry);

/**
 * Retrieves and parses a single StrongReference Object. This function is called by
 * retrieveProperty() when it encounters an SF_STRONG_OBJECT_REFERENCE property.
 *
 * @param aafd   Pointer to the AAF_Data structure.
 * @param Prop   Pointer to the property holding the SF_STRONG_OBJECT_REFERENCE.
 * @param parent Pointer to the parent Object which holds the Prop property.
 */

static int
retrieveStrongReference (AAF_Data* aafd, aafProperty* Prop, aafObject* Parent);

/**
 * Retrieves and parses StrongReferenceSet Objects. This function is called by
 * retrieveProperty() when it encounters an SF_STRONG_OBJECT_REFERENCE_SET property.
 *
 * @param aafd   Pointer to the AAF_Data structure.
 * @param Prop   Pointer to the property holding the SF_STRONG_OBJECT_REFERENCE_SET.
 * @param parent Pointer to the parent Object which holds the Prop property.
 */

static int
retrieveStrongReferenceSet (AAF_Data* aafd, aafProperty* Prop, aafObject* parent);

/**
 * Retrieve and parse StrongReferenceVector Objects. This function is called by
 * retrieveProperty() when it encounters an SF_STRONG_OBJECT_REFERENCE_VECTOR property.
 *
 * @param aafd   Pointer to the AAF_Data structure.
 * @param Prop   Pointer to the property holding the SF_STRONG_OBJECT_REFERENCE_VECTOR.
 * @param parent Pointer to the parent Object which holds the Prop property.
 */

static int
retrieveStrongReferenceVector (AAF_Data* aafd, aafProperty* Prop, aafObject* Parent);

/**
 * Adds a new aafProperty to an Object->properties list. If the property Stored Form is
 * either SF_STRONG_OBJECT_REFERENCE, SF_STRONG_OBJECT_REFERENCE_SET or
 * SF_STRONG_OBJECT_REFERENCE_VECTOR, then the function follows the "link" to the
 * Object(s) by calling respectively retrieveStrongReference(),
 * retrieveStrongReferenceSet() or retrieveStrongReferenceVector(). This function is
 * called by retrieveObjectProperties().
 *
 * @param aafd Pointer to the AAF_Data structure.
 * @param Obj  Pointer to the aafObject structure holding this property.
 * @param Def  Pointer to the aafPropertyDef structure defining this property.
 * @param p    Pointer to the aafPropertyIndexEntry_t structure representing the property
 *             in the file.
 * @param v    Pointer to a p->_length long byte array holding the actual property value.
 * @param bo   uint8_t specifying the property's Byte Order. TO BE IMPLEMENTED
 *
 * @TODO Take ByteOrder into account
 */

static int
retrieveProperty (AAF_Data* aafd, aafObject* Obj, aafPropertyDef* Def, aafPropertyIndexEntry_t* p, aafByte_t* v, uint8_t bo);

/**
 * Retrieves the properties for a given aafObject.
 *
 * @param aafd Pointer to the AAF_Data structure.
 * @param Obj  Pointer to the aafObject holding the properties.
 */

static int
retrieveObjectProperties (AAF_Data* aafd, aafObject* Obj);

/**
 * Retrieves a StrongRef Set/Vector Index Node in the Compound File Tree. This function
 * is called by both retrieveStrongReferenceSet() and retrieveStrongReferenceVector().
 *
 * @param  aafd    Pointer to the AAF_Data structure.
 * @param  parent  Pointer to the parent aafObject.
 * @param  refName Pointer to a null terminated string holding the reference name.
 *
 * @return         Pointer to the retrieved Node cfbNode structure.
 */

static cfbNode*
getStrongRefIndexNode (AAF_Data* aafd, aafObject* Parent, const char* refName);

/**
 * Retrieves a StrongRef Set or Vector Entry Node in the Compound File Tree. This
 * function is called by both retrieveStrongReferenceSet() and
 * retrieveStrongReferenceVector().
 *
 * @param aafd     Pointer to the AAF_Data structure.
 * @param parent   Pointer to the parent Index Node.
 * @param baseName Pointer to a null terminated string holding the reference base name.
 * @param index    uint32_t number representing the index number of the reference.
 *
 * @return         Pointer to the retrieved Node cfbNode structure.
 */

static cfbNode*
getStrongRefEntryNode (AAF_Data* aafd, aafObject* Parent, const char* refName, uint32_t index);

/**
 * Retrieves and returns a list of aafPropertyIndexHeader_t.
 * For a given cfbNode, retrieves its /properties Stream Node and returns the stream as
 * a pointer to an aafPropertyIndexHeader_t structure, wich the stream should begin with.
 *
 * @param  aafd Pointer to the AAF_Data structure.
 * @param  node Pointer to a cfbNode structure.
 *
 * @return      Pointer to an aafPropertyIndexHeader_t structure, followed by _entryCount
 *              aafPropertyIndexEntry_t structures.
 */

static aafByte_t*
getNodeProperties (AAF_Data* aafd, cfbNode* node);

/**
 * Retrieves and returns a list of StrongReferenceSet.
 *
 * For a given Index cfbNode, retrieves its Stream and returns it as a pointer to an
 * aafStrongRefSetHeader_t structure, wich the stream should begin with.
 *
 * @param aafd   Pointer to the AAF_Data structure.
 * @param node   Pointer to an Index cfbNode structure.
 * @param parent Pointer to the aafObject parent, only used on error printing.
 *
 * @return       Pointer to an aafStrongRefSetHeader_t structure, followed by _entryCount
 *               aafStrongRefSetEntry_t structures.
 */

static aafStrongRefSetHeader_t*
getStrongRefSetList (AAF_Data* aafd, cfbNode* Node, aafObject* Parent);

/**
 * Retrieves and returns a list of StrongReferenceVectors.
 *
 * For a given Index cfbNode, retrieves its Stream and returns it as a pointer to an
 * aafStrongRefVectorHeader_t structure, wich the stream should begin with.
 *
 * @param  aafd   Pointer to the AAF_Data structure.
 * @param  node   Pointer to an Index cfbNode structure.
 * @param  parent Pointer to the aafObject parent, only used on error printing.
 *
 * @return        Pointer to an aafStrongRefVectorHeader_t structure, followed by
 *                _entryCount aafStrongRefVectorEntry_t structures.
 */

static aafByte_t*
getStrongRefVectorList (AAF_Data* aafd, cfbNode* Node, aafObject* Parent);

AAF_Data*
aaf_alloc (struct aafLog* log)
{
	AAF_Data* aafd = calloc (1, sizeof (AAF_Data));

	if (!aafd) {
		goto err;
	}

	aafd->cfbd = NULL;

	aafd->Identification.CompanyName          = NULL;
	aafd->Identification.ProductName          = NULL;
	aafd->Identification.ProductVersionString = NULL;
	aafd->Identification.Platform             = NULL;

	aafd->Classes = NULL;
	aafd->Objects = NULL;
	aafd->log     = log;

	aafd->cfbd = cfb_alloc (log);

	if (!aafd->cfbd) {
		goto err;
	}

	return aafd;

err:

	if (aafd) {
		if (aafd->cfbd) {
			cfb_release (&aafd->cfbd);
		}
		free (aafd);
	}

	return NULL;
}

int
aaf_load_file (AAF_Data* aafd, const char* file)
{
	if (!aafd || !file)
		return 1;

	aafd->Objects = NULL;
	aafd->Classes = NULL;

	if (cfb_load_file (&aafd->cfbd, file) < 0) {
		return 1;
	}

	/*
	 * NOTE: at least Avid Media Composer doesn't respect
	 * the standard clsid AAFFileKind_Aaf4KBinary identifier.
	 * Therefore isValidAAF() is useless until futher findings..
	 */

	// if ( isValidAAF( aafd ) == 0 ) {
	// 	return 1;
	// }

	if (aafclass_setDefaultClasses (aafd) < 0) {
		return -1;
	}

	if (retrieveObjectTree (aafd) < 0) {
		return -1;
	}

	if (parse_Header (aafd) < 0) {
		return -1;
	}

	if (parse_Identification (aafd) < 0) {
		return -1;
	}

	return 0;
}

void
aaf_release (AAF_Data** aafd)
{
	if (!aafd || !(*aafd))
		return;

	if ((*aafd)->cfbd != NULL)
		cfb_release (&((*aafd)->cfbd));

	aafClass* Class    = NULL;
	aafClass* tmpClass = NULL;

	for (Class = (*aafd)->Classes; Class != NULL; Class = tmpClass) {
		tmpClass = Class->next;

		aafPropertyDef* PDef    = NULL;
		aafPropertyDef* tmpPDef = NULL;

		free (Class->name);

		for (PDef = Class->Properties; PDef != NULL; PDef = tmpPDef) {
			tmpPDef = PDef->next;

			free (PDef->name);
			free (PDef);
		}

		free (Class);
	}

	aafObject* Object    = NULL;
	aafObject* tmpObject = NULL;

	for (Object = (*aafd)->Objects; Object != NULL; Object = tmpObject) {
		tmpObject = Object->nextObj;

		free (Object->Header);
		free (Object->Entry);
		free (Object->Name);

		aafProperty* Prop    = NULL;
		aafProperty* tmpProp = NULL;

		for (Prop = Object->Properties; Prop != NULL; Prop = tmpProp) {
			tmpProp = Prop->next;

			switch (Prop->sf) {
				case SF_STRONG_OBJECT_REFERENCE:
				case SF_STRONG_OBJECT_REFERENCE_SET:
				case SF_STRONG_OBJECT_REFERENCE_VECTOR:
					break;

				default:
					free (Prop->val);
			}

			free (Prop);
		}

		free (Object);
	}

	free ((*aafd)->Identification.CompanyName);
	free ((*aafd)->Identification.ProductName);
	free ((*aafd)->Identification.ProductVersionString);
	free ((*aafd)->Identification.Platform);

	free (*aafd);

	*aafd = NULL;
}

char*
aaf_get_ObjectPath (aafObject* Obj)
{
	static char path[CFB_PATH_NAME_SZ];

	uint32_t offset = CFB_PATH_NAME_SZ;
	path[--offset]  = '\0';

	while (Obj != NULL) {
		for (int i = (int)strlen (Obj->Name) - 1; i >= 0 && offset > 0; i--) {
			path[--offset] = Obj->Name[i];
		}

		if (offset == 0)
			break;

		path[--offset] = '/';

		Obj = Obj->Parent;
	}

	return path + offset;
}

int
_aaf_foreach_ObjectInSet (aafObject** Obj, aafObject* head, const aafUID_t* filter)
{
	if (!(*Obj))
		*Obj = head;
	else
		*Obj = (*Obj)->next;

	if (filter != NULL)
		for (; *Obj != NULL; *Obj = (*Obj)->next)
			if (aafUIDCmp ((*Obj)->Class->ID, filter))
				break;

	return (!(*Obj)) ? 0 : 1;
}

aafObject*
aaf_get_ObjectByWeakRef (aafObject* list, aafWeakRef_t* ref)
{
	if (!ref || !list || !list->Entry) {
		return NULL;
	}

	AAF_Data* aafd = list->aafd;

	/* Target is a Reference Vector */
	if (list->Header->_identificationSize == 0) {
		// debug( "Has local key" );

		for (; list != NULL; list = list->next) {
			if (list->Entry->_localKey == ref->_referencedPropertyIndex) {
				// debug( "list->Entry->_localKey            : 0x%x", list->Entry->_localKey );
				// debug( "list->Header->_identificationSize : %u", list->Header->_identificationSize );
				// debug( "FOUND : 0x%x", list->Entry->_localKey );
				return list;
			}
		}
	}
	/* Target is a Reference Set */
	else {
		for (; list != NULL; list = list->next) {
			if (memcmp (list->Entry->_identification, ref->_identification, ref->_identificationSize) == 0) {
				if (list->Header->_identificationSize != ref->_identificationSize) {
					/* TODO : is it possible ? is it an error ? */
					debug ("list->Header->_identificationSize (%i bytes) doesn't match ref->_identificationSize (%i bytes)", list->Header->_identificationSize, ref->_identificationSize);
				}

				return list;
			}
		}
	}

	return NULL;
}

aafUID_t*
aaf_get_InterpolationIdentificationByWeakRef (AAF_Data* aafd, aafWeakRef_t* InterpolationDefWeakRef)
{
	aafObject* InterpolationDefinition = aaf_get_ObjectByWeakRef (aafd->InterpolationDefinition, InterpolationDefWeakRef);

	if (!InterpolationDefinition) {
		error ("Could not find InterpolationDefinition.");
		return NULL;
	}

	aafUID_t* InterpolationIdentification = aaf_get_propertyValue (InterpolationDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

	if (!InterpolationIdentification) {
		error ("Missing DefinitionObject::Identification.");
		return NULL;
	}

	return InterpolationIdentification;
}

aafUID_t*
aaf_get_OperationIdentificationByWeakRef (AAF_Data* aafd, aafWeakRef_t* OperationDefWeakRef)
{
	aafObject* OperationDefinition = aaf_get_ObjectByWeakRef (aafd->OperationDefinition, OperationDefWeakRef);

	if (!OperationDefinition) {
		error ("Could not retrieve OperationDefinition from dictionary.");
		return NULL;
	}

	aafUID_t* OperationIdentification = aaf_get_propertyValue (OperationDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

	if (!OperationIdentification) {
		error ("Missing DefinitionObject::Identification.");
		return NULL;
	}

	return OperationIdentification;
}

aafUID_t*
aaf_get_ContainerIdentificationByWeakRef (AAF_Data* aafd, aafWeakRef_t* ContainerDefWeakRef)
{
	aafObject* ContainerDefinition = aaf_get_ObjectByWeakRef (aafd->ContainerDefinition, ContainerDefWeakRef);

	if (!ContainerDefinition) {
		warning ("Could not retrieve WeakRef from Dictionary::ContainerDefinitions.");
		return NULL;
	}

	aafUID_t* ContainerIdentification = aaf_get_propertyValue (ContainerDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

	if (!ContainerIdentification) {
		warning ("Missing ContainerDefinition's DefinitionObject::Identification.");
		return NULL;
	}

	return ContainerIdentification;
}

aafUID_t*
aaf_get_DataIdentificationByWeakRef (AAF_Data* aafd, aafWeakRef_t* DataDefWeakRef)
{
	aafObject* DataDefinition = aaf_get_ObjectByWeakRef (aafd->DataDefinition, DataDefWeakRef);

	if (!DataDefinition) {
		warning ("Could not retrieve WeakRef from Dictionary::DataDefinition.");
		return NULL;
	}

	aafUID_t* DataIdentification = aaf_get_propertyValue (DataDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

	if (!DataIdentification) {
		warning ("Missing DataDefinition's DefinitionObject::Identification.");
		return NULL;
	}

	return DataIdentification;
}

aafObject*
aaf_get_ObjectAncestor (aafObject* Obj, const aafUID_t* ClassID)
{
	/*
	 * NOTE : AAFClassID_ContentStorage is the container of Mob and EssenceData,
	 * not of Identification, Dictionary and MetaDictionary. If needed, the func
	 * should work for them too thanks to Obj != NULL.
	 */

	while (Obj != NULL && !aafUIDCmp (Obj->Class->ID, &AAFClassID_ContentStorage)) {
		if (aafUIDCmp (ClassID, Obj->Class->ID)) {
			return Obj;
		}

		if (aaf_ObjectInheritsClass (Obj, ClassID)) {
			return Obj;
		}

		Obj = Obj->Parent;
	}

	return NULL;
}

int
aaf_ObjectInheritsClass (aafObject* Obj, const aafUID_t* classID)
{
	// AAF_Data *aafd = Obj->aafd;

	aafClass* classObj = NULL;
	foreachClassInheritance (classObj, Obj->Class)
	{
		if (aafUIDCmp (classObj->ID, classID)) {
			// debug( "%s is a parent of class %s",
			// 	aaft_ClassIDToText( aafd, classObj->ID ),
			// 	aaft_ClassIDToText( aafd, Obj->Class->ID ) );
			return 1;
		}
	}
	return 0;
}

aafObject*
aaf_get_MobByID (aafObject* Mobs, aafMobID_t* MobID)
{
	aafObject* Mob = NULL;

	if (!MobID)
		return NULL;

	AAF_foreach_ObjectInSet (&Mob, Mobs, NULL)
	{
		aafMobID_t* Current = aaf_get_propertyValue (Mob, PID_Mob_MobID, &AAFTypeID_MobIDType);

		if (!Current || aafMobIDCmp (Current, MobID))
			break;
	}

	return Mob;
}

aafObject*
aaf_get_MobSlotBySlotID (aafObject* MobSlots, aafSlotID_t SlotID)
{
	aafObject* MobSlot = NULL;

	AAF_foreach_ObjectInSet (&MobSlot, MobSlots, NULL)
	{
		aafSlotID_t* CurrentSlotID = aaf_get_propertyValue (MobSlot, PID_MobSlot_SlotID, &AAFTypeID_UInt32);

		if (!CurrentSlotID || *CurrentSlotID == SlotID)
			break;
	}

	return MobSlot;
}

aafObject*
aaf_get_EssenceDataByMobID (AAF_Data* aafd, aafMobID_t* MobID)
{
	aafMobID_t* DataMobID   = NULL;
	aafObject*  EssenceData = NULL;

	for (EssenceData = aafd->EssenceData; EssenceData != NULL; EssenceData = EssenceData->next) {
		DataMobID = aaf_get_propertyValue (EssenceData, PID_EssenceData_MobID, &AAFTypeID_MobIDType);

		if (aafMobIDCmp (DataMobID, MobID))
			break;
	}

	return EssenceData;
}

aafUID_t*
aaf_get_ParamDefIDByName (AAF_Data* aafd, const char* name)
{
	aafUID_t*  ParamDefIdent        = NULL;
	aafObject* ParameterDefinitions = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_ParameterDefinitions, &AAFTypeID_ParameterDefinitionStrongReferenceSet);
	aafObject* ParameterDefinition  = NULL;

	AAF_foreach_ObjectInSet (&ParameterDefinition, ParameterDefinitions, NULL)
	{
		char* paramName = aaf_get_propertyValue (ParameterDefinition, PID_DefinitionObject_Name, &AAFTypeID_String);

		if (!paramName) {
			continue;
		}

		if (strcmp (paramName, name) == 0) {
			ParamDefIdent = aaf_get_propertyValue (ParameterDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);
			free (paramName);
			break;
		}

		free (paramName);
	}

	return ParamDefIdent;
}

void*
aaf_get_TaggedValueByName (AAF_Data* aafd, aafObject* TaggedValueVector, const char* name, const aafUID_t* type)
{
	struct aafLog* log = aafd->log;

	aafObject* TaggedValue = NULL;

	AAF_foreach_ObjectInSet (&TaggedValue, TaggedValueVector, NULL)
	{
		if (!aafUIDCmp (TaggedValue->Class->ID, &AAFClassID_TaggedValue)) {
			LOG_BUFFER_WRITE (log, "     %sObject > %s\n",
			                  ANSI_COLOR_RESET (log),
			                  aaft_ClassIDToText (aafd, TaggedValue->Class->ID));
			continue;
		}

		char*          taggedName     = aaf_get_propertyValue (TaggedValue, PID_TaggedValue_Name, &AAFTypeID_String);
		aafIndirect_t* taggedIndirect = aaf_get_propertyValue (TaggedValue, PID_TaggedValue_Value, &AAFTypeID_Indirect);

		if (strcmp (taggedName, name) == 0) {
			if (aafUIDCmp (&taggedIndirect->TypeDef, type)) {
				debug ("Found TaggedValue \"%s\" of type %s",
				       taggedName,
				       aaft_TypeIDToText (&taggedIndirect->TypeDef));

				free (taggedName);

				void* value = aaf_get_indirectValue (aafd, taggedIndirect, type);

				return value;
			}

			debug ("Got TaggedValue \"%s\" but of type %s instead of %s",
			       taggedName,
			       aaft_TypeIDToText (&taggedIndirect->TypeDef),
			       aaft_TypeIDToText (type));
		}
		// LOG_BUFFER_WRITE( log, "     %sTagged > Name: %s%s%s%*s      Value: %s(%s)%s %s%s%s",
		// 	ANSI_COLOR_RESET(log),
		// 	ANSI_COLOR_DARKGREY(log),
		// 	(name) ? name : "<unknown>",
		// 	ANSI_COLOR_RESET(log),
		// 	(name) ? (size_t)(24-(int)strlen(name)) : (size_t)(24-strlen("<unknown>")), " ",
		// 	ANSI_COLOR_DARKGREY(log),
		// 	aaft_TypeIDToText( &taggedIndirect->TypeDef ),
		// 	ANSI_COLOR_RESET(log),
		// 	ANSI_COLOR_DARKGREY(log),
		// 	aaft_IndirectValueToText( aafd, taggedIndirect ),
		// 	ANSI_COLOR_RESET(log) );

		free (taggedName);
	}

	debug ("TaggedValue not found \"%s\"", name);

	return NULL;
}

/*
 * TODO Works when the property was retrieved from MetaDictionary. What if the property is standard ?
 */

aafPID_t
aaf_get_PropertyIDByName (AAF_Data* aafd, const char* name)
{
	aafClass* Class = NULL;

	foreachClass (Class, aafd->Classes)
	{
		aafPropertyDef* PDef = NULL;

		foreachPropertyDefinition (PDef, Class->Properties)
		{
			if (PDef->name != NULL && strcmp (PDef->name, name) == 0) {
				return PDef->pid;
			}
		}
	}

	return 0;
}

aafUID_t*
aaf_get_OperationDefIDByName (AAF_Data* aafd, const char* OpDefName)
{
	aafObject* OperationDefinitions = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_OperationDefinitions, &AAFTypeID_OperationDefinitionStrongReferenceSet);
	aafObject* OperationDefinition  = NULL;

	while (_aaf_foreach_ObjectInSet (&OperationDefinition, OperationDefinitions, NULL)) {
		aafUID_t* OpDefIdent = aaf_get_propertyValue (OperationDefinition, PID_DefinitionObject_Identification, &AAFTypeID_AUID);

		char* name = aaf_get_propertyValue (OperationDefinition, PID_DefinitionObject_Name, &AAFTypeID_String);

		if (strcmp (name, OpDefName) == 0) {
			free (name);
			return OpDefIdent;
		}

		free (name);
	}

	return NULL;
}

aafProperty*
aaf_get_property (aafObject* Obj, aafPID_t pid)
{
	if (!Obj)
		return NULL;

	AAF_Data* aafd = Obj->aafd;

	aafProperty* Prop = NULL;

	for (Prop = Obj->Properties; Prop != NULL; Prop = Prop->next)
		if (Prop->pid == pid)
			break;

	if (!Prop) {
		aafPropertyDef* PDef = aafclass_getPropertyDefinitionByID (Obj->Class, pid);

		if (!PDef) {
			warning ("Could not retrieve 0x%04x (%s) of Class %s",
			         pid,
			         aaft_PIDToText (aafd, pid),
			         aaft_ClassIDToText (aafd, Obj->Class->ID));
			return NULL;
		}

		if (PDef->isReq) {
			error ("Could not retrieve %s required property 0x%04x (%s)",
			       aaft_ClassIDToText (aafd, Obj->Class->ID),
			       pid,
			       aaft_PIDToText (aafd, pid));
		} else {
			debug ("Could not retrieve %s optional property 0x%04x (%s)",
			       aaft_ClassIDToText (aafd, Obj->Class->ID),
			       pid,
			       aaft_PIDToText (aafd, pid));
		}
	}

	return Prop;
}

void*
aaf_get_propertyValue (aafObject* Obj, aafPID_t pid, const aafUID_t* typeID)
{
	if (!Obj) {
		return NULL;
	}

	AAF_Data*    aafd = Obj->aafd;
	aafProperty* Prop = aaf_get_property (Obj, pid);

	if (!Prop) {
		return NULL;
	}

	void*    value = Prop->val;
	uint16_t len   = Prop->len;

	if (Prop->sf == SF_DATA_STREAM || aafUIDCmp (typeID, &AAFTypeID_Indirect)) {
		/*
		 * DATA_STREAM stored form and IndirectValues start with a byte identifying byte order : 0x4c, 0x42, 0x55
		 * We must skip that byte.
		 */
		value = (void*)(((char*)value) + 1);
		len--;
	}

	if (aafUIDCmp (typeID, &AAFTypeID_String)) {
		if (((uint16_t*)value)[(len / 2) - 1] != 0x0000) {
			error ("Object %s string property 0x%04x (%s) does not end with NULL",
			       aaft_ClassIDToText (aafd, Obj->Class->ID),
			       pid,
			       aaft_PIDToText (aafd, pid));
			return NULL;
		}

		return cfb_w16toUTF8 (value, len);
	}

	if (aafUIDCmp (typeID, &AAFTypeID_Indirect)) {
		/*
		 * In case of Indirect with string value we check NULL termination here,
		 * because when calling next aaf_get_indirectValue() we wont have access
		 * to Prop->len anymore.
		 */

		aafIndirect_t* Indirect = value;

		if (aafUIDCmp (&Indirect->TypeDef, &AAFTypeID_String) && ((uint16_t*)value)[(len / 2) - 1] != 0x0000) {
			error ("Object %s Indirect::string property 0x%04x (%s) does not end with NULL",
			       aaft_ClassIDToText (aafd, Obj->Class->ID),
			       pid,
			       aaft_PIDToText (aafd, pid));
			return NULL;
		}
	}

	if ((aafUIDCmp (typeID, &AAFTypeID_Boolean) && len != sizeof (aafBoolean_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_Int8) && len != sizeof (int8_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_UInt8) && len != sizeof (uint8_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_Int16) && len != sizeof (int16_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_UInt16) && len != sizeof (uint16_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_Int32) && len != sizeof (int32_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_UInt32) && len != sizeof (uint32_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_Int64) && len != sizeof (int64_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_UInt64) && len != sizeof (uint64_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_PositionType) && len != sizeof (aafPosition_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_LengthType) && len != sizeof (aafLength_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_Rational) && len != sizeof (aafRational_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_TimeStamp) && len != sizeof (aafTimeStamp_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_VersionType) && len != sizeof (aafVersionType_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_ProductVersion) && len != sizeof (aafProductVersion_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_UsageType) && len != sizeof (aafUID_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_AUID) && len != sizeof (aafUID_t)) ||
	    (aafUIDCmp (typeID, &AAFTypeID_MobIDType) && len != sizeof (aafMobID_t))) {
		error ("Object %s property 0x%04x (%s) size (%u) does not match type %s",
		       aaft_ClassIDToText (aafd, Obj->Class->ID),
		       pid,
		       aaft_PIDToText (aafd, pid),
		       len,
		       aaft_TypeIDToText (typeID));
		return NULL;
	}

	return value;
}

void*
aaf_get_indirectValue (AAF_Data* aafd, aafIndirect_t* Indirect, const aafUID_t* typeDef)
{
	if (!Indirect) {
		error ("Indirect is NULL");
		return NULL;
	}

	if (typeDef && aafUIDCmp (&Indirect->TypeDef, typeDef) == 0) {
		error ("Requested Indirect value of type %s but has type %s",
		       aaft_TypeIDToText (typeDef),
		       aaft_TypeIDToText (&Indirect->TypeDef));
		return NULL;
	}

	if (aafUIDCmp (typeDef, &AAFTypeID_String)) {
		/*
		 * Indirect->Value is guaranted by aaf_get_property() to be NULL terminated
		 */

		size_t indirectValueSize = 0;

		for (size_t i = 0; (i % 2 || !(Indirect->Value[i] == 0x00 && Indirect->Value[i + 1] == 0x00)); i++) {
			indirectValueSize++;
		}

		indirectValueSize += 2;

		uint16_t* w16 = malloc (indirectValueSize);

		if (!w16) {
			error ("Out of memory");
			return NULL;
		}

		memcpy (w16, Indirect->Value, indirectValueSize);

		char* str = cfb_w16toUTF8 (w16, indirectValueSize);

		free (w16);

		return str;
	}

	return &Indirect->Value;
}

static int
parse_Header (AAF_Data* aafd)
{
	aafObject* Header = aafd->Header.obj;

	if (!Header) {
		error ("Missing Header Object.");
		return -1;
	}

	int16_t* ByteOrder = aaf_get_propertyValue (Header, PID_Header_ByteOrder, &AAFTypeID_Int16);

	if (!ByteOrder) {
		warning ("Missing Header::ByteOrder.");
	} else {
		aafd->Header.ByteOrder = *ByteOrder;
	}

	aafTimeStamp_t* LastModified = aaf_get_propertyValue (Header, PID_Header_LastModified, &AAFTypeID_TimeStamp);

	if (!LastModified) {
		warning ("Missing Header::LastModified.");
	} else {
		aafd->Header.LastModified = LastModified;
	}

	aafVersionType_t* Version = aaf_get_propertyValue (Header, PID_Header_Version, &AAFTypeID_VersionType);

	if (!Version) {
		warning ("Missing Header::Version.");
	} else {
		aafd->Header.Version = Version;
	}

	uint32_t* ObjectModelVersion = aaf_get_propertyValue (Header, PID_Header_ObjectModelVersion, &AAFTypeID_UInt32);

	if (!ObjectModelVersion) {
		warning ("Missing Header::ObjectModelVersion.");
	} else {
		aafd->Header.ObjectModelVersion = *ObjectModelVersion;
	}

	const aafUID_t* OperationalPattern = aaf_get_propertyValue (Header, PID_Header_OperationalPattern, &AAFTypeID_AUID);

	if (!OperationalPattern) {
		warning ("Missing Header::OperationalPattern.");
		aafd->Header.OperationalPattern = (const aafUID_t*)&AUID_NULL;
	} else {
		aafd->Header.OperationalPattern = OperationalPattern;
	}

	return 0;
}

static int
parse_Identification (AAF_Data* aafd)
{
	aafObject* Identif = aafd->Identification.obj;

	if (!Identif) {
		error ("Missing Identification Object.");
		return -1;
	}

	char* Company = aaf_get_propertyValue (Identif, PID_Identification_CompanyName, &AAFTypeID_String);

	if (!Company) {
		warning ("Missing Identification::CompanyName.");
	} else {
		aafd->Identification.CompanyName = Company;
	}

	char* ProductName = aaf_get_propertyValue (Identif, PID_Identification_ProductName, &AAFTypeID_String);

	if (!ProductName) {
		warning ("Missing Identification::ProductName.");
	} else {
		aafd->Identification.ProductName = ProductName;
	}

	aafProductVersion_t* ProductVersion = aaf_get_propertyValue (Identif, PID_Identification_ProductVersion, &AAFTypeID_ProductVersion);

	if (!ProductVersion) {
		warning ("Missing Identification::ProductVersion.");
	} else {
		aafd->Identification.ProductVersion = ProductVersion;
	}

	char* ProductVersionString = aaf_get_propertyValue (Identif, PID_Identification_ProductVersionString, &AAFTypeID_String);

	if (!ProductVersionString) {
		warning ("Missing Identification::ProductVersionString.");
	} else {
		aafd->Identification.ProductVersionString = ProductVersionString;
	}

	aafUID_t* ProductID = aaf_get_propertyValue (Identif, PID_Identification_ProductID, &AAFTypeID_AUID);

	if (!ProductID) {
		warning ("Missing Identification::ProductID.");
	} else {
		aafd->Identification.ProductID = ProductID;
	}

	aafTimeStamp_t* Date = aaf_get_propertyValue (Identif, PID_Identification_Date, &AAFTypeID_TimeStamp);

	if (!Date) {
		warning ("Missing Identification::Date.");
	} else {
		aafd->Identification.Date = Date;
	}

	aafProductVersion_t* ToolkitVersion = aaf_get_propertyValue (Identif, PID_Identification_ToolkitVersion, &AAFTypeID_ProductVersion);

	if (!ToolkitVersion) {
		warning ("Missing Identification::ToolkitVersion.");
	} else {
		aafd->Identification.ToolkitVersion = ToolkitVersion;
	}

	char* Platform = aaf_get_propertyValue (Identif, PID_Identification_Platform, &AAFTypeID_String);

	if (!Platform) {
		warning ("Missing Identification::Platform.");
	} else {
		aafd->Identification.Platform = Platform;
	}

	aafUID_t* GenerationAUID = aaf_get_propertyValue (Identif, PID_Identification_GenerationAUID, &AAFTypeID_AUID);

	if (!GenerationAUID) {
		warning ("Missing Identification::GenerationAUID.");
	} else {
		aafd->Identification.GenerationAUID = GenerationAUID;
	}

	return 0;
}

// static int isValidAAF( AAF_Data *aafd )
// {
// 	aafUID_t *hdrClsID = (aafUID_t*)&aafd->cfbd->hdr->_clsid;
//
// 	if ( aafUIDCmp( hdrClsID, &AAFFileKind_Aaf512Binary ) ||
// 		 aafUIDCmp( hdrClsID, &AAFFileKind_Aaf4KBinary  ) )
// 			return 1;
//
// //  warning( "Unsuported AAF encoding (%s).", aaft_FileKindToText( hdrClsID ) );
//
// 	return 0;
// }

static void
setObjectShortcuts (AAF_Data* aafd)
{
	//  aafd->Root = aafd->Root;

	aafd->Header.obj = aaf_get_propertyValue (aafd->Root, PID_Root_Header, &AAFUID_NULL);
	//  aafd->MetaDictionary          = aaf_get_propertyValue( aafd->Root,       PID_Root_MetaDictionary                 );

	aafd->ClassDefinition = aaf_get_propertyValue (aafd->MetaDictionary, PID_MetaDictionary_ClassDefinitions, &AAFTypeID_ClassDefinitionStrongReferenceSet);
	aafd->TypeDefinition  = aaf_get_propertyValue (aafd->MetaDictionary, PID_MetaDictionary_TypeDefinitions, &AAFTypeID_TypeDefinitionStrongReferenceSet);

	aafd->Identification.obj = aaf_get_propertyValue (aafd->Header.obj, PID_Header_IdentificationList, &AAFTypeID_IdentificationStrongReferenceVector);
	aafd->Content            = aaf_get_propertyValue (aafd->Header.obj, PID_Header_Content, &AAFTypeID_ContentStorageStrongReference);
	aafd->Dictionary         = aaf_get_propertyValue (aafd->Header.obj, PID_Header_Dictionary, &AAFTypeID_DictionaryStrongReference);

	aafd->Mobs        = aaf_get_propertyValue (aafd->Content, PID_ContentStorage_Mobs, &AAFTypeID_MobStrongReferenceSet);
	aafd->EssenceData = aaf_get_propertyValue (aafd->Content, PID_ContentStorage_EssenceData, &AAFTypeID_EssenceDataStrongReferenceSet);

	aafd->OperationDefinition     = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_OperationDefinitions, &AAFTypeID_OperationDefinitionStrongReferenceSet);
	aafd->ParameterDefinition     = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_ParameterDefinitions, &AAFTypeID_ParameterDefinitionStrongReferenceSet);
	aafd->DataDefinition          = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_DataDefinitions, &AAFTypeID_DataDefinitionStrongReferenceSet);
	aafd->PluginDefinition        = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_PluginDefinitions, &AAFTypeID_PluginDefinitionStrongReferenceSet);
	aafd->CodecDefinition         = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_CodecDefinitions, &AAFTypeID_CodecDefinitionStrongReferenceSet);
	aafd->ContainerDefinition     = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_ContainerDefinitions, &AAFTypeID_ContainerDefinitionStrongReferenceSet);
	aafd->InterpolationDefinition = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_InterpolationDefinitions, &AAFTypeID_InterpolationDefinitionStrongReferenceSet);
	aafd->KLVDataDefinition       = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_KLVDataDefinitions, &AAFTypeID_KLVDataDefinitionStrongReferenceSet);
	aafd->TaggedValueDefinition   = aaf_get_propertyValue (aafd->Dictionary, PID_Dictionary_TaggedValueDefinitions, &AAFTypeID_TaggedValueDefinitionStrongReferenceSet);
}

static int
retrieveObjectTree (AAF_Data* aafd)
{
	int        rc         = 0;
	aafByte_t* propStream = NULL;

	cfbNode* Node = &aafd->cfbd->nodes[0];

	aafClass* Class = aafclass_getClassByID (aafd, (aafUID_t*)&Node->_clsId);

	if (!Class) {
		error ("Could not retrieve class by id");
		goto err;
	}

	aafd->Root = newObject (aafd, Node, Class, NULL);

	if (!aafd->Root) {
		goto err;
	}

	propStream = getNodeProperties (aafd, aafd->Root->Node);

	if (!propStream) {
		error ("Could not retrieve properties for %s.", aaf_get_ObjectPath (aafd->Root));
		goto err;
	}

	aafPropertyIndexHeader_t Header;
	aafPropertyIndexEntry_t  Prop;

	aafPropertyIndexEntry_t AAFHeaderProp;
	aafPropertyIndexEntry_t AAFMetaDcProp;

	memcpy (&Header, propStream, sizeof (aafPropertyIndexHeader_t));

	aafByte_t* AAFHeaderVal = NULL;
	aafByte_t* AAFMetaDcVal = NULL;

	aafByte_t* value = NULL;

	aafPropertyDef* PDef = NULL;

	uint32_t i           = 0;
	size_t   valueOffset = 0;

	foreachPropertyEntry (propStream, Header, Prop, value, valueOffset, i)
	{
		if (Prop._pid == PID_Root_Header) {
			memcpy (&AAFHeaderProp, &Prop, sizeof (aafPropertyIndexEntry_t));
			AAFHeaderVal = value;
		}

		if (Prop._pid == PID_Root_MetaDictionary) {
			memcpy (&AAFMetaDcProp, &Prop, sizeof (aafPropertyIndexEntry_t));
			AAFMetaDcVal = value;
		}
	}

	PDef = aafclass_getPropertyDefinitionByID (aafd->Root->Class, PID_Root_MetaDictionary);

	/* Start recursive parsing of /Root/Header/{*} */

	rc = retrieveProperty (aafd, aafd->Root, PDef, &AAFMetaDcProp, AAFMetaDcVal, Header._byteOrder);

	if (rc < 0) {
		error ("Could not retrieve property %s.", aaft_PIDToText (aafd, PDef->pid));
		goto err;
	}

	/*
	 * Retrieve MetaDictionary.
	 */

	aafObject* MetaDic = aaf_get_propertyValue (aafd->Root, PID_Root_MetaDictionary, &AAFUID_NULL);

	if (!MetaDic) {
		error ("Missing PID_Root_MetaDictionary.");
		goto err;
	}

	aafObject* ClassDefs = aaf_get_propertyValue (MetaDic, PID_MetaDictionary_ClassDefinitions, &AAFTypeID_ClassDefinitionStrongReferenceSet);

	if (!ClassDefs) {
		error ("Missing PID_MetaDictionary_ClassDefinitions.");
		goto err;
	}

	aafObject* ClassDef = NULL;

	AAF_foreach_ObjectInSet (&ClassDef, ClassDefs, NULL)
	{
		retrieveMetaDictionaryClass (aafd, ClassDef);
	}

	PDef = aafclass_getPropertyDefinitionByID (aafd->Root->Class, PID_Root_Header);

	/* Starts recursive parsing of /Root/Header/{*} */

	rc = retrieveProperty (aafd, aafd->Root, PDef, &AAFHeaderProp, AAFHeaderVal, Header._byteOrder);

	if (rc < 0) {
		error ("Could not retrieve property %s.", aaft_PIDToText (aafd, PDef->pid));
		goto err;
	}

	setObjectShortcuts (aafd);

	rc = 0;
	goto end;

err:
	rc = -1;

end:

	free (propStream);

	return rc;
}

static aafClass*
retrieveMetaDictionaryClass (AAF_Data* aafd, aafObject* TargetClassDef)
{
	aafObject* MetaDic = aaf_get_propertyValue (aafd->Root, PID_Root_MetaDictionary, &AAFUID_NULL);

	if (!MetaDic) { /* req */
		debug ("Could not retrieve PID_Root_MetaDictionary property from Root.");
		return NULL;
	}

	aafObject* ClassDefs = aaf_get_propertyValue (MetaDic, PID_MetaDictionary_ClassDefinitions, &AAFTypeID_ClassDefinitionStrongReferenceSet);
	aafObject* ClassDef  = NULL;

	if (!ClassDefs) { /* opt */
		debug ("Could not retrieve PID_MetaDictionary_ClassDefinitions property from MetaDic.");
		return NULL;
	}

	AAF_foreach_ObjectInSet (&ClassDef, ClassDefs, NULL)
	{
		if (ClassDef == TargetClassDef)
			break;
	}

	if (!ClassDef) {
		error ("Could not retrieve ClassDefinition %p", (void*)TargetClassDef);
		return NULL;
	}

	aafUID_t* ClassID = aaf_get_propertyValue (ClassDef, PID_MetaDefinition_Identification, &AAFTypeID_AUID);

	if (!ClassID) { /* req */
		error ("Could not retrieve PID_MetaDefinition_Identification property from ClassDef.");
		return NULL;
	}

	aafWeakRef_t* parent = aaf_get_propertyValue (ClassDef, PID_ClassDefinition_ParentClass, &AAFTypeID_ClassDefinitionWeakReference);

	if (!parent) {
		error ("Could not retrieve PID_ClassDefinition_ParentClass property from ClassDef.");
		return NULL;
	}

	aafObject* Parent = aaf_get_ObjectByWeakRef (ClassDefs, parent);

	if (!Parent) {
		error ("Could not retrieve object by weakRef (PID_ClassDefinition_ParentClass)");
		return NULL;
	}

	aafClass* ParentClass = NULL;

	if (Parent != ClassDef) {
		ParentClass = retrieveMetaDictionaryClass (aafd, Parent);
	} else if (aafUIDCmp (ClassID, &AAFClassID_InterchangeObject) == 0 &&
	           aafUIDCmp (ClassID, &AAFClassID_MetaDefinition) == 0 &&
	           aafUIDCmp (ClassID, &AAFClassID_MetaDictionary) == 0) {
		/*
		 * TODO: what is this ? when does it happen ?
		 */
		error ("Parent's Class equals Child's : %s.", aaft_ClassIDToText (aafd, ClassID));
		return NULL;
	}

	aafClass* Class = aafclass_getClassByID (aafd, ClassID);

	if (!Class) {
		aafBoolean_t* isCon = aaf_get_propertyValue (ClassDef, PID_ClassDefinition_IsConcrete, &AAFTypeID_Boolean);

		if (!isCon) {
			error ("Missing ClassDefinition::IsConcrete.");
			return NULL;
		}

		Class = aafclass_defineNewClass (aafd, ClassID, *isCon, ParentClass);

		if (!Class) {
			error ("Could set new class");
			return NULL;
		}

		Class->meta = 1;
		Class->name = aaf_get_propertyValue (ClassDef, PID_MetaDefinition_Name, &AAFTypeID_String);

		if (!Class->name) {
			debug ("Could not retrieve PID_MetaDefinition_Name property from ClassDef (%s)", aaft_ClassIDToText (aafd, ClassID));
		}
	} else {
		/* if class is standard, we only set its name */
		if (!Class->name) {
			Class->name = aaf_get_propertyValue (ClassDef, PID_MetaDefinition_Name, &AAFTypeID_String);

			if (!Class->name) {
				debug ("Could not retrieve PID_MetaDefinition_Name property from ClassDef (%s)", aaft_ClassIDToText (aafd, ClassID));
			}
		}
	}

	aafObject* Props = aaf_get_propertyValue (ClassDef, PID_ClassDefinition_Properties, &AAFTypeID_PropertyDefinitionStrongReferenceSet);

	if (!Props) { /* opt */
		debug ("Could not retrieve PID_ClassDefinition_Properties property from ClassDef (%s)", aaft_ClassIDToText (aafd, ClassID));
	}

	aafObject* Prop = NULL;

	AAF_foreach_ObjectInSet (&Prop, Props, NULL)
	{
		aafPID_t* Pid = aaf_get_propertyValue (Prop, PID_PropertyDefinition_LocalIdentification, &AAFTypeID_UInt16);

		if (!Pid) {
			error ("Missing PropertyDefinition::LocalIdentification.");
			return NULL;
		}

		aafBoolean_t* isOpt = aaf_get_propertyValue (Prop, PID_PropertyDefinition_IsOptional, &AAFTypeID_Boolean);

		if (!isOpt) {
			error ("Missing PropertyDefinition::IsOptional.");
			return NULL;
		}

		/*
		 * We skip all the properties that were already defined in aafclass_setDefaultClasses().
		 */

		aafPropertyDef* PDef = propertyIdExistsInClass (Class, *Pid);

		if (!PDef) {
			attachNewProperty (Class, PDef, *Pid, (*isOpt) ? 0 : 1);
			PDef->meta = 1;
		} else {
			// debug( "Property %d exists.", *Pid );
			continue;
		}

		PDef->name = aaf_get_propertyValue (Prop, PID_MetaDefinition_Name, &AAFTypeID_String);

		if (!PDef->name) {
			warning ("Could not retrieve PID_MetaDefinition_Name property from PropertyDefinition.");
		}

		aafObject* TypeDefs = aaf_get_propertyValue (MetaDic, PID_MetaDictionary_TypeDefinitions, &AAFTypeID_TypeDefinitionStrongReferenceSet);

		if (!TypeDefs) {
			error ("Missing TypeDefinitions from MetaDictionary");
			return NULL;
		}

		aafWeakRef_t* WeakRefToType = aaf_get_propertyValue (Prop, PID_PropertyDefinition_Type, &AAFTypeID_PropertyDefinitionWeakReference);

		if (!WeakRefToType) {
			error ("Missing PID_PropertyDefinition_Type");
			return NULL;
		}

		aafObject* TypeDef = aaf_get_ObjectByWeakRef (TypeDefs, WeakRefToType);

		if (!TypeDef) {
			error ("Could not retrieve TypeDefinition from dictionary.");
			return NULL;
		}

		aafUID_t* typeUID = aaf_get_propertyValue (TypeDef, PID_MetaDefinition_Identification, &AAFTypeID_AUID);

		if (!typeUID) { /* req */
			error ("Missing PID_MetaDefinition_Identification");
			return NULL;
		}

		/*
		 * Looks like nobody cares about AAF standard TypeDefinition. All observed files
		 * had incorrect values for Type Name and Identification, even Avid's files. So...
		 */

		memcpy (&PDef->type, typeUID, sizeof (aafUID_t));

		// char *typeName = aaf_get_propertyValue( TypeDef, PID_MetaDefinition_Name, &AAFTypeID_String );
		//
		// debug( "TypeName :  %s (%s) |  name : %s.",
		// 	typeName,
		// 	aaft_TypeIDToText( typeUID ),
		// 	PDef->name );
		//
		// free( typeName );
	}

	return Class;
}

static aafObject*
newObject (AAF_Data* aafd, cfbNode* Node, aafClass* Class, aafObject* Parent)
{
	aafObject* Obj = calloc (1, sizeof (aafObject));

	if (!Obj) {
		error ("Out of memory");
		return NULL;
	}

	Obj->Name       = cfb_w16toUTF8 (Node->_ab, Node->_cb);
	Obj->aafd       = aafd;
	Obj->Class      = Class;
	Obj->Node       = Node;
	Obj->Properties = NULL;
	Obj->Parent     = Parent;
	Obj->Header     = NULL;
	Obj->Entry      = NULL;

	Obj->next     = NULL;
	Obj->prev     = NULL;
	Obj->nextObj  = aafd->Objects;
	aafd->Objects = Obj;

	return Obj;
}

static aafProperty*
newProperty (AAF_Data* aafd, aafPropertyDef* Def)
{
	aafProperty* Prop = calloc (1, sizeof (aafProperty));

	if (!Prop) {
		error ("Out of memory");
		return NULL;
	}

	Prop->pid = Def->pid;
	Prop->def = Def;

	return Prop;
}

static aafPropertyDef*
propertyIdExistsInClass (aafClass* Class, aafPID_t Pid)
{
	aafPropertyDef* PDef = NULL;

	foreachPropertyDefinition (PDef, Class->Properties) if (PDef->pid == Pid) return PDef;

	return NULL;
}

static int
setObjectStrongRefSet (aafObject* Obj, aafStrongRefSetHeader_t* Header, aafStrongRefSetEntry_t* Entry)
{
	AAF_Data* aafd = Obj->aafd;

	Obj->Header = malloc (sizeof (aafStrongRefSetHeader_t));

	if (!Obj->Header) {
		error ("Out of memory");
		return -1;
	}

	memcpy (Obj->Header, Header, sizeof (aafStrongRefSetHeader_t));

	/* Real entrySize, taking _identification into account. */
	uint32_t entrySize = sizeof (aafStrongRefSetEntry_t) + Header->_identificationSize;

	Obj->Entry = malloc (entrySize);

	if (!Obj->Entry) {
		error ("Out of memory");
		return -1;
	}

	memcpy (Obj->Entry, Entry, entrySize);

	return 0;
}

static int
setObjectStrongRefVector (aafObject* Obj, aafStrongRefVectorHeader_t* Header, aafStrongRefVectorEntry_t* Entry)
{
	/*
	 * aafStrongRefVectorHeader_t  and  aafStrongRefSetHeader_t begins with the same
	 * data bytes,  so  we  can  safely  memcpy to the first one from the second one,
	 * the remaining bytes simply remaining null.
	 * The same applies  to aafStrongRefVectorEntry_t and aafStrongRefVectorHeader_t.
	 */

	AAF_Data* aafd = Obj->aafd;

	Obj->Header = calloc (1, sizeof (aafStrongRefSetHeader_t));

	if (!Obj->Header) {
		error ("Out of memory");
		return -1;
	}

	memcpy (Obj->Header, Header, sizeof (aafStrongRefVectorHeader_t));

	Obj->Entry = calloc (1, sizeof (aafStrongRefSetEntry_t));

	if (!Obj->Entry) {
		error ("Out of memory");
		return -1;
	}

	memcpy (Obj->Entry, Entry, sizeof (aafStrongRefVectorEntry_t));

	return 0;
}

static int
retrieveStrongReference (AAF_Data* aafd, aafProperty* Prop, aafObject* Parent)
{
	/*
	 * Initial property value is a unicode string holding the name of a child node.
	 * This child node being the object referenced, we store that object directly
	 * as the property value, instead of the initial child node name.
	 */

	char* name = cfb_w16toUTF8 (Prop->val, Prop->len);

	free (Prop->val);
	Prop->val = NULL;

	cfbNode* Node = cfb_getChildNode (aafd->cfbd, name, Parent->Node);

	free (name);

	if (!Node) {
		error ("Could not find child node.");
		return -1;
	}

	aafClass* Class = aafclass_getClassByID (aafd, (aafUID_t*)&Node->_clsId);

	if (!Class) {
		error ("Could not retrieve Class %s @ \"%s\".",
		       aaft_ClassIDToText (aafd, (aafUID_t*)&Node->_clsId),
		       aaf_get_ObjectPath (Parent));
		return -1;
	}

	Prop->val = newObject (aafd, Node, Class, Parent);

	if (!Prop->val) {
		return -1;
	}

	int rc = retrieveObjectProperties (aafd, Prop->val);

	if (rc < 0) {
		return -1;
	}

	return 0;
}

static int
retrieveStrongReferenceSet (AAF_Data* aafd, aafProperty* Prop, aafObject* Parent)
{
	aafStrongRefSetHeader_t* Header = NULL;
	aafStrongRefSetEntry_t*  Entry  = NULL;

	char* refName = cfb_w16toUTF8 (Prop->val, Prop->len);

	free (Prop->val);
	Prop->val = NULL;

	cfbNode* Node = getStrongRefIndexNode (aafd, Parent, refName);

	if (!Node) {
		error ("Could not retrieve StrongReferenceSet's Index node.");
		goto err;
	}

	Header = getStrongRefSetList (aafd, Node, Parent);

	if (!Header) {
		error ("Could not retrieve StrongReferenceSet's CFB Stream.");
		goto err;
	}

	Entry = malloc (sizeof (aafStrongRefSetEntry_t) + Header->_identificationSize);

	if (!Entry) {
		error ("Out of memory");
		goto err;
	}

	memset (Entry, 0x00, sizeof (aafStrongRefSetEntry_t));

	uint32_t i  = 0;
	int      rc = 0;

	foreachStrongRefSetEntry (Header, (*Entry), i)
	{
		Node = getStrongRefEntryNode (aafd, Parent, refName, Entry->_localKey);

		if (!Node) {
			continue;
		}

		aafClass* Class = aafclass_getClassByID (aafd, (aafUID_t*)&Node->_clsId);

		if (!Class) {
			error ("Could not retrieve Class %s.",
			       aaft_ClassIDToText (aafd, (aafUID_t*)&Node->_clsId));
			continue;
		}

		aafObject* Obj = newObject (aafd, Node, Class, Parent);

		if (!Obj) {
			goto err;
		}

		rc = setObjectStrongRefSet (Obj, Header, Entry);

		if (rc < 0) {
			goto err;
		}

		rc = retrieveObjectProperties (aafd, Obj);

		if (rc < 0) {
			goto err;
		}

		Obj->next = Prop->val;
		Prop->val = Obj;
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:

	free (refName);
	free (Header);
	free (Entry);

	return rc;
}

static int
retrieveStrongReferenceVector (AAF_Data* aafd, aafProperty* Prop, aafObject* Parent)
{
	int        rc           = 0;
	aafByte_t* vectorStream = NULL;

	char* refName = cfb_w16toUTF8 (Prop->val, Prop->len);

	free (Prop->val);
	Prop->val = NULL;

	cfbNode* Node = getStrongRefIndexNode (aafd, Parent, refName);

	if (!Node) {
		goto err;
	}

	vectorStream = getStrongRefVectorList (aafd, Node, Parent);

	if (!vectorStream) {
		error ("Could not retrieve StrongRefVectorList");
		goto err;
	}

	aafStrongRefVectorHeader_t Header;
	aafStrongRefVectorEntry_t  Entry;

	memcpy (&Header, vectorStream, sizeof (aafStrongRefVectorHeader_t));

	uint32_t i = 0;

	foreachStrongRefVectorEntry (vectorStream, Header, Entry, i)
	{
		Node = getStrongRefEntryNode (aafd, Parent, refName, Entry._localKey);

		if (!Node) {
			continue;
		}

		aafClass* Class = aafclass_getClassByID (aafd, (aafUID_t*)&Node->_clsId);

		if (!Class) {
			warning ("Could not retrieve Class ID %s.",
			         aaft_ClassIDToText (aafd, (aafUID_t*)&Node->_clsId));
			continue;
		}

		aafObject* Obj = newObject (aafd, Node, Class, Parent);

		if (!Obj) {
			goto err;
		}

		rc = setObjectStrongRefVector (Obj, &Header, &Entry);

		if (rc < 0) {
			goto err;
		}

		rc = retrieveObjectProperties (aafd, Obj);

		if (rc < 0) {
			goto err;
		}

		/*
		 * Vectors are ordered.
		 */

		if (Prop->val != NULL) {
			aafObject* tmp = Prop->val;

			for (; tmp != NULL; tmp = tmp->next)
				if (!tmp->next)
					break;

			Obj->prev = tmp;
			tmp->next = Obj;
		} else {
			Obj->prev = NULL;
			Prop->val = Obj;
		}
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:
	free (refName);
	free (vectorStream);

	return rc;
}

static int
retrieveProperty (AAF_Data* aafd, aafObject* Obj, aafPropertyDef* Def, aafPropertyIndexEntry_t* p, aafByte_t* v, uint8_t bo)
{
	(void)bo; // TODO: ByteOrder support ?

	aafProperty* Prop = newProperty (aafd, Def);

	if (!Prop) {
		return -1;
	}

	Prop->sf = p->_storedForm;

	/*
		TODO Prop->len / Prop->val ---> retrieveStrongReference() retrieveStrongReferenceSet() retrieveStrongReferenceVector()
		only used to retrieve node name ? There could be a better approach.
	 */

	Prop->len = p->_length;

	Prop->val = malloc (p->_length);

	if (!Prop->val) {
		error ("Out of memory");
		free (Prop);
		return -1;
	}

	memcpy (Prop->val, v, p->_length);

	Prop->next      = Obj->Properties;
	Obj->Properties = Prop;

	switch (p->_storedForm) {
		case SF_STRONG_OBJECT_REFERENCE:
			return retrieveStrongReference (aafd, Prop, Obj);

		case SF_STRONG_OBJECT_REFERENCE_SET:
			return retrieveStrongReferenceSet (aafd, Prop, Obj);

		case SF_STRONG_OBJECT_REFERENCE_VECTOR:
			return retrieveStrongReferenceVector (aafd, Prop, Obj);

		default:
			break;
	}

	return 0;
}

static int
retrieveObjectProperties (AAF_Data* aafd, aafObject* Obj)
{
	int rc = 0;

	aafByte_t* propStream = getNodeProperties (aafd, Obj->Node);

	if (!propStream) {
		error ("Could not retrieve object %s properties : %s",
		       aaft_ClassIDToText (aafd, Obj->Class->ID),
		       aaf_get_ObjectPath (Obj));
		goto err;
	}

	aafPropertyIndexHeader_t Header;
	aafPropertyIndexEntry_t  Prop;

	memcpy (&Header, propStream, sizeof (aafPropertyIndexHeader_t));

	aafByte_t*      value = NULL;
	aafPropertyDef* PDef  = NULL;

	size_t valueOffset = 0;

	uint32_t i = 0;

	foreachPropertyEntry (propStream, Header, Prop, value, valueOffset, i)
	{
		PDef = aafclass_getPropertyDefinitionByID (Obj->Class, Prop._pid);

		if (!PDef) {
			warning ("Unknown property 0x%04x (%s) of object %s",
			         Prop._pid,
			         aaft_PIDToText (aafd, Prop._pid),
			         aaft_ClassIDToText (aafd, Obj->Class->ID));
			continue;
		}

		rc = retrieveProperty (aafd, Obj, PDef, &Prop, value, Header._byteOrder);

		if (rc < 0) {
			error ("Could not retrieve property %s of object %s",
			       aaft_PIDToText (aafd, PDef->pid),
			       aaft_ClassIDToText (aafd, Obj->Class->ID));
			goto err;
		}
	}

	rc = 0;
	goto end;

err:
	rc = -1;

end:

	free (propStream);

	return rc;
}

static cfbNode*
getStrongRefIndexNode (AAF_Data* aafd, aafObject* Parent, const char* refName)
{
	char name[CFB_NODE_NAME_SZ];

	int rc = snprintf (name, CFB_NODE_NAME_SZ, "%s index", refName);

	if (rc < 0 || (size_t)rc >= CFB_NODE_NAME_SZ) {
		error ("snprintf() error");
		return NULL;
	}

	cfbNode* Node = cfb_getChildNode (aafd->cfbd, name, Parent->Node);

	if (!Node) {
		error ("Could not retrieve Reference Set/Vector Index Node @ \"%s/%s index\"",
		       aaf_get_ObjectPath (Parent),
		       refName);
		return NULL;
	}

	return Node;
}

static cfbNode*
getStrongRefEntryNode (AAF_Data* aafd, aafObject* Parent, const char* refName, uint32_t index)
{
	char name[CFB_NODE_NAME_SZ];

	int rc = snprintf (name, CFB_NODE_NAME_SZ, "%s{%x}", refName, index);

	if (rc < 0 || (size_t)rc >= CFB_NODE_NAME_SZ) {
		error ("snprintf() error");
		return NULL;
	}

	cfbNode* Node = cfb_getChildNode (aafd->cfbd, name, Parent->Node);

	if (!Node) {
		error ("Could not retrieve Reference Set/vector Entry Node @ \"%s/%s index\"",
		       aaf_get_ObjectPath (Parent),
		       refName);
		return NULL;
	}

	return Node;
}

static aafByte_t*
getNodeProperties (AAF_Data* aafd, cfbNode* Node)
{
	if (!Node) {
		error ("Node is NULL");
		return NULL;
	}

	uint64_t   stream_sz = 0;
	aafByte_t* stream    = NULL;

	cfbNode* propNode = cfb_getChildNode (aafd->cfbd, "properties", Node);

	if (!propNode) {
		error ("Could not retrieve Property Node");
		return NULL;
	}

	cfb_getStream (aafd->cfbd, propNode, &stream, &stream_sz);

	if (!stream) {
		error ("Could not retrieve Property Stream");
		return NULL;
	}

	/*
	 *  Ensures PropHeader + all PropEntries + all PropValues matches the Stream size.
	 *  TODO : is the following test important ?
	 */

	/*
	uint32_t prop_sz = sizeof(aafPropertyIndexHeader_t);

	uint32_t i = 0;

	for ( i = 0; i < ((aafPropertyIndexHeader_t*)stream)->_entryCount; i++ )
		prop_sz += (((aafPropertyIndexEntry_t*)(stream+((sizeof(aafPropertyIndexEntry_t)*i)+sizeof(aafPropertyIndexHeader_t))))->_length) + sizeof(aafPropertyIndexEntry_t);

	if ( prop_sz != stream_sz )
		warning( L"Stream length (%lu Bytes) does not match property length (%u Bytes).",
			stream_sz,
			prop_sz );
*/

	return stream;
}

static aafStrongRefSetHeader_t*
getStrongRefSetList (AAF_Data* aafd, cfbNode* Node, aafObject* Parent)
{
	if (!Node)
		return NULL;

	aafByte_t* stream    = NULL;
	uint64_t   stream_sz = 0;

	cfb_getStream (aafd->cfbd, Node, &stream, &stream_sz);

	if (!stream) {
		char* refName = cfb_w16toUTF8 (Node->_ab, Node->_cb);

		error ("Could not retrieve StrongReferenceSet Index Stream @ \"%s/%s index\"",
		       aaf_get_ObjectPath (Parent),
		       refName);

		free (refName);

		return NULL;
	}

	return (aafStrongRefSetHeader_t*)stream;
}

static aafByte_t*
getStrongRefVectorList (AAF_Data* aafd, cfbNode* Node, aafObject* Parent)
{
	if (!Node)
		return NULL;

	aafByte_t* stream    = NULL;
	uint64_t   stream_sz = 0;

	cfb_getStream (aafd->cfbd, Node, &stream, &stream_sz);

	if (!stream) {
		char* refName = cfb_w16toUTF8 (Node->_ab, Node->_cb);

		error ("Could not retrieve StrongReferenceVector Index Stream \"%s/%s index\"",
		       aaf_get_ObjectPath (Parent),
		       refName);

		return NULL;
	}

	return stream;
}
