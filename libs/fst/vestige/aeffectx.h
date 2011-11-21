/*
 * aeffectx.h - simple header to allow VeSTige compilation and eventually work
 *
 * Copyright (c) 2006 Javier Serrano Polo <jasp00/at/users.sourceforge.net>
 * 
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#ifndef _AEFFECTX_H
#define _AEFFECTX_H

#include <stdint.h>

#define audioMasterAutomate 0
#define audioMasterVersion 1
#define audioMasterCurrentId 2
#define audioMasterIdle 3
#define audioMasterPinConnected 4
// unsupported? 5
#define audioMasterWantMidi 6
#define audioMasterGetTime 7
#define audioMasterProcessEvents 8
#define audioMasterSetTime 9
#define audioMasterTempoAt 10
#define audioMasterGetNumAutomatableParameters 11
#define audioMasterGetParameterQuantization 12
#define audioMasterIOChanged 13
#define audioMasterNeedIdle 14
#define audioMasterSizeWindow 15
#define audioMasterGetSampleRate 16
#define audioMasterGetBlockSize 17
#define audioMasterGetInputLatency 18
#define audioMasterGetOutputLatency 19
#define audioMasterGetPreviousPlug 20
#define audioMasterGetNextPlug 21
#define audioMasterWillReplaceOrAccumulate 22
#define audioMasterGetCurrentProcessLevel 23
#define audioMasterGetAutomationState 24
#define audioMasterOfflineStart 25
#define audioMasterOfflineRead 26
#define audioMasterOfflineWrite 27
#define audioMasterOfflineGetCurrentPass 28
#define audioMasterOfflineGetCurrentMetaPass 29
#define audioMasterSetOutputSampleRate 30
// unsupported? 31
#define audioMasterGetSpeakerArrangement 31 // deprecated in 2.4?
#define audioMasterGetVendorString 32
#define audioMasterGetProductString 33
#define audioMasterGetVendorVersion 34
#define audioMasterVendorSpecific 35
#define audioMasterSetIcon 36
#define audioMasterCanDo 37
#define audioMasterGetLanguage 38
#define audioMasterOpenWindow 39
#define audioMasterCloseWindow 40
#define audioMasterGetDirectory 41
#define audioMasterUpdateDisplay 42
#define audioMasterBeginEdit 43
#define audioMasterEndEdit 44
#define audioMasterOpenFileSelector 45
#define audioMasterCloseFileSelector 46// currently unused
#define audioMasterEditFile 47// currently unused
#define audioMasterGetChunkFile 48// currently unused
#define audioMasterGetInputSpeakerArrangement 49 // currently unused

#define effFlagsHasEditor 1
// very likely
#define effFlagsCanReplacing (1 << 4)
// currently unused
#define effFlagsIsSynth (1 << 8)

#define effOpen 0
//currently unused
#define effClose 1
// currently unused
#define effSetProgram 2
// currently unused
#define effGetProgram 3
// currently unused
#define effGetProgramName 5
#define effGetParamLabel 6
// currently unused
#define effGetParamName 8
// this is a guess
#define effSetSampleRate 10
#define effSetBlockSize 11
#define effMainsChanged 12
#define effEditGetRect 13
#define effEditOpen 14
#define effEditClose 15
#define effEditIdle 19
#define effProcessEvents 25
#define effGetEffectName 45
// missing
#define effGetParameterProperties 47
#define effGetVendorString 47
#define effGetProductString 48
#define effGetVendorVersion 49
// currently unused
#define effCanDo 51
// currently unused
#define effGetVstVersion 58

#ifdef WORDS_BIGENDIAN
// "VstP"
#define kEffectMagic 0x50747356
#else
// "PtsV"
#define kEffectMagic 0x56737450
#endif

#define kVstLangEnglish 1
#define kVstMidiType 1
#define kVstTransportPlaying (1 << 1)

/* validity flags for a VstTimeINfo structure this info comes from the web */

#define kVstNanosValid (1 << 8)
#define kVstPpqPosValid (1 << 9)
#define kVstTempoValid (1 << 10)
#define kVstBarsValid (1 << 11)
#define kVstCyclePosValid (1 << 12)
#define kVstTimeSigValid (1 << 13)
#define kVstSmpteValid (1 << 14)
#define kVstClockValid (1 << 15)

#define kVstTransportChanged 1

typedef struct VstMidiEvent
{
	// 00
	int type;
	// 04
	int byteSize;
	// 08
	int deltaFrames;
	// 0c?
	int flags;
	// 10?
	int noteLength;
	// 14?
	int noteOffset;
	// 18
	char midiData[4];
	// 1c?
	char detune;
	// 1d?
	char noteOffVelocity;
	// 1e?
	char reserved1;
	// 1f?
	char reserved2;

} VstMidiEvent;




