/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

//----------------------------------------------------------------------------------
//
//
//! \file	WCMRPortAudioDeviceManager.h
//!
//! WCMRPortAudioDeviceManager and related class declarations
//!
//---------------------------------------------------------------------------------*/
#ifndef __WCMRPortAudioDeviceManager_h_
	#define __WCMRPortAudioDeviceManager_h_

#include "WCMRAudioDeviceManager.h"
#include "WCMRNativeAudio.h"
#include "portaudio.h"

//forward decl.
class WCMRPortAudioDeviceManager;

//! Manages a port audio device, providing information
//! about the device, and managing audio callbacks.
class WCMRPortAudioDevice : public WCMRNativeAudioDevice
{
public:

	WCMRPortAudioDevice (WCMRPortAudioDeviceManager *pManager, unsigned int  deviceID, bool useMultiThreading = true, bool bNoCopy = false);///<Constructor
	virtual ~WCMRPortAudioDevice ();///<Destructor

	virtual int CurrentSamplingRate(); ///<Current Sampling rate.?
	virtual WTErr SetCurrentSamplingRate(int newRate);///<Change Current Sampling Rate : This is a requset, might not be successful at run time!

	virtual int CurrentBufferSize();///<Current Buffer Size.? - note that this may change with change in sampling rate.
	virtual WTErr SetCurrentBufferSize (int newSize);///<Change Current Buffer Size : This is a requset, might not be successful at run time!

	virtual ConnectionStates ConnectionStatus();///< Connection Status - device available, gone, disconnected

	virtual WTErr SetActive (bool newState);///<Prepare/Activate device.
	
	virtual WTErr SetStreaming (bool newState);///<Start/Stop Streaming - should reconnect connections when streaming starts!

	virtual WTErr SetMonitorChannels (int leftChannel, int rightChannel);///<Set monitor channels. - optional, will not be available with AG
	virtual WTErr SetMonitorGain (float newGain);///<Set monitor gain. - optional, will not be available with AG
	
	virtual WTErr ShowConfigPanel (void *pParam);///< Show Control Panel - in case of ASIO this will work only with Active device!

	virtual int AudioCallback (const float *pInputBuffer, float *pOutputBuffer, unsigned long framesPerBuffe, bool dropsDetectedr);

	virtual WTErr UpdateDeviceInfo ();

	virtual WTErr ResetDevice();

#ifdef PLATFORM_WINDOWS
	static long StaticASIOMessageHook (void *pRefCon, long selector, long value, void* message, double* opt);
	long ASIOMessageHook (long selector, long value, void* message, double* opt);
#endif //PLATFORM_WINDOWS
	
protected:
	static DWORD WINAPI __DoIdle__(LPVOID lpThreadParameter);

	// Methods which are executed by device processing thread
	WTErr DoIdle();///<Do Idle Processing
	void initDevice();
	void terminateDevice();
	void updateDeviceInfo(bool callerIsWaiting = false);
	void activateDevice(bool callerIsWaiting = false);
	void deactivateDevice(bool callerIsWaiting = false);
	void startStreaming(bool callerIsWaiting = false);
	void stopStreaming(bool callerIsWaiting = false);
	void resetDevice (bool callerIsWaiting = false);///<Reset device - close and reopen stream, update device information!

	PaError testStateValidness(int sampleRate, int bufferSize);
	///////////////////////////////////////////////////////////
	
	static int TheCallback (const void *pInputBuffer, void *pOutputBuffer, unsigned long framesPerBuffer, 
							const PaStreamCallbackTimeInfo* /*pTimeInfo*/, PaStreamCallbackFlags /*statusFlags*/, void *pUserData );

	unsigned int m_DeviceID; ///< The PA device id
	PaStream* m_PortAudioStream; ///< Port audio stream, when the device is active!
	bool m_StopRequested; ///< should be set to true when want to stop, set to false otherwise.
	const float *m_pInputData; ///< This is what came in with the most recent callback.
	int64_t m_SampleCounter; ///< The current running sample counter, updated by the audio callback.
	int64_t m_SampleCountAtLastIdle;

	int m_DropsDetected; ///< Number of times audio drops have been detected so far.
	int m_DropsReported; ///< Number of times audio drops have been reported so far to the client.
	bool m_IgnoreThisDrop; ///< Allows disregarding the first drop

	int m_BufferSizeChangeRequested;
	int m_BufferSizeChangeReported;
	int m_ResetRequested;
	int m_ResetReported;
	int m_ResyncRequested;
	int m_ResyncReported;

	HANDLE m_hDeviceProcessingThread;
	DWORD m_DeviceProcessingThreadID;

	///< Backend request events
	HANDLE m_hResetRequestedEvent;
	HANDLE m_hResetDone;

	HANDLE m_hUpdateDeviceInfoRequestedEvent;
	HANDLE m_hUpdateDeviceInfoDone;

	HANDLE m_hActivateRequestedEvent;
	HANDLE m_hActivationDone;

	HANDLE m_hDeActivateRequestedEvent;
	HANDLE m_hDeActivationDone;

	HANDLE m_hStartStreamingRequestedEvent;
	HANDLE m_hStartStreamingDone;

	HANDLE m_hStopStreamingRequestedEvent;
	HANDLE m_hStopStreamingDone;
	/////////////////////////

	///< Device request events
	HANDLE m_hResetFromDevRequestedEvent;
	HANDLE m_hBufferSizeChangedEvent;
	HANDLE m_hSampleRateChangedEvent;
	/////////////////////////////

	///< Sync events
	HANDLE m_hDeviceInitialized;
	HANDLE m_hExitIdleThread;

	//Should be set if the device connection status is "DeviceErrors"
	WTErr m_lastErr;
};

//! WCMRPortAudioDeviceManager
/*! The PortAudio Device Manager class */
class WCMRPortAudioDeviceManager : public WCMRAudioDeviceManager
{
public:
	WCMRPortAudioDeviceManager(WCMRAudioDeviceManagerClient *pTheClient, eAudioDeviceFilter eCurAudioDeviceFilter,
								bool useMultithreading = true, bool bNocopy = false); ///< constructor
	
	virtual ~WCMRPortAudioDeviceManager(void); ///< destructor

protected:

	virtual WCMRAudioDevice*	initNewCurrentDeviceImpl(const std::string & deviceName);
	virtual void				destroyCurrentDeviceImpl();
	virtual WTErr				generateDeviceListImpl(); // use this in derived class to fill device list
	virtual WTErr				updateDeviceListImpl() {return eNoErr; } // not supported
	virtual WTErr				getDeviceBufferSizesImpl(const std::string & deviceName, std::vector<int>& buffers) const;
    virtual WTErr				getDeviceSampleRatesImpl(const std::string & deviceName, std::vector<int>& sampleRates) const;

	bool m_UseMultithreading; ///< Flag indicates whether to use multi-threading for audio processing.
    bool m_bNoCopyAudioBuffer;

private:
	// helper functions for this class only
    WTErr getDeviceAvailableSampleRates(DeviceID deviceId, std::vector<int>& sampleRates);

	WCMRAudioDevice*			m_NoneDevice;
};

#endif //#ifndef __WCMRPortAudioDeviceManager_h_
