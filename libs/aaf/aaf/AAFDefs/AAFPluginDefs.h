#ifndef __PluginDefinition_h__
#define __PluginDefinition_h__

#include "aaf/AAFTypes.h"

// AAF well-known PluginDefinition instances
//

//{3d1dd891-e793-11d2-809e-006008143e6f}
static const aafUID_t kAAFPlatform_Independent =
{0x3d1dd891, 0xe793, 0x11d2, {0x80, 0x9e, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{9fdef8c1-e847-11d2-809e-006008143e6f}
static const aafUID_t kAAFEngine_None =
{0x9fdef8c1, 0xe847, 0x11d2, {0x80, 0x9e, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{69c870a1-e793-11d2-809e-006008143e6f}
static const aafUID_t kAAFPluginAPI_EssenceAccess =
{0x69c870a1, 0xe793, 0x11d2, {0x80, 0x9e, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};


//{56905e0b-537d-11d4-a36c-009027dfca6a}
static const aafUID_t kAAFPluginCategory_Codec =
{0x56905e0b, 0x537d, 0x11d4, {0xa3, 0x6c, 0x00, 0x90, 0x27, 0xdf, 0xca, 0x6a}};


// AAF PluginDefinition legacy aliases
//

static const aafUID_t kAAFPlatformIndependant = kAAFPlatform_Independent;
static const aafUID_t kAAFNoEngine = kAAFEngine_None;
static const aafUID_t kAAFEssencePluginAPI = kAAFPluginAPI_EssenceAccess;
static const aafUID_t kAAFPluginNoCategory = kAAFPluginCategory_Codec;

#endif // ! __PluginDefinition_h__
