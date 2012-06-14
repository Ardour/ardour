#include <stdio.h>

#include <WavesPublicAPI/WavesMixerAPI/1.0/WavesMixerAPI.h>
#include <WavesPublicAPI/WTErr.h>

#include <pbd/compose.h>

#include "ardour/soundgrid.h"
#include "ardour/debug.h"

using ARDOUR::SoundGrid;
using namespace PBD;

/*
struct WSCoreCallbackTable
{
    WMSD_SET_EVENT_PROC         setEventProc;
    WMSD_GET_CONTROLINFO_PROC   getControlInfoProc;
};
*/

 /* This API is really a C API, but we want to be able to use C++ objects within it, so ... */
extern "C" {

static const char* surface_type = "ArdourSurface";

uint32_t 
WMSD_QueryInterfaceVersion()
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return WMSD_INTERFACE_VERSION;
}

WSDSurfaceHandle 
WMSD_CreateSurfaceFromPreset (const WSDCoreHandle hostHandle, 
                              const WSCoreCallbackTable* pCallbackTable, const WSMixerConfig* pMixerConfig, 
                              const void*, WSDSize)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (hostHandle, pCallbackTable, pMixerConfig);
        return (WSDSurfaceHandle) &SoundGrid::instance();
}

WMSDErr 
WMSD_GetAvailableSurfaceInfo (struct WMSD_SURFACEINFO *p)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        strncpy (p->surfaceDriverName, "ArdourSurfaceDriver", WMSD_MAX_SURFACEDRIVERNAME_LENGTH);
        strncpy (p->surfaceDriverCategory, "Ardour", WMSD_MAX_SURFACEDRIVERCATEGORY_LENGTH);
        strncpy (p->surfaceType, surface_type, WMSD_MAX_SURFACETYPE_LENGTH);

        return eNoErr;
}

WSDSurfaceHandle 
WMSD_CreateSurfaceForType (const char* /* pSurfaceType */, 
                           const WSDCoreHandle hostHandle, const WSCoreCallbackTable* pCallbackTable, 
                           const WSMixerConfig* pMixerConfig)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (hostHandle, pCallbackTable, pMixerConfig);
        return (WSDSurfaceHandle) &SoundGrid::instance();
}

WMSDErr 
WMSD_ShowConfigWindow (const WSDSurfaceHandle /* surfaceHandle */)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_IdentifySurface (const WSDSurfaceHandle /*surfaceHandle*/, const bool /*turnOnLed*/)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_GetPreset (const WSDSurfaceHandle /*surfaceHandle*/, void* /*pPresetChunk*/, WSDSize *pPresetSize)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        *pPresetSize = 0;
        return eNoErr;
}

WMSDErr 
WMSD_SetPreset (const WSDSurfaceHandle /*surfaceHandle*/, void* /*pPresetChunk*/, WSDSize /*presetSize*/)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_SurfaceDisplayUpdate (const WSDSurfaceHandle /*surfaceHandle*/, 
                           const struct WSControlID* pControlID)
{
        
        switch (pControlID->clusterID.clusterType) {
        case eClusterType_Global:
                switch (pControlID->clusterID.clusterTypeIndex) {
                case eClusterType_Global_Notification:
                        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Surface Update, notification event %1 state %2\n",
                                                                       ((WSControlIDNotification*) pControlID)->pEventTicket,
                                                                       ((WSControlIDNotification*) pControlID)->eventState));
                        SoundGrid::finalize (((WSControlIDNotification*) pControlID)->pEventTicket, 
                                             ((WSControlIDNotification*) pControlID)->eventState);

                        break;
                default:
                        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Surface Update, global index %1 ctype %2 cindex %3 cid %4\n",
                                                               pControlID->clusterID.clusterTypeIndex,
                                                               pControlID->clusterControlID.controlType,
                                                               pControlID->clusterControlID.controlTypeIndex,
                                                               pControlID->clusterControlID.controlID));
                        break;
                }
                break;
        default:
                break;
        };

        return eNoErr;
}

WMSDErr 
WMSD_DestroySurface (const WSDSurfaceHandle /*surfaceHandle*/)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (0, 0, 0);
        return eNoErr;
}

WMSDErr 
WMSD_GetTypeForSurface (const WSDSurfaceHandle /*surfaceHandle*/, char *out_surfaceType)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        strncpy (out_surfaceType, surface_type, WMSD_MAX_SURFACETYPE_LENGTH);
        return eNoErr;
}

} /* extern "C" */
