#include <stdio.h>

#include <WavesMixerAPI/1.0/WavesMixerAPI.h>
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

static const char* controller_type = PROGRAM_NAME;

uint32_t 
WMSD_QueryInterfaceVersion()
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        return WMSD_INTERFACE_VERSION;
}

WSDControllerHandle 
WMSD_CreateControllerFromPreset (const WSDCoreHandle hostHandle, 
                              const WSCoreCallbackTable* pCallbackTable, const WSMixerConfig* pMixerConfig, 
                              const void*, WSDSize)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (hostHandle, pCallbackTable, pMixerConfig);
        return (WSDControllerHandle) &SoundGrid::instance();
}

WMSDErr 
WMSD_GetAvailableControllerInfo (struct WMSD_CONTROLLERINFO *p)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        strncpy (p->mixerDriverName, "ArdourControllerDriver", WMSD_MAX_MIXERDRIVERNAME_LENGTH);
        strncpy (p->mixerDriverCategory, "Ardour", WMSD_MAX_MIXERDRIVERCATEGORY_LENGTH);
        strncpy (p->controllerType, controller_type, WMSD_MAX_CONTROLLERTYPE_LENGTH);

        return eNoErr;
}

WSDControllerHandle 
WMSD_CreateControllerForType (const char* /* pControllerType */, 
                           const WSDCoreHandle hostHandle, const WSCoreCallbackTable* pCallbackTable, 
                           const WSMixerConfig* pMixerConfig)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (hostHandle, pCallbackTable, pMixerConfig);
        return (WSDControllerHandle) &SoundGrid::instance();
}

WMSDErr 
WMSD_ShowConfigWindow (const WSDControllerHandle /* controllerHandle */)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_IdentifyController (const WSDControllerHandle /*controllerHandle*/, const bool /*turnOnLed*/)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_GetPreset (const WSDControllerHandle /*controllerHandle*/, void* /*pPresetChunk*/, WSDSize *pPresetSize)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        *pPresetSize = 0;
        return eNoErr;
}

WMSDErr 
WMSD_SetPreset (const WSDControllerHandle /*controllerHandle*/, void* /*pPresetChunk*/, WSDSize /*presetSize*/)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        return eNoErr;
}

WMSDErr 
WMSD_CommandStatusUpdate(const WSDControllerHandle controllerHandle, struct WSCommand* pCommand )
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("CommandStatusUpdate, controllerHandle = %1, commandStatus = %2\n",
                                                       controllerHandle, pCommand->out_status));
        SoundGrid* sg = (SoundGrid *)controllerHandle;

        if (sg) {
                sg->command_status_update (pCommand);
        }

        return eNoErr;
}

WMSDErr 
WMSD_ControllerDisplayUpdate (const WSDControllerHandle controllerHandle, 
                           const struct WSControlID* pControlID)
{
        SoundGrid* sg = (SoundGrid*) controllerHandle;

        switch (pControlID->clusterID.clusterType) {

        case eClusterType_Global:
                switch (pControlID->clusterID.clusterHandle) {
                case eClusterType_Global_AudioSetup:
                        DEBUG_TRACE (DEBUG::SGDriver, "AudioSetup\n");
                        break;
                case eClusterType_Global_SGSetup:
                        DEBUG_TRACE (DEBUG::SGDriver, "SGSetup\n");
                        break;
                case eClusterType_Global_DoIdleEvents:
                        DEBUG_TRACE (DEBUG::SGDriver, "DoIdleEvents\n");
                        break;
                case eClusterType_Global_AudioDevicePanel:
                        DEBUG_TRACE (DEBUG::SGDriver, "AudioDevicePanel\n");
                        break;
                case eClusterType_Global_Channel:
                        DEBUG_TRACE (DEBUG::SGDriver, "Channel\n");
                        break;
                case eClusterType_Global_RequestTimeout:
                        DEBUG_TRACE (DEBUG::SGDriver, "RequestTimeout\n");
                        break;
                case eClusterType_Global_SurfacesSetup:
                        DEBUG_TRACE (DEBUG::SGDriver, "SurfacesSetup\n");
                        break;
                case eClusterType_Global_TimerReason:
                        DEBUG_TRACE (DEBUG::SGDriver, "TimerReason\n");
                        break;
                case eClusterType_Global_SessionFile:
                        DEBUG_TRACE (DEBUG::SGDriver, "SessionFile\n");
                        break;
                case eClusterType_Global_PreviewMode:
                        DEBUG_TRACE (DEBUG::SGDriver, "PreviewMode\n");
                        break;
                case eClusterType_Global_Assignment:
                        DEBUG_TRACE (DEBUG::SGDriver, "Assignment\n");
                        break;
                case eClusterType_Global_Scene:
                        DEBUG_TRACE (DEBUG::SGDriver, "Scene\n");
                        break;
                case eClusterType_Global_Notification:
                        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("Controller Update, notification event %1 state %2\n",
                                                                       ((WSControlIDNotification*) pControlID)->pEventTicket,
                                                                       ((WSControlIDNotification*) pControlID)->eventState));
                        if (sg) {
                                sg->finalize (((WSControlIDNotification*) pControlID)->pEventTicket, 
                                              ((WSControlIDNotification*) pControlID)->eventState);
                        }

                        break;
                default:
                        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("Controller Update, global index %1 ctype %2 cindex %3 cid %4\n",
                                                               pControlID->clusterID.clusterHandle,
                                                               pControlID->sectionControlID.sectionType,
                                                               pControlID->sectionControlID.sectionIndex,
                                                               pControlID->sectionControlID.controlID));
                        break;
                }
                break;
        case eClusterType_Input:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, InputChannel\n");
                break;
        case eClusterType_Group:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, GroupChannel\n");
                break;
        case eClusterType_Aux:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, AuxChannel\n");
                break;
        case eClusterType_Matrix:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, MatrixChannel\n");
                break;
        case eClusterType_LCRM:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, LCRMChannel\n");
                break;
        case eClusterType_DCA:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, DCAChannel\n");
                break;
        case eClusterType_Cue:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, CueChannel\n");
                break;
        case eClusterType_TB:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, TBChannel\n");
                break;
        case eClusterType_Inputs:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, Inputs\n");
                break;
        case eClusterType_Outputs:
                // DEBUG_TRACE (DEBUG::SGDriver, "update, Outputs\n");
                break;

        default:
                break;
        };

        return eNoErr;
}

WMSDErr 
WMSD_DestroyController (const WSDControllerHandle /*controllerHandle*/)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        SoundGrid::driver_register (0, 0, 0);
        return eNoErr;
}

WMSDErr 
WMSD_GetTypeForController (const WSDControllerHandle /*controllerHandle*/, char *out_controllerType)
{
        DEBUG_TRACE (DEBUG::SGDriver, string_compose ("ControllerDriver:%1\n", __FUNCTION__));
        strncpy (out_controllerType, controller_type, WMSD_MAX_CONTROLLERTYPE_LENGTH);
        return eNoErr;
}

} /* extern "C" */
