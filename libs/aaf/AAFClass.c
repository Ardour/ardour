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

/**
 * @brief AAF core functions.
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 04 october 2017
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/AAFCore.h"
#include "aaf/AAFToText.h"
#include "aaf/AAFTypes.h"

#include "aaf/AAFDefs/AAFClassDefUIDs.h"
#include "aaf/AAFDefs/AAFPropertyIDs.h"
#include "aaf/AAFDefs/AAFTypeDefUIDs.h"

#include "aaf/log.h"

#include "aaf/AAFClass.h"

#define debug(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (aafd->log, aafd, LOG_SRC_ID_AAF_CORE, VERB_ERROR, __VA_ARGS__)

#define attachNewProperty(aafd, Class, Prop, Pid, IsReq) \
	Prop = calloc (1, sizeof (aafPropertyDef));      \
	if (!Prop) {                                     \
		error ("Out of memory");                 \
		return -1;                               \
	}                                                \
	Prop->pid   = Pid;                               \
	Prop->name  = NULL;                              \
	Prop->isReq = IsReq;                             \
	Prop->meta  = 0;                                 \
	Prop->next  = Class->Properties;                 \
	memset (&Prop->type, 0x00, sizeof (aafUID_t));   \
	Class->Properties = Prop;

int
aafclass_classExists (AAF_Data* aafd, aafUID_t* ClassID)
{
	aafClass* Class = NULL;

	foreachClass (Class, aafd->Classes) if (aafUIDCmp (Class->ID, ClassID)) break;

	if (!Class)
		return 0;

	return 1;
}

/**
 * Allocates and initializes a new aafClass structure, adds it
 * to the aafd->class  list and returns a pointer to the newly
 * allocated Class.
 *
 * @param  aafd       pointer to the AAF_Data structure.
 * @param  id         pointer to the ClassID.
 * @param  isConcrete boolean to set the Class as either an ABSTRACT or a CONCRETE Class.
 * @param  parent     pointer to the parent Class.
 * @return            pointer to the newly allocated aafClass.
 */

aafClass*
aafclass_defineNewClass (AAF_Data* aafd, const aafUID_t* id, uint8_t isConcrete, aafClass* parent)
{
	aafClass* Class = malloc (sizeof (aafClass));

	if (!Class) {
		error ("Out of memory");
		return NULL;
	}

	Class->ID         = id;
	Class->Parent     = parent;
	Class->Properties = NULL;
	Class->isConcrete = isConcrete;

	Class->meta = 0;
	Class->name = NULL;

	Class->next   = aafd->Classes;
	aafd->Classes = Class;

	return Class;
}

/**
 * Retrieve an aafClass for a given ClassID.
 *
 * @param  aafd pointer to the AAF_Data structure.
 * @param  id   pointer to the ClassID to search for.
 * @return      pointer to the retrieved aafClass structure, or NULL if not found.
 */

aafClass*
aafclass_getClassByID (AAF_Data* aafd, const aafUID_t* id)
{
	aafClass* Class = NULL;

	for (Class = aafd->Classes; Class != NULL; Class = Class->next)
		if (aafUIDCmp (Class->ID, id))
			break;

	return Class;
}

aafPropertyDef*
aafclass_getPropertyDefinitionByID (aafClass* Classes, aafPID_t pid)
{
	aafClass*       Class = NULL;
	aafPropertyDef* PDef  = NULL;

	foreachClassInheritance (Class, Classes)
	    foreachPropertyDefinition (PDef, Class->Properties) if (PDef->pid == pid) return PDef;

	return NULL;
}

/**
 * Defines each Class with its properties according to
 * the standard. All the Classes are then hold by the
 * AAF_Data.Class list.
 *
 * We define the Classes at runtime, so we can later
 * add any custom class defined in the MetaDictionary.
 * This is not yet implemented though.
 *
 * @param aafd The AAF_Data struct pointer.
 */
