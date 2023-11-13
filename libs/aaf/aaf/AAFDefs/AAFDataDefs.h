#ifndef __DataDefinition_h__
#define __DataDefinition_h__

#include "aaf/AAFTypes.h"

// AAF well-known DataDefinition instances
//

//{01030202-0100-0000-060e-2b3404010101}
static const aafUID_t AAFDataDef_Picture =
{0x01030202, 0x0100, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};


//{6f3c8ce1-6cef-11d2-807d-006008143e6f}
static const aafUID_t AAFDataDef_LegacyPicture =
{0x6f3c8ce1, 0x6cef, 0x11d2, {0x80, 0x7d, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{05cba731-1daa-11d3-80ad-006008143e6f}
static const aafUID_t AAFDataDef_Matte =
{0x05cba731, 0x1daa, 0x11d3, {0x80, 0xad, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{05cba732-1daa-11d3-80ad-006008143e6f}
static const aafUID_t AAFDataDef_PictureWithMatte =
{0x05cba732, 0x1daa, 0x11d3, {0x80, 0xad, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{01030202-0200-0000-060e-2b3404010101}
static const aafUID_t AAFDataDef_Sound =
{0x01030202, 0x0200, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};


//{78e1ebe1-6cef-11d2-807d-006008143e6f}
static const aafUID_t AAFDataDef_LegacySound =
{0x78e1ebe1, 0x6cef, 0x11d2, {0x80, 0x7d, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{01030201-0100-0000-060e-2b3404010101}
static const aafUID_t AAFDataDef_Timecode =
{0x01030201, 0x0100, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};


//{7f275e81-77e5-11d2-807f-006008143e6f}
static const aafUID_t AAFDataDef_LegacyTimecode =
{0x7f275e81, 0x77e5, 0x11d2, {0x80, 0x7f, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{d2bb2af0-d234-11d2-89ee-006097116212}
static const aafUID_t AAFDataDef_Edgecode =
{0xd2bb2af0, 0xd234, 0x11d2, {0x89, 0xee, 0x00, 0x60, 0x97, 0x11, 0x62, 0x12}};


//{01030201-1000-0000-060e-2b3404010101}
static const aafUID_t AAFDataDef_DescriptiveMetadata =
{0x01030201, 0x1000, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};


//{01030203-0100-0000-060e-2b3404010105}
static const aafUID_t AAFDataDef_Auxiliary =
{0x01030203, 0x0100, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x05}};


//{851419d0-2e4f-11d3-8a5b-0050040ef7d2}
static const aafUID_t AAFDataDef_Unknown =
{0x851419d0, 0x2e4f, 0x11d3, {0x8a, 0x5b, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};


// AAF DataDefinition legacy aliases
//

/*
static const aafUID_t DDEF_Picture = AAFDataDef_LegacyPicture;
static const aafUID_t DDEF_Matte = AAFDataDef_Matte;
static const aafUID_t DDEF_PictureWithMatte = AAFDataDef_PictureWithMatte;
static const aafUID_t DDEF_Sound = AAFDataDef_LegacySound;
static const aafUID_t DDEF_Timecode = AAFDataDef_LegacyTimecode;
static const aafUID_t DDEF_Edgecode = AAFDataDef_Edgecode;
static const aafUID_t DDEF_Unknown = AAFDataDef_Unknown;
*/
#endif // ! __DataDefinition_h__
