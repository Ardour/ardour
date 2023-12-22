#ifndef __ParameterDefinition_h__
#define __ParameterDefinition_h__

#include <libaaf/AAFTypes.h>

// AAF well-known ParameterDefinition instances
//

//{e4962320-2267-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_Level = {
    0xe4962320,
    0x2267,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{e4962323-2267-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEWipeNumber = {
    0xe4962323,
    0x2267,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba0-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEReverse = {
    0x9c894ba0,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{72559a80-24d7-11d3-8a50-0050040ef7d2}
static const aafUID_t AAFParameterDef_SpeedRatio = {
    0x72559a80,
    0x24d7,
    0x11d3,
    {0x8a, 0x50, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{c573a510-071a-454f-b617-ad6ae69054c2}
static const aafUID_t AAFParameterDef_PositionOffsetX = {
    0xc573a510,
    0x071a,
    0x454f,
    {0xb6, 0x17, 0xad, 0x6a, 0xe6, 0x90, 0x54, 0xc2}};

//{82e27478-1336-4ea3-bcb9-6b8f17864c42}
static const aafUID_t AAFParameterDef_PositionOffsetY = {
    0x82e27478,
    0x1336,
    0x4ea3,
    {0xbc, 0xb9, 0x6b, 0x8f, 0x17, 0x86, 0x4c, 0x42}};

//{d47b3377-318c-4657-a9d8-75811b6dc3d1}
static const aafUID_t AAFParameterDef_CropLeft = {
    0xd47b3377,
    0x318c,
    0x4657,
    {0xa9, 0xd8, 0x75, 0x81, 0x1b, 0x6d, 0xc3, 0xd1}};

//{5ecc9dd5-21c1-462b-9fec-c2bd85f14033}
static const aafUID_t AAFParameterDef_CropRight = {
    0x5ecc9dd5,
    0x21c1,
    0x462b,
    {0x9f, 0xec, 0xc2, 0xbd, 0x85, 0xf1, 0x40, 0x33}};

//{8170a539-9b55-4051-9d4e-46598d01b914}
static const aafUID_t AAFParameterDef_CropTop = {
    0x8170a539,
    0x9b55,
    0x4051,
    {0x9d, 0x4e, 0x46, 0x59, 0x8d, 0x01, 0xb9, 0x14}};

//{154ba82b-990a-4c80-9101-3037e28839a1}
static const aafUID_t AAFParameterDef_CropBottom = {
    0x154ba82b,
    0x990a,
    0x4c80,
    {0x91, 0x01, 0x30, 0x37, 0xe2, 0x88, 0x39, 0xa1}};

//{8d568129-847e-11d5-935a-50f857c10000}
static const aafUID_t AAFParameterDef_ScaleX = {
    0x8d568129,
    0x847e,
    0x11d5,
    {0x93, 0x5a, 0x50, 0xf8, 0x57, 0xc1, 0x00, 0x00}};

//{8d56812a-847e-11d5-935a-50f857c10000}
static const aafUID_t AAFParameterDef_ScaleY = {
    0x8d56812a,
    0x847e,
    0x11d5,
    {0x93, 0x5a, 0x50, 0xf8, 0x57, 0xc1, 0x00, 0x00}};

//{062cfbd8-f4b1-4a50-b944-f39e2fc73c17}
static const aafUID_t AAFParameterDef_Rotation = {
    0x062cfbd8,
    0xf4b1,
    0x4a50,
    {0xb9, 0x44, 0xf3, 0x9e, 0x2f, 0xc7, 0x3c, 0x17}};

//{72a3b4a2-873d-4733-9052-9f83a706ca5b}
static const aafUID_t AAFParameterDef_PinTopLeftX = {
    0x72a3b4a2,
    0x873d,
    0x4733,
    {0x90, 0x52, 0x9f, 0x83, 0xa7, 0x06, 0xca, 0x5b}};

//{29e4d78f-a502-4ebb-8c07-ed5a0320c1b0}
static const aafUID_t AAFParameterDef_PinTopLeftY = {
    0x29e4d78f,
    0xa502,
    0x4ebb,
    {0x8c, 0x07, 0xed, 0x5a, 0x03, 0x20, 0xc1, 0xb0}};

//{a95296c0-1ed9-4925-8481-2096c72e818d}
static const aafUID_t AAFParameterDef_PinTopRightX = {
    0xa95296c0,
    0x1ed9,
    0x4925,
    {0x84, 0x81, 0x20, 0x96, 0xc7, 0x2e, 0x81, 0x8d}};

//{ce1757ae-7a0b-45d9-b3f3-3686adff1e2d}
static const aafUID_t AAFParameterDef_PinTopRightY = {
    0xce1757ae,
    0x7a0b,
    0x45d9,
    {0xb3, 0xf3, 0x36, 0x86, 0xad, 0xff, 0x1e, 0x2d}};

//{08b2bc81-9b1b-4c01-ba73-bba3554ed029}
static const aafUID_t AAFParameterDef_PinBottomLeftX = {
    0x08b2bc81,
    0x9b1b,
    0x4c01,
    {0xba, 0x73, 0xbb, 0xa3, 0x55, 0x4e, 0xd0, 0x29}};

//{c163f2ff-cd83-4655-826e-3724ab7fa092}
static const aafUID_t AAFParameterDef_PinBottomLeftY = {
    0xc163f2ff,
    0xcd83,
    0x4655,
    {0x82, 0x6e, 0x37, 0x24, 0xab, 0x7f, 0xa0, 0x92}};

//{53bc5884-897f-479e-b833-191f8692100d}
static const aafUID_t AAFParameterDef_PinBottomRightX = {
    0x53bc5884,
    0x897f,
    0x479e,
    {0xb8, 0x33, 0x19, 0x1f, 0x86, 0x92, 0x10, 0x0d}};

//{812fb15b-0b95-4406-878d-efaa1cffc129}
static const aafUID_t AAFParameterDef_PinBottomRightY = {
    0x812fb15b,
    0x0b95,
    0x4406,
    {0x87, 0x8d, 0xef, 0xaa, 0x1c, 0xff, 0xc1, 0x29}};

//{a2667f65-65d8-4abf-a179-0b9b93413949}
static const aafUID_t AAFParameterDef_AlphaKeyInvertAlpha = {
    0xa2667f65,
    0x65d8,
    0x4abf,
    {0xa1, 0x79, 0x0b, 0x9b, 0x93, 0x41, 0x39, 0x49}};

//{21ed5b0f-b7a0-43bc-b779-c47f85bf6c4d}
static const aafUID_t AAFParameterDef_LumKeyLevel = {
    0x21ed5b0f,
    0xb7a0,
    0x43bc,
    {0xb7, 0x79, 0xc4, 0x7f, 0x85, 0xbf, 0x6c, 0x4d}};

//{cbd39b25-3ece-441e-ba2c-da473ab5cc7c}
static const aafUID_t AAFParameterDef_LumKeyClip = {
    0xcbd39b25,
    0x3ece,
    0x441e,
    {0xba, 0x2c, 0xda, 0x47, 0x3a, 0xb5, 0xcc, 0x7c}};

//{e4962321-2267-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_Amplitude = {
    0xe4962321,
    0x2267,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{e4962322-2267-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_Pan = {
    0xe4962322,
    0x2267,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9e610007-1be2-41e1-bb11-c95de9964d03}
static const aafUID_t AAFParameterDef_OutgoingLevel = {
    0x9e610007,
    0x1be2,
    0x41e1,
    {0xbb, 0x11, 0xc9, 0x5d, 0xe9, 0x96, 0x4d, 0x03}};

//{48cea642-a8f9-455b-82b3-86c814b797c7}
static const aafUID_t AAFParameterDef_IncomingLevel = {
    0x48cea642,
    0xa8f9,
    0x455b,
    {0x82, 0xb3, 0x86, 0xc8, 0x14, 0xb7, 0x97, 0xc7}};

//{cb7c0ec4-f45f-4ee6-aef0-c63ddb134924}
static const aafUID_t AAFParameterDef_OpacityLevel = {
    0xcb7c0ec4,
    0xf45f,
    0x4ee6,
    {0xae, 0xf0, 0xc6, 0x3d, 0xdb, 0x13, 0x49, 0x24}};

//{7b92827b-5ae3-465e-b5f9-5ee21b070859}
static const aafUID_t AAFParameterDef_TitleText = {
    0x7b92827b,
    0x5ae3,
    0x465e,
    {0xb5, 0xf9, 0x5e, 0xe2, 0x1b, 0x07, 0x08, 0x59}};

//{e8eb7f50-602f-4a2f-8fb2-86c8826ccf24}
static const aafUID_t AAFParameterDef_TitleFontName = {
    0xe8eb7f50,
    0x602f,
    0x4a2f,
    {0x8f, 0xb2, 0x86, 0xc8, 0x82, 0x6c, 0xcf, 0x24}};

//{01c55287-31b3-4f8f-bb87-c92f06eb7f5a}
static const aafUID_t AAFParameterDef_TitleFontSize = {
    0x01c55287,
    0x31b3,
    0x4f8f,
    {0xbb, 0x87, 0xc9, 0x2f, 0x06, 0xeb, 0x7f, 0x5a}};

//{dfe86f24-8a71-4dc5-83a2-988f583af711}
static const aafUID_t AAFParameterDef_TitleFontColorR = {
    0xdfe86f24,
    0x8a71,
    0x4dc5,
    {0x83, 0xa2, 0x98, 0x8f, 0x58, 0x3a, 0xf7, 0x11}};

//{f9f41222-36d9-4650-bd5a-a17866cf86b9}
static const aafUID_t AAFParameterDef_TitleFontColorG = {
    0xf9f41222,
    0x36d9,
    0x4650,
    {0xbd, 0x5a, 0xa1, 0x78, 0x66, 0xcf, 0x86, 0xb9}};

//{f5ba87fa-cf72-4f37-a736-d7096fcb06f1}
static const aafUID_t AAFParameterDef_TitleFontColorB = {
    0xf5ba87fa,
    0xcf72,
    0x4f37,
    {0xa7, 0x36, 0xd7, 0x09, 0x6f, 0xcb, 0x06, 0xf1}};

//{47c1733f-6afb-4168-9b6d-476adfbae7ab}
static const aafUID_t AAFParameterDef_TitleAlignment = {
    0x47c1733f,
    0x6afb,
    0x4168,
    {0x9b, 0x6d, 0x47, 0x6a, 0xdf, 0xba, 0xe7, 0xab}};

//{8b5732c0-be8e-4332-aa71-5d866add777d}
static const aafUID_t AAFParameterDef_TitleBold = {
    0x8b5732c0,
    0xbe8e,
    0x4332,
    {0xaa, 0x71, 0x5d, 0x86, 0x6a, 0xdd, 0x77, 0x7d}};

//{e4a3c91b-f96a-4dd4-91d8-1ba32000ab72}
static const aafUID_t AAFParameterDef_TitleItalic = {
    0xe4a3c91b,
    0xf96a,
    0x4dd4,
    {0x91, 0xd8, 0x1b, 0xa3, 0x20, 0x00, 0xab, 0x72}};

//{a25061da-db25-402e-89ff-a6d0efa39444}
static const aafUID_t AAFParameterDef_TitlePositionX = {
    0xa25061da,
    0xdb25,
    0x402e,
    {0x89, 0xff, 0xa6, 0xd0, 0xef, 0xa3, 0x94, 0x44}};

//{6151541f-9d3f-4a0e-a3f9-24cc60eea969}
static const aafUID_t AAFParameterDef_TitlePositionY = {
    0x6151541f,
    0x9d3f,
    0x4a0e,
    {0xa3, 0xf9, 0x24, 0xcc, 0x60, 0xee, 0xa9, 0x69}};

//{be2033da-723b-4146-ace0-3299e0ff342e}
static const aafUID_t AAFParameterDef_ColorSlopeR = {
    0xbe2033da,
    0x723b,
    0x4146,
    {0xac, 0xe0, 0x32, 0x99, 0xe0, 0xff, 0x34, 0x2e}};

//{7ca8e01b-c6d8-4b3f-b251-28a53e5b958f}
static const aafUID_t AAFParameterDef_ColorSlopeG = {
    0x7ca8e01b,
    0xc6d8,
    0x4b3f,
    {0xb2, 0x51, 0x28, 0xa5, 0x3e, 0x5b, 0x95, 0x8f}};

//{1aeb007b-3cd5-4814-87b5-cbd6a3cdfe8d}
static const aafUID_t AAFParameterDef_ColorSlopeB = {
    0x1aeb007b,
    0x3cd5,
    0x4814,
    {0x87, 0xb5, 0xcb, 0xd6, 0xa3, 0xcd, 0xfe, 0x8d}};

//{4d1e65e0-85fc-4bb9-a264-13cf320a8539}
static const aafUID_t AAFParameterDef_ColorOffsetR = {
    0x4d1e65e0,
    0x85fc,
    0x4bb9,
    {0xa2, 0x64, 0x13, 0xcf, 0x32, 0x0a, 0x85, 0x39}};

//{76f783e4-0bbd-41d7-b01e-f418c1602a6f}
static const aafUID_t AAFParameterDef_ColorOffsetG = {
    0x76f783e4,
    0x0bbd,
    0x41d7,
    {0xb0, 0x1e, 0xf4, 0x18, 0xc1, 0x60, 0x2a, 0x6f}};

//{57110628-522d-4b48-8a28-75477ced984d}
static const aafUID_t AAFParameterDef_ColorOffsetB = {
    0x57110628,
    0x522d,
    0x4b48,
    {0x8a, 0x28, 0x75, 0x47, 0x7c, 0xed, 0x98, 0x4d}};

//{c2d79c3a-9263-40d9-827d-953ac6b88813}
static const aafUID_t AAFParameterDef_ColorPowerR = {
    0xc2d79c3a,
    0x9263,
    0x40d9,
    {0x82, 0x7d, 0x95, 0x3a, 0xc6, 0xb8, 0x88, 0x13}};

//{524d52e6-86a3-4f41-864b-fb53b15b1d5d}
static const aafUID_t AAFParameterDef_ColorPowerG = {
    0x524d52e6,
    0x86a3,
    0x4f41,
    {0x86, 0x4b, 0xfb, 0x53, 0xb1, 0x5b, 0x1d, 0x5d}};

//{5f0cc7dc-907d-4153-bf00-1f3cdf3c05bb}
static const aafUID_t AAFParameterDef_ColorPowerB = {
    0x5f0cc7dc,
    0x907d,
    0x4153,
    {0xbf, 0x00, 0x1f, 0x3c, 0xdf, 0x3c, 0x05, 0xbb}};

//{0b135705-3312-4d03-ba89-be9ef45e5470}
static const aafUID_t AAFParameterDef_ColorSaturation = {
    0x0b135705,
    0x3312,
    0x4d03,
    {0xba, 0x89, 0xbe, 0x9e, 0xf4, 0x5e, 0x54, 0x70}};

//{f3b9466a-2579-4168-beb5-66b996919a3f}
static const aafUID_t AAFParameterDef_ColorCorrectionDescription = {
    0xf3b9466a,
    0x2579,
    0x4168,
    {0xbe, 0xb5, 0x66, 0xb9, 0x96, 0x91, 0x9a, 0x3f}};

//{b0124dbe-7f97-443c-ae39-c49c1c53d728}
static const aafUID_t AAFParameterDef_ColorInputDescription = {
    0xb0124dbe,
    0x7f97,
    0x443c,
    {0xae, 0x39, 0xc4, 0x9c, 0x1c, 0x53, 0xd7, 0x28}};

//{5a9dfc6f-611f-4db8-8eff-3b9cdb6e1220}
static const aafUID_t AAFParameterDef_ColorViewingDescription = {
    0x5a9dfc6f,
    0x611f,
    0x4db8,
    {0x8e, 0xff, 0x3b, 0x9c, 0xdb, 0x6e, 0x12, 0x20}};

//{9c894ba1-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTESoft = {
    0x9c894ba1,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba2-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEBorder = {
    0x9c894ba2,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba3-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEPosition = {
    0x9c894ba3,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba4-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEModulator = {
    0x9c894ba4,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba5-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEShadow = {
    0x9c894ba5,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba6-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTETumble = {
    0x9c894ba6,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba7-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTESpotlight = {
    0x9c894ba7,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba8-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEReplicationH = {
    0x9c894ba8,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894ba9-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTEReplicationV = {
    0x9c894ba9,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{9c894baa-2277-11d3-8a4c-0050040ef7d2}
static const aafUID_t AAFParameterDef_SMPTECheckerboard = {
    0x9c894baa,
    0x2277,
    0x11d3,
    {0x8a, 0x4c, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

//{5f1c2560-2415-11d3-8a4f-0050040ef7d2}
static const aafUID_t AAFParameterDef_PhaseOffset = {
    0x5f1c2560,
    0x2415,
    0x11d3,
    {0x8a, 0x4f, 0x00, 0x50, 0x04, 0x0e, 0xf7, 0xd2}};

// AAF ParameterDefinition legacy aliases
//
/*
static const aafUID_t AAFParameterDefLevel = AAFParameterDef_Level;
static const aafUID_t AAFParameterDefSMPTEWipeNumber =
AAFParameterDef_SMPTEWipeNumber; static const aafUID_t
AAFParameterDefSMPTEReverse = AAFParameterDef_SMPTEReverse; static const
aafUID_t AAFParameterDefSpeedRatio = AAFParameterDef_SpeedRatio; static const
aafUID_t AAFParameterDefAmplitude = AAFParameterDef_Amplitude; static const
aafUID_t AAFParameterDefPan = AAFParameterDef_Pan; static const aafUID_t
AAFParameterDefSMPTESoft = AAFParameterDef_SMPTESoft; static const aafUID_t
AAFParameterDefSMPTEBorder = AAFParameterDef_SMPTEBorder; static const aafUID_t
AAFParameterDefSMPTEPosition = AAFParameterDef_SMPTEPosition; static const
aafUID_t AAFParameterDefSMPTEModulator = AAFParameterDef_SMPTEModulator; static
const aafUID_t AAFParameterDefSMPTEShadow = AAFParameterDef_SMPTEShadow; static
const aafUID_t AAFParameterDefSMPTETumble = AAFParameterDef_SMPTETumble; static
const aafUID_t AAFParameterDefSMPTESpotlight = AAFParameterDef_SMPTESpotlight;
static const aafUID_t AAFParameterDefSMPTEReplicationH =
AAFParameterDef_SMPTEReplicationH; static const aafUID_t
AAFParameterDefSMPTEReplicationV = AAFParameterDef_SMPTEReplicationV; static
const aafUID_t AAFParameterDefSMPTECheckerboard =
AAFParameterDef_SMPTECheckerboard; static const aafUID_t
AAFParameterDefPhaseOffset = AAFParameterDef_PhaseOffset;
*/

/* Seen in Avid and PT files */
// static const aafUID_t PanVol_IsTrimGainEffect =
// {0x922d458d, 0x8f22, 0x11d4, {0xa0, 0x3c, 0x00, 0x04, 0xac, 0x96, 0x9f,
// 0x50}};

#endif // ! __ParameterDefinition_h__
