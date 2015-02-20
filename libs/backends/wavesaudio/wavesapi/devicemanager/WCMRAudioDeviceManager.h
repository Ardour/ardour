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
#include "WCThreadSafe.h"

#define WCUNUSEDPARAM(a)

class WCMRAudioDevice;
class WCMRAudioDeviceManager;

typedef unsigned int DeviceID;

struct DeviceInfo
{
	DeviceID m_DeviceId;
	std::string m_DeviceName;
	std::vector<int> m_AvailableSampleRates;
	unsigned int m_MaxInputChannels;
	unsigned int m_MaxOutputChannels;
	unsigned int m_DefaultBufferSize;

    DeviceInfo():
    m_DeviceId(-1), m_DeviceName("Unknown"), m_MaxInputChannels(0), m_MaxOutputChannels(0)
	{};
    
	DeviceInfo(unsigned int deviceID, const std::string & deviceName):
		m_DeviceId(deviceID), m_DeviceName(deviceName), m_MaxInputChannels(0), m_MaxOutputChannels(0)
	{};
};

typedef std::vector<DeviceInfo*> DeviceInfoVec;
typedef DeviceInfoVec::iterator DeviceInfoVecIter;
typedef DeviceInfoVec::const_iterator DeviceInfoVecConstIter;

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
		DeviceStartsStreaming,
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
        int64_t acdSampleTime;
        int64_t acdCycleStartTimeNanos;
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
		DeviceErrors
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

	virtual WTErr ResetDevice ();

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

	virtual WTErr UpdateDeviceInfo () = 0;
    
protected:
	WCMRAudioDeviceManager *m_pMyManager; ///< The manager who's managing this device, can be used for sending notifications!
	
	std::string m_DeviceName; ///< Name of the device.
	std::vector<std::string> m_InputChannels; ///< List of input channel names.
	std::vector<std::string> m_OutputChannels; ///< List of output channel names.
	std::vector<int> m_SamplingRates; ///< List of available sampling rates.
	std::vector<int> m_BufferSizes; ///< List of available buffer sizes.
	int m_DefaultBufferSize; ///soundcard preferred buffer size
	
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


class WCMRAudioDeviceManager : public WCRefManager
{
public://< Public functions for the class.
  
	WCMRAudioDeviceManager(WCMRAudioDeviceManagerClient *pTheClient, eAudioDeviceFilter eCurAudioDeviceFilter); ///< constructor
	virtual ~WCMRAudioDeviceManager(void); ///< Destructor

	//interfaces
	WCMRAudioDevice*	InitNewCurrentDevice(const std::string & deviceName);
	void				DestroyCurrentDevice();
	const DeviceInfoVec DeviceInfoList () const;
    WTErr               GetDeviceInfoByName(const std::string & nameToMatch, DeviceInfo & devInfo) const;
    WTErr               GetDeviceSampleRates(const std::string & nameToMatch, std::vector<int>& sampleRates) const;
	WTErr				GetDeviceBufferSizes(const std::string & nameToMatch, std::vector<int>& bufferSizes) const;

    //virtual void		EnableVerboseLogging(bool /*bEnable*/, const std::string& /*logFilePath*/) { };

	//notify backend
	void					NotifyClient (WCMRAudioDeviceManagerClient::NotificationReason forReason, void *pParam = NULL);

protected:
    
    mutable wvNS::wvThread::ThreadMutex         m_AudioDeviceInfoVecMutex; // mutex to lock device info list
	DeviceInfoVec                               m_DeviceInfoVec;
	
    eAudioDeviceFilter                          m_eAudioDeviceFilter;
	WCMRAudioDevice*                            m_CurrentDevice;

private:
	// override in derived classes
	// made private to avoid pure virtual function call
	virtual WCMRAudioDevice*	initNewCurrentDeviceImpl(const std::string & deviceName) = 0;
	virtual void				destroyCurrentDeviceImpl() = 0;
    virtual WTErr				getDeviceSampleRatesImpl(const std::string & deviceName, std::vector<int>& sampleRates) const = 0;
	virtual WTErr				getDeviceBufferSizesImpl(const std::string & deviceName, std::vector<int>& bufferSizes) const = 0;
    virtual WTErr				generateDeviceListImpl() = 0;
    virtual WTErr				updateDeviceListImpl() = 0;
    
	WCMRAudioDeviceManagerClient	*m_pTheClient; ///< The device manager's client, used to send notifications.
};

#endif //#ifndef __WCMRAudioDeviceManager_h_