typedef struct VstEvent
{
	char dump[sizeof( VstMidiEvent )];

} VstEvent ;




typedef struct VstEvents
{
	// 00
	int numEvents;
	// 04
	int reserved;
	// 08
	VstEvent * events[];
} VstEvents;

/* constants from http://www.rawmaterialsoftware.com/juceforum/viewtopic.php?t=3740&sid=183f74631fee71a493316735e2b9f28b */

enum Vestige2StringConstants
{
	VestigeMaxNameLen       = 64,
	VestigeMaxLabelLen      = 64,
	VestigeMaxShortLabelLen = 8,
	VestigeMaxCategLabelLen = 24,
	VestigeMaxFileNameLen   = 100
}; 

/* this struct taken from http://asseca.com/vst-24-specs/efGetParameterProperties.html */
struct VstParameterProperties
{
    float stepFloat;              /* float step */
    float smallStepFloat;         /* small float step */
    float largeStepFloat;         /* large float step */
    char label[VestigeMaxLabelLen];  /* parameter label */
    int32_t flags;               /* @see VstParameterFlags */
    int32_t minInteger;          /* integer minimum */
    int32_t maxInteger;          /* integer maximum */
    int32_t stepInteger;         /* integer step */
    int32_t largeStepInteger;    /* large integer step */
    char shortLabel[VestigeMaxShortLabelLen]; /* short label, recommended: 6 + delimiter */
    int16_t displayIndex;        /* index where this parameter should be displayed (starting with 0) */
    int16_t category;            /* 0: no category, else group index + 1 */
    int16_t numParametersInCategory; /* number of parameters in category */
    int16_t reserved;            /* zero */
    char categoryLabel[VestigeMaxCategLabelLen]; /* category label, e.g. "Osc 1"  */
    char future[16];              /* reserved for future use */
};

/* this enum taken from http://asseca.com/vst-24-specs/efGetParameterProperties.html */
enum VstParameterFlags
{
	kVstParameterIsSwitch                = 1 << 0,  /* parameter is a switch (on/off) */
	kVstParameterUsesIntegerMinMax       = 1 << 1,  /* minInteger, maxInteger valid */
	kVstParameterUsesFloatStep           = 1 << 2,  /* stepFloat, smallStepFloat, largeStepFloat valid */
	kVstParameterUsesIntStep             = 1 << 3,  /* stepInteger, largeStepInteger valid */
	kVstParameterSupportsDisplayIndex    = 1 << 4,  /* displayIndex valid */
	kVstParameterSupportsDisplayCategory = 1 << 5,  /* category, etc. valid */
	kVstParameterCanRamp                 = 1 << 6   /* set if parameter value can ramp up/down */
};

typedef struct AEffect
{
	// Never use c++!!!
	// 00-03
	int magic;
	// dispatcher 04-07
	int (* dispatcher)( struct AEffect * , int , int , int , void * , float );
	// process, quite sure 08-0b
	void (* process)( struct AEffect * , float * * , float * * , int );
	// setParameter 0c-0f
	void (* setParameter)( struct AEffect * , int , float );
	// getParameter 10-13
	float (* getParameter)( struct AEffect * , int );
	// programs 14-17
	int numPrograms;
	// Params 18-1b
	int numParams;
	// Input 1c-1f
	int numInputs;
	// Output 20-23
	int numOutputs;
	// flags 24-27
	int flags;
	// Fill somewhere 28-2b
        void * user;
	// Zeroes 2c-2f 30-33 34-37 38-3b
	char empty3[4 + 4 + 4 + 4];
	// 1.0f 3c-3f
	float unkown_float;
	// An object? pointer 40-43
	char empty4[4];
	// Zeroes 44-47
	char empty5[4];
	// Id 48-4b
	char unused_id[4];
	// Don't know 4c-4f
	char unknown1[4];
	// processReplacing 50-53
	void (* processReplacing)( struct AEffect * , float * * , float * * , int );

 int uniqueID;

} AEffect;




typedef struct VstTimeInfo
{
    /* info from online documentation of VST provided by Steinberg */

    double samplePos;
    double sampleRate;
    double nanoSeconds;
    double ppqPos;
    double tempo;
    double barStartPos;
    double cycleStartPos;
    double cycleEndPos;
    double timeSigNumerator;
    double timeSigDenominator;
    long   smpteOffset;
    long   smpteFrameRate;
    long   samplesToNextClock;
    long   flags;

} VstTimeInfo;

typedef intptr_t (* audioMasterCallback) (AEffect *, int32_t, int32_t, intptr_t, void *, float);

#endif
