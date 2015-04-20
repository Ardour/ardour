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
//! \file	WCMRCoreAudioDeviceManager.h
//!
//! WCMRCoreAudioDeviceManager and related class declarations
//!
//---------------------------------------------------------------------------------*/
#ifndef __WCMRCoreAudioDeviceManager_h_
	#define __WCMRCoreAudioDeviceManager_h_

#include "WCMRAudioDeviceManager.h"
#include "WCMRNativeAudio.h"
#include "Threads/WCThreadSafe.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include <mach/mach.h>

#include <CoreAudio/CoreAudio.h> 

//forward decl.
class WCMRCoreAudioDeviceManager;

#define WV_USE_TONE_GEN 0 ///! Set this to 1 to use a tone generator for input. See description at SetupToneGenerator for details.

// This enum is for choosing filter for audio devices scan
typedef enum eCABS_Method
{
	eCABS_Simple = 0,       
	eCABS_DestructiveCache,
	eCABS_CacheOnDeviceSet,    
	eCABS_MethodNum // Must be last
}	eCABS_Method;

//! Manages a port audio device, providing information
//! about the device, and managing audio callbacks.
class WCMRCoreAudioDevice : public WCMRNativeAudioDevice
{
public:

	WCMRCoreAudioDevice (WCMRCoreAudioDeviceManager *pManager, AudioDeviceID deviceID, bool useMultithreading = true, bool bNocopy = false);///<Constructor
	virtual ~WCMRCoreAudioDevice ();///<Destructor

	virtual const std::string& DeviceName() const;///<Name?
	virtual const std::vector<std::string>& InputChannels();///<Current Input Channel List? - note that this may change with change in sampling rate.
	virtual const std::vector<std::string>& OutputChannels();///<Current Output Channel List? - note that this may change with change in sampling rate.
    
    
	virtual const std::vector<int>& SamplingRates();///<Supported Sampling Rate List?
	virtual int CurrentSamplingRate(); ///<Current Sampling rate.?
	virtual WTErr SetCurrentSamplingRate(int newRate);///<Change Current Sampling Rate : This is a requset, might not be successful at run time!

	virtual const std::vector<int>& BufferSizes();///<Supported Buffer Size List? - note that this may change with change in sampling rate.
	virtual int CurrentBufferSize();///<Current Buffer Size.? - note that this may change with change in sampling rate.
	virtual WTErr SetCurrentBufferSize (int newSize);///<Change Current Buffer Size : This is a requset, might not be successful at run time!

	virtual ConnectionStates ConnectionStatus();///< Connection Status - device available, gone, disconnected

	virtual WTErr SetActive (bool newState);///<Prepare/Activate device.
	virtual WTErr SetStreaming (bool newState);///<Start/Stop Streaming - should reconnect connections when streaming starts!

	virtual WTErr DoIdle();///<Do Idle Processing
	
	virtual WTErr SetMonitorChannels (int leftChannel, int rightChannel);///<Set monitor channels. - optional, will not be available with AG
	virtual WTErr SetMonitorGain (float newGain);///<Set monitor gain. - optional, will not be available with AG
	
	virtual WTErr ShowConfigPanel (void *pParam);///< Show Control Panel - in case of ASIO this will work only with Active device!

	virtual int AudioCallback (float *pOutputBuffer, unsigned long framesPerBuffer, int64_t inSampleTime, uint64_t inCycleStartTime);
	
	AudioDeviceID DeviceID () {return m_DeviceID;}
    
    virtual uint32_t GetLatency (bool isInput); ///< Get latency.
    virtual OSStatus GetStreamLatency(AudioDeviceID deviceID, bool isInput, std::vector<int>& latencies);

    
protected:

	AudioDeviceID m_DeviceID; ///< The CoreAudio device id
	bool m_StopRequested; ///< should be set to true when want to stop, set to false otherwise.
    float *m_pInputData; ///< This is what came in with the most recent callback.
	int64_t m_SampleCounter; ///< The current running sample counter, updated by the audio callback.
	int64_t m_SampleCountAtLastIdle; ///< What was the sample count last time we checked...
	int m_StalledSampleCounter; ///< The number of idle calls with same sample count detected
	int m_ChangeCheckCounter; ///< The number of idle calls passed since we checked the buffer size change.