int
aafclass_setDefaultClasses (AAF_Data* aafd)
{
	aafPropertyDef* prop = NULL;

	aafClass* IOC = aafclass_defineNewClass (aafd, &AAFClassID_InterchangeObject, ABSTRACT, NULL);

	if (IOC == NULL) {
		return -1;
	}

	attachNewProperty (aafd, IOC, prop, PID_InterchangeObject_ObjClass, PROP_REQ);
	attachNewProperty (aafd, IOC, prop, PID_InterchangeObject_Generation, PROP_OPT);

	aafClass* Root = aafclass_defineNewClass (aafd, &AAFClassID_Root, CONCRETE, IOC);

	if (Root == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Root, prop, PID_Root_MetaDictionary, PROP_REQ);
	attachNewProperty (aafd, Root, prop, PID_Root_Header, PROP_REQ);

	aafClass* Header = aafclass_defineNewClass (aafd, &AAFClassID_Header, CONCRETE, IOC);

	if (Header == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Header, prop, PID_Header_ByteOrder, PROP_REQ);
	attachNewProperty (aafd, Header, prop, PID_Header_LastModified, PROP_REQ);
	attachNewProperty (aafd, Header, prop, PID_Header_Version, PROP_REQ);
	attachNewProperty (aafd, Header, prop, PID_Header_Content, PROP_REQ);
	attachNewProperty (aafd, Header, prop, PID_Header_Dictionary, PROP_REQ);
	attachNewProperty (aafd, Header, prop, PID_Header_IdentificationList, PROP_REQ);
	attachNewProperty (aafd, Header, prop, PID_Header_ObjectModelVersion, PROP_OPT);
	attachNewProperty (aafd, Header, prop, PID_Header_OperationalPattern, PROP_OPT);
	attachNewProperty (aafd, Header, prop, PID_Header_EssenceContainers, PROP_OPT);
	attachNewProperty (aafd, Header, prop, PID_Header_DescriptiveSchemes, PROP_OPT);

	aafClass* Identif = aafclass_defineNewClass (aafd, &AAFClassID_Identification, CONCRETE, IOC);

	if (Identif == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Identif, prop, PID_Identification_CompanyName, PROP_REQ);
	attachNewProperty (aafd, Identif, prop, PID_Identification_ProductName, PROP_REQ);
	attachNewProperty (aafd, Identif, prop, PID_Identification_ProductVersion, PROP_OPT);
	attachNewProperty (aafd, Identif, prop, PID_Identification_ProductVersionString, PROP_REQ);
	attachNewProperty (aafd, Identif, prop, PID_Identification_ProductID, PROP_REQ);
	attachNewProperty (aafd, Identif, prop, PID_Identification_Date, PROP_REQ);
	attachNewProperty (aafd, Identif, prop, PID_Identification_ToolkitVersion, PROP_OPT);
	attachNewProperty (aafd, Identif, prop, PID_Identification_Platform, PROP_OPT);
	attachNewProperty (aafd, Identif, prop, PID_Identification_GenerationAUID, PROP_REQ);

	aafClass* Dictionary = aafclass_defineNewClass (aafd, &AAFClassID_Dictionary, CONCRETE, IOC);

	if (Dictionary == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_OperationDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_ParameterDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_DataDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_PluginDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_CodecDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_ContainerDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_InterpolationDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_KLVDataDefinitions, PROP_OPT);
	attachNewProperty (aafd, Dictionary, prop, PID_Dictionary_TaggedValueDefinitions, PROP_OPT);

	aafClass* Content = aafclass_defineNewClass (aafd, &AAFClassID_ContentStorage, CONCRETE, IOC);

	if (Content == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Content, prop, PID_ContentStorage_Mobs, PROP_REQ);
	attachNewProperty (aafd, Content, prop, PID_ContentStorage_EssenceData, PROP_REQ);

	aafClass* Mob = aafclass_defineNewClass (aafd, &AAFClassID_Mob, ABSTRACT, IOC);

	if (Mob == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Mob, prop, PID_Mob_MobID, PROP_REQ);
	attachNewProperty (aafd, Mob, prop, PID_Mob_Name, PROP_OPT);
	attachNewProperty (aafd, Mob, prop, PID_Mob_Slots, PROP_REQ);
	attachNewProperty (aafd, Mob, prop, PID_Mob_LastModified, PROP_REQ);
	attachNewProperty (aafd, Mob, prop, PID_Mob_CreationTime, PROP_REQ);
	attachNewProperty (aafd, Mob, prop, PID_Mob_UserComments, PROP_OPT);
	attachNewProperty (aafd, Mob, prop, PID_Mob_Attributes, PROP_OPT);
	attachNewProperty (aafd, Mob, prop, PID_Mob_KLVData, PROP_OPT);
	attachNewProperty (aafd, Mob, prop, PID_Mob_UsageCode, PROP_OPT);

	aafClass* CompoMob = aafclass_defineNewClass (aafd, &AAFClassID_CompositionMob, CONCRETE, Mob);

	if (CompoMob == NULL) {
		return -1;
	}

	attachNewProperty (aafd, CompoMob, prop, PID_CompositionMob_DefaultFadeLength, PROP_OPT);
	attachNewProperty (aafd, CompoMob, prop, PID_CompositionMob_DefFadeType, PROP_OPT);
	attachNewProperty (aafd, CompoMob, prop, PID_CompositionMob_DefFadeEditUnit, PROP_OPT);
	attachNewProperty (aafd, CompoMob, prop, PID_CompositionMob_Rendering, PROP_OPT);

	aafClass* MasterMob = aafclass_defineNewClass (aafd, &AAFClassID_MasterMob, CONCRETE, Mob);

	if (MasterMob == NULL) {
		return -1;
	}

	/* The MasterMob class does not define any additional properties. */

	aafClass* SourceMob = aafclass_defineNewClass (aafd, &AAFClassID_SourceMob, CONCRETE, Mob);

	if (SourceMob == NULL) {
		return -1;
	}

	attachNewProperty (aafd, SourceMob, prop, PID_SourceMob_EssenceDescription, PROP_REQ);

	aafClass* MobSlot = aafclass_defineNewClass (aafd, &AAFClassID_MobSlot, ABSTRACT, IOC);

	if (MobSlot == NULL) {
		return -1;
	}

	attachNewProperty (aafd, MobSlot, prop, PID_MobSlot_SlotID, PROP_REQ);
	attachNewProperty (aafd, MobSlot, prop, PID_MobSlot_SlotName, PROP_OPT);
	attachNewProperty (aafd, MobSlot, prop, PID_MobSlot_PhysicalTrackNumber, PROP_OPT);
	attachNewProperty (aafd, MobSlot, prop, PID_MobSlot_Segment, PROP_REQ);

	aafClass* TimelineMobSlot = aafclass_defineNewClass (aafd, &AAFClassID_TimelineMobSlot, CONCRETE, MobSlot);

	if (TimelineMobSlot == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TimelineMobSlot, prop, PID_TimelineMobSlot_EditRate, PROP_REQ);
	attachNewProperty (aafd, TimelineMobSlot, prop, PID_TimelineMobSlot_Origin, PROP_REQ);
	attachNewProperty (aafd, TimelineMobSlot, prop, PID_TimelineMobSlot_MarkIn, PROP_OPT);
	attachNewProperty (aafd, TimelineMobSlot, prop, PID_TimelineMobSlot_MarkOut, PROP_OPT);
	attachNewProperty (aafd, TimelineMobSlot, prop, PID_TimelineMobSlot_UserPos, PROP_OPT);

	aafClass* EventMobSlot = aafclass_defineNewClass (aafd, &AAFClassID_EventMobSlot, CONCRETE, MobSlot);

	if (EventMobSlot == NULL) {
		return -1;
	}

	attachNewProperty (aafd, EventMobSlot, prop, PID_EventMobSlot_EditRate, PROP_REQ);
	//	attachNewProperty( aafd, EventMobSlot,     prop, PID_EventMobSlot_EventSlotOrigin,                  ??? );

	aafClass* StaticMobSlot = aafclass_defineNewClass (aafd, &AAFClassID_StaticMobSlot, CONCRETE, MobSlot);

	if (StaticMobSlot == NULL) {
		return -1;
	}

	/* The StaticMobSlot class does not define any additional properties. */

	aafClass* KLVData = aafclass_defineNewClass (aafd, &AAFClassID_KLVData, CONCRETE, IOC);

	if (KLVData == NULL) {
		return -1;
	}

	attachNewProperty (aafd, KLVData, prop, PID_KLVData_Value, PROP_REQ);

	aafClass* TaggedValue = aafclass_defineNewClass (aafd, &AAFClassID_TaggedValue, CONCRETE, IOC);

	if (TaggedValue == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TaggedValue, prop, PID_TaggedValue_Name, PROP_REQ);
	attachNewProperty (aafd, TaggedValue, prop, PID_TaggedValue_Value, PROP_REQ);

	aafClass* Parameter = aafclass_defineNewClass (aafd, &AAFClassID_Parameter, ABSTRACT, IOC);

	if (Parameter == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Parameter, prop, PID_Parameter_Definition, PROP_REQ);

	aafClass* ConstantValue = aafclass_defineNewClass (aafd, &AAFClassID_ConstantValue, CONCRETE, Parameter);

	if (ConstantValue == NULL) {
		return -1;
	}

	attachNewProperty (aafd, ConstantValue, prop, PID_ConstantValue_Value, PROP_REQ);

	aafClass* VaryingValue = aafclass_defineNewClass (aafd, &AAFClassID_VaryingValue, CONCRETE, Parameter);

	if (VaryingValue == NULL) {
		return -1;
	}

	attachNewProperty (aafd, VaryingValue, prop, PID_VaryingValue_Interpolation, PROP_REQ);
	attachNewProperty (aafd, VaryingValue, prop, PID_VaryingValue_PointList, PROP_REQ);

	aafClass* ControlPoint = aafclass_defineNewClass (aafd, &AAFClassID_ControlPoint, CONCRETE, IOC);

	if (ControlPoint == NULL) {
		return -1;
	}

	attachNewProperty (aafd, ControlPoint, prop, PID_ControlPoint_Value, PROP_REQ);
	attachNewProperty (aafd, ControlPoint, prop, PID_ControlPoint_Time, PROP_REQ);
	attachNewProperty (aafd, ControlPoint, prop, PID_ControlPoint_EditHint, PROP_OPT);

	aafClass* Locator = aafclass_defineNewClass (aafd, &AAFClassID_Locator, ABSTRACT, IOC);

	if (Locator == NULL) {
		return -1;
	}

	/* The Locator class does not define any additional properties. */

	aafClass* NetworkLocator = aafclass_defineNewClass (aafd, &AAFClassID_NetworkLocator, CONCRETE, Locator);

	if (NetworkLocator == NULL) {
		return -1;
	}

	attachNewProperty (aafd, NetworkLocator, prop, PID_NetworkLocator_URLString, PROP_REQ);

	aafClass* TextLocator = aafclass_defineNewClass (aafd, &AAFClassID_TextLocator, CONCRETE, Locator);

	if (TextLocator == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TextLocator, prop, PID_TextLocator_Name, PROP_REQ);

	aafClass* Component = aafclass_defineNewClass (aafd, &AAFClassID_Component, ABSTRACT, IOC);

	if (Component == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Component, prop, PID_Component_DataDefinition, PROP_REQ);
	attachNewProperty (aafd, Component, prop, PID_Component_Length, PROP_OPT);
	attachNewProperty (aafd, Component, prop, PID_Component_KLVData, PROP_OPT);
	attachNewProperty (aafd, Component, prop, PID_Component_UserComments, PROP_OPT);
	attachNewProperty (aafd, Component, prop, PID_Component_Attributes, PROP_OPT);

	aafClass* Transition = aafclass_defineNewClass (aafd, &AAFClassID_Transition, CONCRETE, Component);

	if (Transition == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Transition, prop, PID_Transition_OperationGroup, PROP_REQ);
	attachNewProperty (aafd, Transition, prop, PID_Transition_CutPoint, PROP_REQ);

	aafClass* Segment = aafclass_defineNewClass (aafd, &AAFClassID_Segment, ABSTRACT, Component);

	if (Segment == NULL) {
		return -1;
	}

	/* The Segment class does not define any additional properties. */

	aafClass* Sequence = aafclass_defineNewClass (aafd, &AAFClassID_Sequence, CONCRETE, Segment);

	if (Sequence == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Sequence, prop, PID_Sequence_Components, PROP_REQ);

	aafClass* Filler = aafclass_defineNewClass (aafd, &AAFClassID_Filler, CONCRETE, Segment);

	if (Filler == NULL) {
		return -1;
	}

	/* The Filler class does not define any additional properties. */

	aafClass* SourceRef = aafclass_defineNewClass (aafd, &AAFClassID_SourceReference, ABSTRACT, Segment);

	if (SourceRef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, SourceRef, prop, PID_SourceReference_SourceID, PROP_OPT);
	attachNewProperty (aafd, SourceRef, prop, PID_SourceReference_SourceMobSlotID, PROP_REQ);
	attachNewProperty (aafd, SourceRef, prop, PID_SourceReference_ChannelIDs, PROP_OPT);
	attachNewProperty (aafd, SourceRef, prop, PID_SourceReference_MonoSourceSlotIDs, PROP_OPT);

	aafClass* SourceClip = aafclass_defineNewClass (aafd, &AAFClassID_SourceClip, CONCRETE, SourceRef);

	if (SourceClip == NULL) {
		return -1;
	}

	attachNewProperty (aafd, SourceClip, prop, PID_SourceClip_StartTime, PROP_OPT);
	attachNewProperty (aafd, SourceClip, prop, PID_SourceClip_FadeInLength, PROP_OPT);
	attachNewProperty (aafd, SourceClip, prop, PID_SourceClip_FadeInType, PROP_OPT);
	attachNewProperty (aafd, SourceClip, prop, PID_SourceClip_FadeOutLength, PROP_OPT);
	attachNewProperty (aafd, SourceClip, prop, PID_SourceClip_FadeOutType, PROP_OPT);

	aafClass* Event = aafclass_defineNewClass (aafd, &AAFClassID_Event, ABSTRACT, Segment);

	if (Event == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Event, prop, PID_Event_Position, PROP_REQ);
	attachNewProperty (aafd, Event, prop, PID_Event_Comment, PROP_OPT);

	aafClass* CommentMarker = aafclass_defineNewClass (aafd, &AAFClassID_CommentMarker, CONCRETE, Event);

	if (CommentMarker == NULL) {
		return -1;
	}

	attachNewProperty (aafd, CommentMarker, prop, PID_CommentMarker_Annotation, PROP_OPT);

	aafClass* DescriptiveMarker = aafclass_defineNewClass (aafd, &AAFClassID_DescriptiveMarker, CONCRETE, CommentMarker);

	if (DescriptiveMarker == NULL) {
		return -1;
	}

	attachNewProperty (aafd, DescriptiveMarker, prop, PID_DescriptiveMarker_DescribedSlots, PROP_OPT);
	attachNewProperty (aafd, DescriptiveMarker, prop, PID_DescriptiveMarker_Description, PROP_OPT);

	aafClass* GPITrigger = aafclass_defineNewClass (aafd, &AAFClassID_GPITrigger, CONCRETE, Event);

	if (GPITrigger == NULL) {
		return -1;
	}

	attachNewProperty (aafd, GPITrigger, prop, PID_GPITrigger_ActiveState, PROP_REQ);

	aafClass* Timecode = aafclass_defineNewClass (aafd, &AAFClassID_Timecode, CONCRETE, Segment);

	if (Timecode == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Timecode, prop, PID_Timecode_Start, PROP_REQ);
	attachNewProperty (aafd, Timecode, prop, PID_Timecode_FPS, PROP_REQ);
	attachNewProperty (aafd, Timecode, prop, PID_Timecode_Drop, PROP_REQ);

	aafClass* TCStream = aafclass_defineNewClass (aafd, &AAFClassID_TimecodeStream, ABSTRACT, Segment);

	if (TCStream == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TCStream, prop, PID_TimecodeStream_SampleRate, PROP_REQ);
	attachNewProperty (aafd, TCStream, prop, PID_TimecodeStream_Source, PROP_REQ);
	attachNewProperty (aafd, TCStream, prop, PID_TimecodeStream_SourceType, PROP_REQ);

	aafClass* TCStream12M = aafclass_defineNewClass (aafd, &AAFClassID_TimecodeStream12M, CONCRETE, TCStream);

	if (TCStream12M == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TCStream12M, prop, PID_TimecodeStream12M_IncludeSync, PROP_OPT);

	aafClass* Edgecode = aafclass_defineNewClass (aafd, &AAFClassID_Edgecode, CONCRETE, Segment);

	if (Edgecode == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Edgecode, prop, PID_EdgeCode_Start, PROP_REQ);
	attachNewProperty (aafd, Edgecode, prop, PID_EdgeCode_FilmKind, PROP_REQ);
	attachNewProperty (aafd, Edgecode, prop, PID_EdgeCode_CodeFormat, PROP_REQ);
	attachNewProperty (aafd, Edgecode, prop, PID_EdgeCode_Header, PROP_OPT);

	aafClass* Pulldown = aafclass_defineNewClass (aafd, &AAFClassID_Pulldown, CONCRETE, Segment);

	if (Pulldown == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Pulldown, prop, PID_Pulldown_InputSegment, PROP_REQ);
	attachNewProperty (aafd, Pulldown, prop, PID_Pulldown_PulldownKind, PROP_REQ);
	attachNewProperty (aafd, Pulldown, prop, PID_Pulldown_PulldownDirection, PROP_REQ);
	attachNewProperty (aafd, Pulldown, prop, PID_Pulldown_PhaseFrame, PROP_REQ);

	aafClass* OperationGroup = aafclass_defineNewClass (aafd, &AAFClassID_OperationGroup, CONCRETE, Segment);

	if (OperationGroup == NULL) {
		return -1;
	}

	attachNewProperty (aafd, OperationGroup, prop, PID_OperationGroup_Operation, PROP_REQ);
	attachNewProperty (aafd, OperationGroup, prop, PID_OperationGroup_InputSegments, PROP_OPT);
	attachNewProperty (aafd, OperationGroup, prop, PID_OperationGroup_Parameters, PROP_OPT);
	attachNewProperty (aafd, OperationGroup, prop, PID_OperationGroup_Rendering, PROP_OPT);
	attachNewProperty (aafd, OperationGroup, prop, PID_OperationGroup_BypassOverride, PROP_OPT);

	aafClass* NestedScope = aafclass_defineNewClass (aafd, &AAFClassID_NestedScope, CONCRETE, Segment);

	if (NestedScope == NULL) {
		return -1;
	}

	attachNewProperty (aafd, NestedScope, prop, PID_NestedScope_Slots, PROP_REQ);

	aafClass* ScopeReference = aafclass_defineNewClass (aafd, &AAFClassID_ScopeReference, CONCRETE, Segment);

	if (ScopeReference == NULL) {
		return -1;
	}

	attachNewProperty (aafd, ScopeReference, prop, PID_ScopeReference_RelativeScope, PROP_REQ);
	attachNewProperty (aafd, ScopeReference, prop, PID_ScopeReference_RelativeSlot, PROP_REQ);

	aafClass* Selector = aafclass_defineNewClass (aafd, &AAFClassID_Selector, CONCRETE, Segment);

	if (Selector == NULL) {
		return -1;
	}

	attachNewProperty (aafd, Selector, prop, PID_Selector_Selected, PROP_REQ);
	attachNewProperty (aafd, Selector, prop, PID_Selector_Alternates, PROP_OPT);

	aafClass* EssenceGroup = aafclass_defineNewClass (aafd, &AAFClassID_EssenceGroup, CONCRETE, Segment);

	if (EssenceGroup == NULL) {
		return -1;
	}

	attachNewProperty (aafd, EssenceGroup, prop, PID_EssenceGroup_Choices, PROP_REQ);
	attachNewProperty (aafd, EssenceGroup, prop, PID_EssenceGroup_StillFrame, PROP_OPT);

	aafClass* DescriptiveFramework = aafclass_defineNewClass (aafd, &AAFClassID_DescriptiveFramework, ABSTRACT, IOC);

	if (DescriptiveFramework == NULL) {
		return -1;
	}

	aafClass* EssenceDesc = aafclass_defineNewClass (aafd, &AAFClassID_EssenceDescriptor, ABSTRACT, IOC);

	if (EssenceDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, EssenceDesc, prop, PID_EssenceDescriptor_Locator, PROP_OPT);

	aafClass* FileDesc = aafclass_defineNewClass (aafd, &AAFClassID_FileDescriptor, ABSTRACT, EssenceDesc);

	if (FileDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, FileDesc, prop, PID_FileDescriptor_SampleRate, PROP_REQ);
	attachNewProperty (aafd, FileDesc, prop, PID_FileDescriptor_Length, PROP_REQ);
	attachNewProperty (aafd, FileDesc, prop, PID_FileDescriptor_ContainerFormat, PROP_OPT);
	attachNewProperty (aafd, FileDesc, prop, PID_FileDescriptor_CodecDefinition, PROP_OPT);
	//	attachNewProperty( aafd, FileDesc,         prop, PID_FileDescriptor_LinkedSlotID,                           ??? );

	aafClass* DigitalImageDesc = aafclass_defineNewClass (aafd, &AAFClassID_DigitalImageDescriptor, ABSTRACT, FileDesc);

	if (DigitalImageDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_Compression, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_StoredHeight, PROP_REQ);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_StoredWidth, PROP_REQ);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_StoredF2Offset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_SampledHeight, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_SampledWidth, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_SampledXOffset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_SampledYOffset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_DisplayHeight, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_DisplayWidth, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_DisplayXOffset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_DisplayYOffset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_DisplayF2Offset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_FrameLayout, PROP_REQ);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_VideoLineMap, PROP_REQ);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_ImageAspectRatio, PROP_REQ);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_ActiveFormatDescriptor, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_AlphaTransparency, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_ImageAlignmentFactor, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_FieldDominance, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_FieldStartOffset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_FieldEndOffset, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_ColorPrimaries, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_CodingEquations, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_TransferCharacteristic, PROP_OPT);
	attachNewProperty (aafd, DigitalImageDesc, prop, PID_DigitalImageDescriptor_SignalStandard, PROP_OPT);

	aafClass* CDCIDesc = aafclass_defineNewClass (aafd, &AAFClassID_CDCIDescriptor, CONCRETE, DigitalImageDesc);

	if (CDCIDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_HorizontalSubsampling, PROP_REQ);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_VerticalSubsampling, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_ComponentWidth, PROP_REQ);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_AlphaSamplingWidth, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_PaddingBits, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_ColorSiting, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_BlackReferenceLevel, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_WhiteReferenceLevel, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_ColorRange, PROP_OPT);
	attachNewProperty (aafd, CDCIDesc, prop, PID_CDCIDescriptor_ReversedByteOrder, PROP_OPT);

	aafClass* RGBADesc = aafclass_defineNewClass (aafd, &AAFClassID_RGBADescriptor, CONCRETE, DigitalImageDesc);

	if (RGBADesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_PixelLayout, PROP_REQ);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_Palette, PROP_OPT);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_PaletteLayout, PROP_OPT);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_ComponentMinRef, PROP_OPT);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_ComponentMaxRef, PROP_OPT);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_AlphaMinRef, PROP_OPT);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_AlphaMaxRef, PROP_OPT);
	attachNewProperty (aafd, RGBADesc, prop, PID_RGBADescriptor_ScanningDirection, PROP_OPT);

	aafClass* TapeDesc = aafclass_defineNewClass (aafd, &AAFClassID_TapeDescriptor, CONCRETE, EssenceDesc);

	if (TapeDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_FormFactor, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_VideoSignal, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_TapeFormat, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_Length, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_ManufacturerID, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_Model, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_TapeBatchNumber, PROP_OPT);
	attachNewProperty (aafd, TapeDesc, prop, PID_TapeDescriptor_TapeStock, PROP_OPT);

	aafClass* FilmDesc = aafclass_defineNewClass (aafd, &AAFClassID_FilmDescriptor, CONCRETE, EssenceDesc);

	if (FilmDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_FilmFormat, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_FrameRate, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_PerforationsPerFrame, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_FilmAspectRatio, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_Manufacturer, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_Model, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_FilmGaugeFormat, PROP_OPT);
	attachNewProperty (aafd, FilmDesc, prop, PID_FilmDescriptor_FilmBatchNumber, PROP_OPT);

	aafClass* WAVEDesc = aafclass_defineNewClass (aafd, &AAFClassID_WAVEDescriptor, CONCRETE, FileDesc);

	if (WAVEDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, WAVEDesc, prop, PID_WAVEDescriptor_Summary, PROP_REQ);

	aafClass* AIFCDesc = aafclass_defineNewClass (aafd, &AAFClassID_AIFCDescriptor, CONCRETE, FileDesc);

	if (AIFCDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, AIFCDesc, prop, PID_AIFCDescriptor_Summary, PROP_REQ);

	aafClass* TIFFDesc = aafclass_defineNewClass (aafd, &AAFClassID_TIFFDescriptor, CONCRETE, FileDesc);

	if (TIFFDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TIFFDesc, prop, PID_TIFFDescriptor_IsUniform, PROP_REQ);
	attachNewProperty (aafd, TIFFDesc, prop, PID_TIFFDescriptor_IsContiguous, PROP_REQ);
	attachNewProperty (aafd, TIFFDesc, prop, PID_TIFFDescriptor_LeadingLines, PROP_OPT);
	attachNewProperty (aafd, TIFFDesc, prop, PID_TIFFDescriptor_TrailingLines, PROP_OPT);
	attachNewProperty (aafd, TIFFDesc, prop, PID_TIFFDescriptor_JPEGTableID, PROP_OPT);
	attachNewProperty (aafd, TIFFDesc, prop, PID_TIFFDescriptor_Summary, PROP_REQ);

	aafClass* SoundDesc = aafclass_defineNewClass (aafd, &AAFClassID_SoundDescriptor, CONCRETE, FileDesc);

	if (SoundDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_AudioSamplingRate, PROP_REQ);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_Locked, PROP_OPT);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_AudioRefLevel, PROP_OPT);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_ElectroSpatial, PROP_OPT);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_Channels, PROP_REQ);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_QuantizationBits, PROP_REQ);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_DialNorm, PROP_OPT);
	attachNewProperty (aafd, SoundDesc, prop, PID_SoundDescriptor_Compression, PROP_OPT);

	aafClass* PCMDesc = aafclass_defineNewClass (aafd, &AAFClassID_PCMDescriptor, CONCRETE, SoundDesc);

	if (PCMDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_BlockAlign, PROP_REQ);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_SequenceOffset, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_AverageBPS, PROP_REQ);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_ChannelAssignment, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakEnvelopeVersion, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakEnvelopeFormat, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PointsPerPeakValue, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakEnvelopeBlockSize, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakChannels, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakFrames, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakOfPeaksPosition, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakEnvelopeTimestamp, PROP_OPT);
	attachNewProperty (aafd, PCMDesc, prop, PID_PCMDescriptor_PeakEnvelopeData, PROP_OPT);

	aafClass* PhysicalDesc = aafclass_defineNewClass (aafd, &AAFClassID_PhysicalDescriptor, ABSTRACT, EssenceDesc);

	if (PhysicalDesc == NULL) {
		return -1;
	}

	/* The PhysicalDescriptor class does not define any additional properties. */

	aafClass* ImportDesc = aafclass_defineNewClass (aafd, &AAFClassID_ImportDescriptor, CONCRETE, PhysicalDesc);

	if (ImportDesc == NULL) {
		return -1;
	}

	/* The ImportDescriptor class does not define any additional properties. */

	aafClass* RecordingDesc = aafclass_defineNewClass (aafd, &AAFClassID_RecordingDescriptor, CONCRETE, PhysicalDesc);

	if (RecordingDesc == NULL) {
		return -1;
	}

	/* The RecordingDescriptor class does not define any additional properties. */

	aafClass* AuxiliaryDesc = aafclass_defineNewClass (aafd, &AAFClassID_AuxiliaryDescriptor, CONCRETE, PhysicalDesc);

	if (AuxiliaryDesc == NULL) {
		return -1;
	}

	attachNewProperty (aafd, AuxiliaryDesc, prop, PID_AuxiliaryDescriptor_MimeType, PROP_REQ);
	attachNewProperty (aafd, AuxiliaryDesc, prop, PID_AuxiliaryDescriptor_CharSet, PROP_OPT);

	aafClass* DefObject = aafclass_defineNewClass (aafd, &AAFClassID_DefinitionObject, ABSTRACT, IOC);

	if (DefObject == NULL) {
		return -1;
	}

	attachNewProperty (aafd, DefObject, prop, PID_DefinitionObject_Identification, PROP_REQ);
	attachNewProperty (aafd, DefObject, prop, PID_DefinitionObject_Name, PROP_REQ);
	attachNewProperty (aafd, DefObject, prop, PID_DefinitionObject_Description, PROP_OPT);

	aafClass* DataDef = aafclass_defineNewClass (aafd, &AAFClassID_DataDefinition, CONCRETE, DefObject);

	if (DataDef == NULL) {
		return -1;
	}

	/* The DataDefinition class does not define any additional properties. */

	aafClass* ContainerDef = aafclass_defineNewClass (aafd, &AAFClassID_ContainerDefinition, CONCRETE, DefObject);

	if (ContainerDef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, ContainerDef, prop, PID_ContainerDefinition_EssenceIsIdentified, PROP_OPT);

	aafClass* OperationDef = aafclass_defineNewClass (aafd, &AAFClassID_OperationDefinition, CONCRETE, DefObject);

	if (OperationDef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_DataDefinition, PROP_REQ);
	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_IsTimeWarp, PROP_OPT);
	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_DegradeTo, PROP_OPT);
	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_OperationCategory, PROP_OPT);
	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_NumberInputs, PROP_REQ);
	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_Bypass, PROP_OPT);
	attachNewProperty (aafd, OperationDef, prop, PID_OperationDefinition_ParametersDefined, PROP_OPT);

	aafClass* ParameterDef = aafclass_defineNewClass (aafd, &AAFClassID_ParameterDefinition, CONCRETE, DefObject);

	if (ParameterDef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, ParameterDef, prop, PID_ParameterDefinition_Type, PROP_REQ);
	attachNewProperty (aafd, ParameterDef, prop, PID_ParameterDefinition_DisplayUnits, PROP_OPT);

	aafClass* InterpolationDef = aafclass_defineNewClass (aafd, &AAFClassID_InterpolationDefinition, CONCRETE, DefObject);

	if (InterpolationDef == NULL) {
		return -1;
	}

	/* The InterpolationDefinition class does not define any additional properties. */

	aafClass* CodecDef = aafclass_defineNewClass (aafd, &AAFClassID_CodecDefinition, CONCRETE, DefObject);

	if (CodecDef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, CodecDef, prop, PID_CodecDefinition_FileDescriptorClass, PROP_REQ);
	attachNewProperty (aafd, CodecDef, prop, PID_CodecDefinition_DataDefinitions, PROP_REQ);

	aafClass* PluginDef = aafclass_defineNewClass (aafd, &AAFClassID_PluginDefinition, CONCRETE, DefObject);

	if (PluginDef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_PluginCategory, PROP_REQ);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_VersionNumber, PROP_REQ);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_VersionString, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_Manufacturer, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_ManufacturerInfo, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_ManufacturerID, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_Platform, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_MinPlatformVersion, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_MaxPlatformVersion, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_Engine, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_MinEngineVersion, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_MaxEngineVersion, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_PluginAPI, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_MinPluginAPI, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_MaxPluginAPI, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_SoftwareOnly, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_Accelerator, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_Locators, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_Authentication, PROP_OPT);
	attachNewProperty (aafd, PluginDef, prop, PID_PluginDefinition_DefinitionObject, PROP_OPT);

	aafClass* TaggedValueDef = aafclass_defineNewClass (aafd, &AAFClassID_TaggedValueDefinition, CONCRETE, DefObject);

	if (TaggedValueDef == NULL) {
		return -1;
	}

	/* The TaggedValueDefinition class does not define any additional properties. */

	aafClass* KLVDataDef = aafclass_defineNewClass (aafd, &AAFClassID_KLVDataDefinition, CONCRETE, DefObject);

	if (KLVDataDef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, KLVDataDef, prop, PID_KLVDataDefinition_KLVDataType, PROP_OPT);

	aafClass* EssenceData = aafclass_defineNewClass (aafd, &AAFClassID_EssenceData, CONCRETE, IOC);

	if (EssenceData == NULL) {
		return -1;
	}

	attachNewProperty (aafd, EssenceData, prop, PID_EssenceData_MobID, PROP_REQ);
	attachNewProperty (aafd, EssenceData, prop, PID_EssenceData_Data, PROP_REQ);
	attachNewProperty (aafd, EssenceData, prop, PID_EssenceData_SampleIndex, PROP_OPT);

	aafClass* MetaDefinition = aafclass_defineNewClass (aafd, &AAFClassID_MetaDefinition, ABSTRACT, NULL);

	if (MetaDefinition == NULL) {
		return -1;
	}

	attachNewProperty (aafd, MetaDefinition, prop, PID_MetaDefinition_Identification, PROP_REQ);
	attachNewProperty (aafd, MetaDefinition, prop, PID_MetaDefinition_Name, PROP_REQ);
	attachNewProperty (aafd, MetaDefinition, prop, PID_MetaDefinition_Description, PROP_OPT);

	aafClass* ClassDefinition = aafclass_defineNewClass (aafd, &AAFClassID_ClassDefinition, CONCRETE, MetaDefinition);

	if (ClassDefinition == NULL) {
		return -1;
	}

	attachNewProperty (aafd, ClassDefinition, prop, PID_ClassDefinition_ParentClass, PROP_REQ);
	attachNewProperty (aafd, ClassDefinition, prop, PID_ClassDefinition_Properties, PROP_OPT);
	attachNewProperty (aafd, ClassDefinition, prop, PID_ClassDefinition_IsConcrete, PROP_REQ);

	aafClass* PropertyDefinition = aafclass_defineNewClass (aafd, &AAFClassID_PropertyDefinition, CONCRETE, MetaDefinition);

	if (PropertyDefinition == NULL) {
		return -1;
	}

	attachNewProperty (aafd, PropertyDefinition, prop, PID_PropertyDefinition_Type, PROP_REQ);
	attachNewProperty (aafd, PropertyDefinition, prop, PID_PropertyDefinition_IsOptional, PROP_REQ);
	attachNewProperty (aafd, PropertyDefinition, prop, PID_PropertyDefinition_LocalIdentification, PROP_REQ);
	attachNewProperty (aafd, PropertyDefinition, prop, PID_PropertyDefinition_IsUniqueIdentifier, PROP_OPT);

	aafClass* TypeDef = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinition, ABSTRACT, MetaDefinition);

	if (TypeDef == NULL) {
		return -1;
	}

	/* The TypeDefinition class does not define any additional properties. */

	aafClass* TypeDefCharacter = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionCharacter, CONCRETE, TypeDef);

	if (TypeDefCharacter == NULL) {
		return -1;
	}

	/* The TypeDefinitionCharacter class does not define any additional properties. */

	aafClass* TypeDefEnum = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionEnumeration, CONCRETE, TypeDef);

	if (TypeDefEnum == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefEnum, prop, PID_TypeDefinitionEnumeration_ElementType, PROP_REQ);
	attachNewProperty (aafd, TypeDefEnum, prop, PID_TypeDefinitionEnumeration_ElementNames, PROP_REQ);
	attachNewProperty (aafd, TypeDefEnum, prop, PID_TypeDefinitionEnumeration_ElementValues, PROP_REQ);

	aafClass* TypeDefExtEnum = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionExtendibleEnumeration, CONCRETE, TypeDef);

	if (TypeDefExtEnum == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefExtEnum, prop, PID_TypeDefinitionExtendibleEnumeration_ElementNames, PROP_REQ);
	attachNewProperty (aafd, TypeDefExtEnum, prop, PID_TypeDefinitionExtendibleEnumeration_ElementValues, PROP_REQ);

	aafClass* TypeDefFixedArray = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionFixedArray, CONCRETE, TypeDef);

	if (TypeDefFixedArray == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefFixedArray, prop, PID_TypeDefinitionFixedArray_ElementType, PROP_REQ);
	attachNewProperty (aafd, TypeDefFixedArray, prop, PID_TypeDefinitionFixedArray_ElementCount, PROP_REQ);

	aafClass* TypeDefIndirect = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionIndirect, CONCRETE, TypeDef);

	if (TypeDefIndirect == NULL) {
		return -1;
	}

	/* The TypeDefinitionIndirect class does not define any additional properties. */

	aafClass* TypeDefInt = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionInteger, CONCRETE, TypeDef);

	if (TypeDefInt == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefInt, prop, PID_TypeDefinitionInteger_Size, PROP_REQ);
	attachNewProperty (aafd, TypeDefInt, prop, PID_TypeDefinitionInteger_IsSigned, PROP_REQ);

	aafClass* TypeDefOpaque = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionOpaque, CONCRETE, TypeDef);

	if (TypeDefOpaque == NULL) {
		return -1;
	}

	/* The TypeDefinitionOpaque class does not define any additional properties. */

	aafClass* TypeDefRecord = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionRecord, CONCRETE, TypeDef);

	if (TypeDefRecord == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefRecord, prop, PID_TypeDefinitionRecord_MemberTypes, PROP_REQ);
	attachNewProperty (aafd, TypeDefRecord, prop, PID_TypeDefinitionRecord_MemberNames, PROP_REQ);

	aafClass* TypeDefRename = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionRename, CONCRETE, TypeDef);

	if (TypeDefRename == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefRename, prop, PID_TypeDefinitionRename_RenamedType, PROP_REQ);

	aafClass* TypeDefSet = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionSet, CONCRETE, TypeDef);

	if (TypeDefSet == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefSet, prop, PID_TypeDefinitionSet_ElementType, PROP_REQ);

	aafClass* TypeDefStream = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionStream, CONCRETE, TypeDef);

	if (TypeDefStream == NULL) {
		return -1;
	}

	/* The TypeDefinitionStream class does not define any additional properties. */

	aafClass* TypeDefString = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionString, CONCRETE, TypeDef);

	if (TypeDefString == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefString, prop, PID_TypeDefinitionString_ElementType, PROP_REQ);

	aafClass* TypeDefStrongObjRef = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionStrongObjectReference, CONCRETE, TypeDef);

	if (TypeDefStrongObjRef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefStrongObjRef, prop, PID_TypeDefinitionStrongObjectReference_ReferencedType, PROP_REQ);

	aafClass* TypeDefVariableArray = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionVariableArray, CONCRETE, TypeDef);

	if (TypeDefVariableArray == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefVariableArray, prop, PID_TypeDefinitionVariableArray_ElementType, PROP_REQ);

	aafClass* TypeDefWeakObjRef = aafclass_defineNewClass (aafd, &AAFClassID_TypeDefinitionWeakObjectReference, CONCRETE, TypeDef);

	if (TypeDefWeakObjRef == NULL) {
		return -1;
	}

	attachNewProperty (aafd, TypeDefWeakObjRef, prop, PID_TypeDefinitionWeakObjectReference_ReferencedType, PROP_REQ);
	attachNewProperty (aafd, TypeDefWeakObjRef, prop, PID_TypeDefinitionWeakObjectReference_TargetSet, PROP_REQ);

	aafClass* MetaDictionary = aafclass_defineNewClass (aafd, &AAFClassID_MetaDictionary, CONCRETE, NULL);

	if (MetaDictionary == NULL) {
		return -1;
	}

	attachNewProperty (aafd, MetaDictionary, prop, PID_MetaDictionary_ClassDefinitions, PROP_OPT);
	attachNewProperty (aafd, MetaDictionary, prop, PID_MetaDictionary_TypeDefinitions, PROP_OPT);

	return 0;
}
