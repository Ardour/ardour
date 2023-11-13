#ifndef __InterpolationDefinition_h__
#define __InterpolationDefinition_h__

#include "aaf/AAFTypes.h"

// AAF well-known InterpolationDefinition instances
//

//{5b6c85a3-0ede-11d3-80a9-006008143e6f}
static const aafUID_t AAFInterpolationDef_None =
{0x5b6c85a3, 0x0ede, 0x11d3, {0x80, 0xa9, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{5b6c85a4-0ede-11d3-80a9-006008143e6f}
static const aafUID_t AAFInterpolationDef_Linear =
{0x5b6c85a4, 0x0ede, 0x11d3, {0x80, 0xa9, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{5b6c85a5-0ede-11d3-80a9-006008143e6f}
static const aafUID_t AAFInterpolationDef_Constant =
{0x5b6c85a5, 0x0ede, 0x11d3, {0x80, 0xa9, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{5b6c85a6-0ede-11d3-80a9-006008143e6f}
static const aafUID_t AAFInterpolationDef_BSpline =
{0x5b6c85a6, 0x0ede, 0x11d3, {0x80, 0xa9, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{15829ec3-1f24-458a-960d-c65bb23c2aa1}
static const aafUID_t AAFInterpolationDef_Log =
{0x15829ec3, 0x1f24, 0x458a, {0x96, 0x0d, 0xc6, 0x5b, 0xb2, 0x3c, 0x2a, 0xa1}};


//{c09153f7-bd18-4e5a-ad09-cbdd654fa001}
static const aafUID_t AAFInterpolationDef_Power =
{0xc09153f7, 0xbd18, 0x4e5a, {0xad, 0x09, 0xcb, 0xdd, 0x65, 0x4f, 0xa0, 0x01}};


// AAF InterpolationDefinition legacy aliases
//
/*
static const aafUID_t NoInterpolator = AAFInterpolationDef_None;
static const aafUID_t LinearInterpolator = AAFInterpolationDef_Linear;
static const aafUID_t ConstantInterpolator = AAFInterpolationDef_Constant;
static const aafUID_t BSplineInterpolator = AAFInterpolationDef_BSpline;
static const aafUID_t LogInterpolator = AAFInterpolationDef_Log;
static const aafUID_t PowerInterpolator = AAFInterpolationDef_Power;
*/
#endif // ! __InterpolationDefinition_h__