	wvNS::wvThread::timestamp m_LastCPULog; ///< The time when the last CPU details log was sent as a notification.
//	unsigned int m_IOCyclesTimesTaken[MAX_IOCYCLE_TIMES]; ///< This stores the times taken by each IOCycle, in host-time units.
//	int m_CurrentIOCycle; ///< The location in m_IOCyclesTymesTaken array, where the next cycle's value will go.
//	int m_CyclesToAccumulate; ///< The number of cycles to accumulate the values for - maximum for last one second.
//	unsigned int m_CyclePeriod; ///< The number of host time units for a cycle period - determined by buffer size and sampling rate
	

	AudioUnit m_AUHALAudioUnit;///< The AUHAL AudioUnit

	int m_BufferSizeChangeRequested;
	int m_BufferSizeChangeReported;
	int m_ResetRequested;
	int m_ResetReported;
	int m_ResyncRequested;
	int m_ResyncReported;
	int m_SRChangeRequested;
	int m_SRChangeReported;

	int m_DropsDetected; ///< Number of times audio drops have been detected so far.
	int m_DropsReported; ///< Number of times audio drops have been reported so far to the client.
	bool m_IgnoreThisDrop; ///< Allows disregarding the first drop

	thread_t m_IOProcThreadPort; ///< Thread handle to calculate CPU consumption.
	int m_CPUCount; ///< Number of processors/core to normalize cpu consumption calculation.

#if WV_USE_TONE_GEN
	//The Tone Generator...
	float_t *m_pToneData;
	uint32_t m_ToneDataSamples;
	uint32_t m_NextSampleToUse;
#endif //WV_USE_TONE_GEN
	
	WTErr UpdateDeviceInfo ();
	WTErr UpdateDeviceName();
	WTErr UpdateDeviceInputs();
	WTErr UpdateDeviceOutputs();
	WTErr UpdateDeviceSampleRates();
	WTErr UpdateDeviceBufferSizes();
	WTErr SetWorkingBufferSize(int newSize);
	OSStatus SetBufferSizesByIO(int newSize);
	WTErr SetAndCheckCurrentSamplingRate (int newRate);

	WTErr EnableAudioUnitIO();
	WTErr virtual EnableListeners();
	WTErr virtual DisableListeners();
	WTErr SetupAUHAL();
	WTErr TearDownAUHAL();

#if WV_USE_TONE_GEN
	void SetupToneGenerator ();
#endif //WV_USE_TONE_GEN
	
	static OSStatus StaticAudioIOProc(void *inRefCon, AudioUnitRenderActionFlags *	ioActionFlags,
		const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *ioData);
	OSStatus AudioIOProc(AudioUnitRenderActionFlags *	ioActionFlags,
		const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *ioData);
		
	static OSStatus StaticPropertyChangeProc (AudioDeviceID inDevice, UInt32 inChannel, Boolean isInput,
	AudioDevicePropertyID inPropertyID, void *inClientData);
	void PropertyChangeProc (AudioDevicePropertyID inPropertyID);
    
    void resetAudioDevice();
private:

};


//! WCMRCoreAudioDeviceManager
/*! The CoreAudio Device Manager class */
class WCMRCoreAudioDeviceManager : public WCMRAudioDeviceManager
{
public:

	WCMRCoreAudioDeviceManager(WCMRAudioDeviceManagerClient *pTheClient, eAudioDeviceFilter eCurAudioDeviceFilter,
		bool useMultithreading = true, bool bNocopy = false); ///< constructor
	virtual ~WCMRCoreAudioDeviceManager(void); ///< Destructor

protected:
    static OSStatus HardwarePropertyChangeCallback (AudioHardwarePropertyID inPropertyID, void* inClientData);
    
    virtual WCMRAudioDevice*	initNewCurrentDeviceImpl(const std::string & deviceName);
	virtual void				destroyCurrentDeviceImpl();
	virtual WTErr				generateDeviceListImpl();
    virtual WTErr				updateDeviceListImpl();
    virtual WTErr               getDeviceSampleRatesImpl(const std::string & deviceName, std::vector<int>& sampleRates) const;
	virtual WTErr				getDeviceBufferSizesImpl(const std::string & deviceName, std::vector<int>& bufferSizes) const;
    
	bool m_UseMultithreading; ///< Flag indicates whether to use multi-threading for audio processing.
    bool m_bNoCopyAudioBuffer;
	    
private:
    // helper functions for this class only
    WTErr getDeviceAvailableSampleRates(DeviceID deviceId, std::vector<int>& sampleRates);
    WTErr getDeviceMaxInputChannels(DeviceID deviceId, unsigned int& inputChannels);
    WTErr getDeviceMaxOutputChannels(DeviceID deviceId, unsigned int& outputChannels);
    
    WCMRAudioDevice*			m_NoneDevice;
};

#endif //#ifndef __WCMRCoreAudioDeviceManager_h_
