#ifndef __AAFFileKinds_h__
#define __AAFFileKinds_h__

#include "aaf/AAFTypes.h"

//
// The following enumerations select the file encoding type without
// specifying the implementation (in cases where multiple
// implementations are available)
//
// New code should use one of these encodings.
//

// AAF file encoded using the default encoding for the particular SDK
// release.
static const aafUID_t AAFFileKind_DontCare =
{0,0,0,{0,0,0,0,0,0,0,0}};


// AAF files encoded as structured storage with a 512 bytes sector size
static const aafUID_t AAFFileKind_Aaf512Binary =
{ 0x42464141, 0x000d, 0x4d4f, { 0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0xff } };

// AAF files structured storage with a 4096 bytes sector size
static const aafUID_t AAFFileKind_Aaf4KBinary =
{ 0x92b02efb, 0xaf40, 0x4896, { 0xa5, 0x8e, 0xd1, 0x57, 0x2f, 0x42, 0x2b, 0x58 } };

/*
Looks like Avid's Media Composer own AAFFileKind_Aaf4KBinary :

{ 0x0d010201, 0x0200, 0x0000, { 0x06, 0x0e, 0x2b, 0x34, 0x03, 0x02, 0x01, 0x01 } }

_CFB_Header____________________________________________________________________________________

_abSig              : 0xe11ab1a1e011cfd0
_clsId              : {0x0d010201, 0x0200, 0x0000, { 0x06 0x0e 0x2b 0x34 0x03 0x02 0x01 0x01 }}
 version            : 62.4 ( 0x003e 0x0004 )
_uByteOrder         : little-endian ( 0xfffe )
_uSectorShift       : 12 (4096 bytes sectors)
_uMiniSectorShift   : 6 (64 bytes mini-sectors)
_usReserved0        : 0x00
_ulReserved1        : 0x0000
_csectDir           : 92
_csectFat           : 3
_sectDirStart       : 1
_signature          : 0
_ulMiniSectorCutoff : 4096
_sectMiniFatStart   : 2721
_csectMiniFat       : 4
_sectDifStart       : -2
_csectDif           : 0



 ByteOrder            : Little-Endian (0x4949)
 LastModified         : 2017-09-23 09:38:53.00
 AAF ObjSpec Version  : 1.1
 ObjectModel Version  : 1
 Operational Pattern  : AAFOPDef_EditProtocol


 CompanyName          : Avid Technology, Inc.
 ProductName          : Avid Media Composer 8.4.5
 ProductVersion       : 8.4.0.0 (1)
 ProductVersionString : Unknown version
 ProductID            : {d0b7c06e cd3d 4ad7 {ac fb f0 3a 4f 42 a2 31}}
 Date                 : 2017-09-23 09:38:53.00
 ToolkitVersion       : 1.1.6.8635 (0)
 Platform             : AAFSDK (Win64)
 GenerationAUID       : {feadd9ba e1d3 474d {8b 3f 6a 79 a6 29 1d 2f}}

*/


// AAF files encoded as XML (text)
static const aafUID_t AAFFileKind_AafXmlText =
{ 0xfe0d0101, 0x60e1, 0x4e78, { 0xb2, 0xcd, 0x2b, 0x03, 0xdb, 0xb0, 0xfa, 0x87 } };

// AAF files encoded as KLV (binary) (MXF)
//
static const aafUID_t AAFFileKind_AafKlvBinary =
{0x4b464141, 0x000d, 0x4d4f, {0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0xff } };

// the enum to select the default implementation with 4096 byte sectors


//
// The following encodings select a specific encoding implementation
// in cases where multiple implementations are available. It is not
// advisable to use these in new code. The exist primarily for test
// purposes.
//

// the enum to select the Microsoft implementation with 512 byte sectors
static const aafUID_t AAFFileKind_AafM512Binary =
{ 0xc95e8ee6, 0xa6ec, 0x4e53, { 0x92, 0x28, 0xbd, 0x9b, 0x57, 0x23, 0x57, 0xe5 } };

// the enum to select the SchemaSoft implementation with 512 byte sectors
static const aafUID_t AAFFileKind_AafS512Binary =
{ 0xbb153a22, 0xc2ed, 0x4b2e, { 0xbb, 0x69, 0x19, 0xbd, 0x58, 0x9d, 0xf6, 0xdc } };

// the enum to select the GSF implementation with 512 byte sectors
static const aafUID_t AAFFileKind_AafG512Binary =
{ 0xb965c7f1, 0xf89d, 0x4490, { 0xbd, 0x22, 0x77, 0x35, 0x69, 0xb4, 0xd3, 0x61 } };

// the enum to select the Microsoft implementation with 4096 byte sectors
static const aafUID_t AAFFileKind_AafM4KBinary =
{ 0x7653a218, 0x3e03, 0x4ecf, { 0x87, 0x98, 0xf4, 0x5f, 0xc1, 0x17, 0x11, 0x78 } };

// the enum to select the SchemaSoft implementation with 4096 byte sectors
static const aafUID_t AAFFileKind_AafS4KBinary =
{ 0xa8ab424a, 0xc5a0, 0x48d0, { 0x9e, 0xea, 0x96, 0x69, 0x69, 0x75, 0xc6, 0xd0 } };

// the enum to select the GSF implementation with 4096 byte sectors
static const aafUID_t AAFFileKind_AafG4KBinary =
{ 0xb44818b, 0xc3dd, 0x4f0a, { 0xad, 0x37, 0xe9, 0x71, 0x0, 0x7a, 0x88, 0xe8 } };


//
// The folloing enumerations exist for testing purposes only and will
// trigger anb error if used.
//

static const aafUID_t AAFFileKind_Pathological =
{0xff,0xff,0xff,{0,0,0,0,0,0,0,0}};


//
// Old deprecated symbols which may be removed in a future release.
// Don't use these in new code.
//
/*
static const aafUID_t aafFileKindDontCare     = AAFFileKind_DontCare;
static const aafUID_t aafFileKindPathalogical = AAFFileKind_Pathological;
static const aafUID_t aafFileKindAafKlvBinary = AAFFileKind_AafKlvBinary;
static const aafUID_t aafFileKindAafMSSBinary = AAFFileKind_AafM512Binary;
static const aafUID_t aafFileKindAafM4KBinary = AAFFileKind_AafM4KBinary;
static const aafUID_t aafFileKindAafSSSBinary = AAFFileKind_AafS512Binary;
static const aafUID_t aafFileKindAafS4KBinary = AAFFileKind_AafS4KBinary;
static const aafUID_t aafFileKindAafXmlText   = AAFFileKind_AafXmlText;
static const aafUID_t aafFileKindAafSSBinary  = AAFFileKind_Aaf512Binary;
static const aafUID_t aafFileKindAaf4KBinary  = AAFFileKind_Aaf4KBinary;
*/


#endif // ! __AAFFileKinds_h__
