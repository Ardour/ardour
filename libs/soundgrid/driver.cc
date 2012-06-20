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
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return WMSD_INTERFACE_VERSION;
}

WSDSurfaceHandle 
WMSD_CreateSurfaceFromPreset (const WSDCoreHandle hostHandle, 
                              const WSCoreCallbackTable* pCallbackTable, const WSMixerConfig* pMixerConfig, 
                              const void*, WSDSize)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (hostHandle, pCallbackTable, pMixerConfig);
        return (WSDSurfaceHandle) &SoundGrid::instance();
}

WMSDErr 
WMSD_GetAvailableSurfaceInfo (struct WMSD_SURFACEINFO *p)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
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
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (hostHandle, pCallbackTable, pMixerConfig);
        return (WSDSurfaceHandle) &SoundGrid::instance();
}

WMSDErr 
WMSD_ShowConfigWindow (const WSDSurfaceHandle /* surfaceHandle */)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_IdentifySurface (const WSDSurfaceHandle /*surfaceHandle*/, const bool /*turnOnLed*/)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_GetPreset (const WSDSurfaceHandle /*surfaceHandle*/, void* /*pPresetChunk*/, WSDSize *pPresetSize)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        *pPresetSize = 0;
        return eNoErr;
}

WMSDErr 
WMSD_SetPreset (const WSDSurfaceHandle /*surfaceHandle*/, void* /*pPresetChunk*/, WSDSize /*presetSize*/)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_SurfaceDisplayUpdate (const WSDSurfaceHandle /*surfaceHandle*/, 
                           const struct WSControlID* pControlID)
{
        
        switch (pControlID->clusterID.clusterType) {

        case eClusterType_Global:
                switch (pControlID->clusterID.clusterIndex) {
                case eClusterType_Global_AudioSetup:
                        DEBUG_TRACE (DEBUG::SGSurface, "AudioSetup\n");
                        break;
                case eClusterType_Global_SGSetup:
                        DEBUG_TRACE (DEBUG::SGSurface, "SGSetup\n");
                        break;
                case eClusterType_Global_DoIdleEvents:
                        DEBUG_TRACE (DEBUG::SGSurface, "DoIdleEvents\n");
                        break;
                case eClusterType_Global_AudioDevicePanel:
                        DEBUG_TRACE (DEBUG::SGSurface, "AudioDevicePanel\n");
                        break;
                case eClusterType_Global_Channel:
                        DEBUG_TRACE (DEBUG::SGSurface, "Channel\n");
                        break;
                case eClusterType_Global_RequestTimeout:
                        DEBUG_TRACE (DEBUG::SGSurface, "RequestTimeout\n");
                        break;
                case eClusterType_Global_NetworkLatency:
                        DEBUG_TRACE (DEBUG::SGSurface, "NetworkLatency\n");
                        break;
                case eClusterType_Global_SurfacesSetup:
                        DEBUG_TRACE (DEBUG::SGSurface, "SurfacesSetup\n");
                        break;
                case eClusterType_Global_TimerReason:
                        DEBUG_TRACE (DEBUG::SGSurface, "TimerReason\n");
                        break;
                case eClusterType_Global_SessionFile:
                        DEBUG_TRACE (DEBUG::SGSurface, "SessionFile\n");
                        break;
                case eClusterType_Global_PreviewMode:
                        DEBUG_TRACE (DEBUG::SGSurface, "PreviewMode\n");
                        break;
                case eClusterType_Global_Assignment:
                        DEBUG_TRACE (DEBUG::SGSurface, "Assignment\n");
                        break;
                case eClusterType_Global_Scene:
                        DEBUG_TRACE (DEBUG::SGSurface, "Scene\n");
                        break;
                case eClusterType_Global_Notification:
                        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("Surface Update, notification event %1 state %2\n",
                                                                       ((WSControlIDNotification*) pControlID)->pEventTicket,
                                                                       ((WSControlIDNotification*) pControlID)->eventState));
                        SoundGrid::finalize (((WSControlIDNotification*) pControlID)->pEventTicket, 
                                             ((WSControlIDNotification*) pControlID)->eventState);

                        break;
                default:
                        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("Surface Update, global index %1 ctype %2 cindex %3 cid %4\n",
                                                               pControlID->clusterID.clusterIndex,
                                                               pControlID->sectionControlID.sectionType,
                                                               pControlID->sectionControlID.sectionIndex,
                                                               pControlID->sectionControlID.controlID));
                        break;
                }
                break;
        case eClusterType_Input:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, InputChannel\n");
                break;
        case eClusterType_Group:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, GroupChannel\n");
                break;
        case eClusterType_Aux:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, AuxChannel\n");
                break;
        case eClusterType_Matrix:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, MatrixChannel\n");
                break;
        case eClusterType_LCRM:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, LCRMChannel\n");
                break;
        case eClusterType_DCA:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, DCAChannel\n");
                break;
        case eClusterType_Cue:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, CueChannel\n");
                break;
        case eClusterType_TB:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, TBChannel\n");
                break;
        case eClusterType_Inputs:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, Inputs\n");
                break;
        case eClusterType_Outputs:
                // DEBUG_TRACE (DEBUG::SGSurface, "update, Outputs\n");
                break;

        default:
                break;
        };

        return eNoErr;
}

WMSDErr 
WMSD_DestroySurface (const WSDSurfaceHandle /*surfaceHandle*/)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (0, 0, 0);
        return eNoErr;
}

WMSDErr 
WMSD_GetTypeForSurface (const WSDSurfaceHandle /*surfaceHandle*/, char *out_surfaceType)
{
        DEBUG_TRACE (DEBUG::SGSurface, string_compose ("SurfaceDriver:%1\n", __FUNCTION__));
        strncpy (out_surfaceType, surface_type, WMSD_MAX_SURFACETYPE_LENGTH);
        return eNoErr;
}

} /* extern "C" */
