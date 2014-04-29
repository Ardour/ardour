/*
    Copyright (C) 2013 Waves Audio Ltd.

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
//! \file	WCMRAudioDeviceManager.h
//!
//! WCMRAudioDeviceManager and related class declarations
//!
//---------------------------------------------------------------------------------*/
#ifndef __WCMRAudioDeviceManager_h_
	#define __WCMRAudioDeviceManager_h_

/* Copy to include
#include "WCMRAudioDeviceManager.h"
*/

#define AUTO_FUNC_DEBUG
#define DEBUG_MSG(a)
#define ASSERT_ERROR(a, b)
#define TRACE_MSG(a)

#include <string>
#include <vector>
#include <map>
#include "WCRefManager.h"
#include "BasicTypes/WUTypes.h"
#include "WUErrors.h"

#define WCUNUSEDPARAM(a)

//forward decl.
class WCMRAudioConnection;
class WCMRAudioDevice;
class WCMRAudioDeviceManager;

typedef std::vector<WCMRAudioDevice *> WCMRAudioDeviceList; ///< Vector for audio devices
typedef std::vector<WCMRAudioDevice *>::iterator WCMRAudioDeviceListIter; ///< Vector iterator for audio devices
typedef std::vector<WCMRAudioDevice *>::const_iterator WCMRAudioDeviceListConstIter; ///< Vector iterator for audio devices
typedef std::vector<WCMRAudioConnection *> WCMRAudioConnectionsList; ///< Vector for audio devices


/// for notification... A client must derive it's class from us.
class WCMRAudioDeviceManagerClient
{
	public:
	enum NotificationReason
	{
		DeviceListChanged,
		Dropout,
		RequestReset,
		RequestResync,
		SamplingRateChanged, //param has new SR, or -1 if not known
        SamplingRateChangedSilent, //To indicate sampling rate changed but no need to notify user
		BufferSizeChanged,
		ClockSourceChanged,
		DeviceStoppedStreaming,
		DeviceDroppedSamples,
		DeviceConnectionLost,
		DeviceGenericError,
		DeviceStatusChanged,
		DeviceStatisticsUpdated,
		DeviceDebugInfo, //param has c string
		DeviceProgressInfo, //param has c string
		MIDIData,
		MIDINodeUp,
		MIDINodeDown,
		DeviceSampleRateMisMatch,
		SystemSamplingRateChangedInfoOnly,
		LostClockSource,
		IODeviceDisconnected,
		ChannelCountModified,
		MasterUp,
		MasterDown,
		AudioDropFound,
		ReflasherEvent,
        AGDeviceSamplingRateChangedInfoOnly,
		IODeviceNameChanged,
        SetDisplayNameFromIOModule,
        IOMStateChanged,    ///< This is used when IOM state is changed.
        AudioCallback // VKamyshniy: param  is AudioCallbackDataData*
	};

	WCMRAudioDeviceManagerClient () {}
	virtual ~WCMRAudioDeviceManagerClient () {}

    // VKamyshniy: This is a structure to call the client's AudioDeviceManagerNotification
    // every AudioCallback time
    struct AudioCallbackData
    {
        const float *acdInputBuffer;
        float *acdOutputBuffer;
        size_t acdFrames;
        uint32_t acdSampleTime;
        uint64_t acdCycleStartTimeNanos;
    };

	virtual void AudioDeviceManagerNotification (NotificationReason WCUNUSEDPARAM(reason), void *WCUNUSEDPARAM(pParam)) {}
};


class WCMRAudioDevice : public WCRefManager
{
public:

	enum ConnectionStates
	{
		DeviceAvailable,
		DeviceDisconnected,
		DeviceError
	};

	WCMRAudioDevice (WCMRAudioDeviceManager *pManager);///<Constructor
	virtual ~WCMRAudioDevice ();///<Destructor

	virtual const std::string& DeviceName() const;///<Name?
	virtual const std::vector<std::string>& InputChannels();///<Current Input Channel List? - note that this may change with change in sampling rate.
	virtual const std::vector<std::string>& OutputChannels();///<Current Output Channel List? - note that this may change with change in sampling rate.

	virtual const std::vector<int>& SamplingRates();///<Supported Sampling Rate List?
	virtual int CurrentSamplingRate(); ///<Current Sampling rate.?
	virtual WTErr SetCurrentSamplingRate(int newRate);///<Change Current Sampling Rate : This is a requset, might not be successful at run time!

	virtual const std::vector<int>& BufferSizes();///<Supported Buffer Size List? - note that this may change with change in sampling rate.
	virtual int CurrentBufferSize();///<Current Buffer Size.? - note that this may change with change in sampling rate.
	virtual WTErr SetCurrentBufferSize (int newSize);///<Change Current Buffer Size : This is a requset, might not be successful at run time!

    virtual int CurrentBlockSize();

	virtual ConnectionStates ConnectionStatus();///< Connection Status - device available, gone, disconnected

	virtual bool Active();///<Active status - mainly significant for ASIO, as certain ops can only be performed on active devices!
	virtual WTErr SetActive (bool newState);///<Prepare/Activate device.
	
	virtual bool Streaming();///<Streaming Status?
	virtual WTErr SetStreaming (bool newState);///<Start/Stop Streaming - should reconnect connections when streaming starts!

    virtual bool IsProcessActive();
	
	virtual WTErr DoIdle();///<Do Idle Processing
	
