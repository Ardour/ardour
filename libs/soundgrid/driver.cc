#include <stdio.h>

#include <WavesPublicAPI/WavesMixerAPI/1.0/WavesMixerAPI.h>
#include <WavesPublicAPI/WTErr.h>

/*
struct WSCoreCallbackTable
{
    WMSD_SET_EVENT_PROC         setEventProc;
    WMSD_GET_CONTROLINFO_PROC   getControlInfoProc;
};
*/

uint32_t 
WMSD_QueryInterfaceVersion()
{
        return 0;
}

WSDSurfaceHandle 
WMSD_CreateSurfaceFromPreset(const WSDCoreHandle hostHandle, 
                             const WSCoreCallbackTable* pCallbackTable, const WSMixerConfig* pMixerConfig, 
                             const void* pPresetChunk, WSDSize presetSize)
{
        return eNoErr;
}

WMSDErr 
WMSD_GetAvailableSurfaceInfo(struct WMSD_SURFACEINFO *p)
{
        snprintf (p->surfaceDriverName, sizeof (p->surfaceDriverName), "Ardour");
        snprintf (p->surfaceDriverCategory, sizeof (p->surfaceDriverCategory), "Ardour");
        snprintf (p->surfaceType, sizeof (p->surfaceType), "Ardour");

        return eNoErr;
}

WSDSurfaceHandle 
WMSD_CreateSurfaceForType(const char* pSurfaceType, 
                          const WSDCoreHandle hostHandle, const WSCoreCallbackTable* pCallbackTable, 
                          const WSMixerConfig* pMixerConfig)
{
        return eNoErr;
}

WMSDErr 
WMSD_ShowConfigWindow(const WSDSurfaceHandle surfaceHandle)
{
        return eNoErr;
}

WMSDErr 
WMSD_IdentifySurface(const WSDSurfaceHandle surfaceHandle, const bool turnOnLed)
{
        return eNoErr;
}

WMSDErr 
WMSD_GetPreset(const WSDSurfaceHandle surfaceHandle, void *pPresetChunk, 
               WSDSize *pPresetSize)
{
        return eNoErr;
}

WMSDErr 
WMSD_SetPreset(const WSDSurfaceHandle surfaceHandle, void *pPresetChunk, 
               WSDSize presetSize)
{
        return eNoErr;
}

WMSDErr 
WMSD_SurfaceDisplayUpdate(const WSDSurfaceHandle surfaceHandle, 
                          const struct WSControlID *pControlID)
{
        return eNoErr;
}

WMSDErr 
WMSD_DestroySurface(const WSDSurfaceHandle surfaceHandle)
{
        return eNoErr;
}

WMSDErr 
WMSD_GetTypeForSurface(const WSDSurfaceHandle surfaceHandle, char *out_surfaceType)
{
        return eNoErr;
}