	virtual const std::vector<float>& InputLevels();///<Retrieve Input Levels (for VU display)?
	
	virtual const std::vector<float>& OutputLevels();///<Retrieve Output Levels (for VU display)?

	void GetMonitorInfo (int *pLeftChannel = NULL, int *pRightChannel = NULL, float *pGain = NULL);///<Retrieve current monitor channel pair and gain - optional, will not be available with AG
	virtual WTErr SetMonitorChannels (int leftChannel, int rightChannel);///<Set monitor channels. - optional, will not be available with AG
	virtual WTErr SetMonitorGain (float newGain);///<Set monitor gain. - optional, will not be available with AG
	
	virtual WTErr ShowConfigPanel (void *pParam);///< Show Control Panel - in case of ASIO this will work only with Active device!
	virtual WTErr SendCustomCommand (int customCommand, void *pCommandParam); ///< Send a custom command to the audiodevice...
    
    virtual uint32_t GetLatency (bool isInput); ///Get latency.
    
protected:
	WCMRAudioDeviceManager *m_pMyManager; ///< The manager who's managing this device, can be used for sending notifications!
	
	std::string m_DeviceName; ///< Name of the device.
	std::vector<std::string> m_InputChannels; ///< List of input channel names.
	std::vector<std::string> m_OutputChannels; ///< List of output channel names.
	std::vector<int> m_SamplingRates; ///< List of available sampling rates.
	std::vector<int> m_BufferSizes; ///< List of available buffer sizes.
	
	int m_CurrentSamplingRate; ///< Currently selected sampling rate.
	int m_CurrentBufferSize; ///< Currently selected buffer size.

	ConnectionStates m_ConnectionStatus; ///< Status of device connection
	bool m_IsActive; ///< Flag for teh active status.
	bool m_IsStreaming; ///< Flag for streaming status.
	std::vector<float> m_InputLevels; ///< List of input levels.
	std::vector<float> m_OutputLevels; ///< List of output levels.
	
	int m_LeftMonitorChannel; ///< The device channel to use for monitoring left channel data.
	int m_RightMonitorChannel; ///< The device channel to use for monitoring right channel data.
	float m_MonitorGain; ///< Amount of gain to apply for monitoring signal.
};

// This enum is for choosing filter for audio devices scan
typedef enum eAudioDeviceFilter
{
	eAllDevices = 0,        // Choose all audio devices
	eInputOnlyDevices,      // Choose only input audio devices
	eOutputOnlyDevices,     // Choose only output audio devices
	eFullDuplexDevices,     // Choose audio devices that have both input and output channels on the same device
	eMatchedDuplexDevices,  // Match(aggregate) audio devices that have both input and output channels but are considered different audio devices (For mac)
	eAudioDeviceFilterNum   // Number of enums
}	eAudioDeviceFilter;

//! WCMRAudioDeviceManager
/*! The Audio Device Manager class */
class WCMRAudioDeviceManager : public WCRefManager
{
private://< Private version of class functions which will be called by class's public function after mutex lock acquistion.
    WCMRAudioDevice* GetDefaultDevice_Private();
    WTErr DoIdle_Private();
    const WCMRAudioDeviceList& Devices_Private() const;	
    WCMRAudioDevice* GetDeviceByName_Private(const std::string & nameToMatch) const;

public://< Public functions for the class.
    WCMRAudioDevice* GetDefaultDevice()
    {
		//wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceManagerMutex);
        return GetDefaultDevice_Private();
    }

    virtual WTErr DoIdle()
    {
        //wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceManagerMutex);
        return DoIdle_Private();
    }

    const WCMRAudioDeviceList& Devices() const
    {
        //wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceManagerMutex);
        return Devices_Private();
    }

    WCMRAudioDevice* GetDeviceByName(const std::string & nameToMatch) const
    {
        //wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceManagerMutex);
        return GetDeviceByName_Private(nameToMatch);
    }

public:
	
	WCMRAudioDeviceManager(WCMRAudioDeviceManagerClient *pTheClient, eAudioDeviceFilter eCurAudioDeviceFilter
		); ///< constructor
	virtual ~WCMRAudioDeviceManager(void); ///< Destructor	
    
	virtual WTErr UpdateDeviceList () = 0; //has to be overridden!
	
	
	//This is primarily for use by WCMRAudioDevice and it's descendants... We could have made it
	//protected and made WCMRAudioDevice a friend, and then in some way found a way to extend 
	//the friendship to WCMRAudioDevice's descendants, but that would require a lot of extra
	//effort!
	void NotifyClient (WCMRAudioDeviceManagerClient::NotificationReason forReason, void *pParam = NULL);
    virtual void EnableVerboseLogging(bool /*bEnable*/, const std::string& /*logFilePath*/) { };

protected:
    
    //< NOTE : Mutex protection is commented, but wrapper classes are still there, in case they are required in future.
    //wvNS::wvThread::ThreadMutex   m_AudioDeviceManagerMutex;   ///< Mutex for Audio device manager class function access.
	WCMRAudioDeviceManagerClient *m_pTheClient; ///< The device manager's client, used to send notifications.
	
	WCMRAudioDeviceList m_Devices; ///< List of all relevant devices devices
	eAudioDeviceFilter m_eAudioDeviceFilter; // filter of 'm_Devices'
};

#endif //#ifndef __WCMRAudioDeviceManager_h_
