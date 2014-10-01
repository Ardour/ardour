//----------------------------------------------------------------------------------
//
// Copyright (c) 2008 Waves Audio Ltd. All rights reserved.
//
//! \file	WCMRAudioDeviceManager.cpp
//!
//! WCMRAudioDeviceManager and related class declarations
//!
//---------------------------------------------------------------------------------*/
#include <iostream>
#include "WCMRAudioDeviceManager.h"


//**********************************************************************************************
// WCMRAudioDevice::WCMRAudioDevice 
//
//! Constructor for the audio device. The derived classes will need to do more actual work, such
//!		as determining supported sampling rates, buffer sizes, and channel counts. Connection
//!		and streaming will also be provided by the derived implementations.
//!
//! \param *pManager : The audio device manager that's managing this device.
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRAudioDevice::WCMRAudioDevice (WCMRAudioDeviceManager *pManager) :
	m_pMyManager (pManager)
	, m_ConnectionStatus (DeviceDisconnected)
	, m_IsActive (false)
	, m_IsStreaming (false)
	, m_CurrentSamplingRate (-1)
	, m_CurrentBufferSize (0)
	, m_LeftMonitorChannel (-1)
	, m_RightMonitorChannel (-1)
	, m_MonitorGain (1.0f)
{
	m_DeviceName = "Unknown";
}



//**********************************************************************************************
// WCMRAudioDevice::~WCMRAudioDevice 
//
//! Destructor for the audio device. It release all the connections that were created.
//!
//! \param none
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRAudioDevice::~WCMRAudioDevice ()
{
    AUTO_FUNC_DEBUG;
	try 
	{
	}
	catch (...)
	{
		//destructors should absorb exceptions, no harm in logging though!!
		DEBUG_MSG ("Exception during destructor");
	}
}




//**********************************************************************************************
// WCMRAudioDevice::DeviceName 
//
//! Retrieves Device's name.
//!
//! \param none
//! 
//! \return The device name.
//! 
//**********************************************************************************************
const std::string& WCMRAudioDevice::DeviceName () const
{
	return (m_DeviceName);
	
}



//**********************************************************************************************
// WCMRAudioDevice::InputChannels 
//
//! Retrieves Input Channel information. Note that the list may be changed at run-time.
//!
//! \param none
//! 
//! \return A vector with Input Channel Names.
//! 
//**********************************************************************************************
const std::vector<std::string>& WCMRAudioDevice::InputChannels ()
{
	return (m_InputChannels);
	
}



//**********************************************************************************************
// WCMRAudioDevice::OutputChannels 
//
//! Retrieves Output Channel Information. Note that the list may be changed at run-time.
//!
//! \param none
//! 
//! \return A vector with Output Channel Names.
//! 
//**********************************************************************************************
const std::vector<std::string>& WCMRAudioDevice::OutputChannels ()
{
	return (m_OutputChannels);
}




//**********************************************************************************************
// WCMRAudioDevice::SamplingRates 
//
//! Retrieves supported sampling rate information.
//!
//! \param none
//! 
//! \return A vector with supported sampling rates.
//! 
//**********************************************************************************************
const std::vector<int>& WCMRAudioDevice::SamplingRates ()
{
	return (m_SamplingRates);
}



//**********************************************************************************************
// WCMRAudioDevice::CurrentSamplingRate 
//
//! The device's current sampling rate. This may be overridden, if the device needs to 
//!		query the driver for the current rate.
//!
//! \param none
//! 
//! \return The device's current sampling rate. -1 on error.
//! 
//**********************************************************************************************
int WCMRAudioDevice::CurrentSamplingRate ()
{
	return (m_CurrentSamplingRate);
}




//**********************************************************************************************
// WCMRAudioDevice::SetCurrentSamplingRate 
//
//! Change the sampling rate to be used by the device. This will most likely be overridden, 
//!		the base class simply updates the member variable.
//!
//! \param newRate : The rate to use (samples per sec).
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SetCurrentSamplingRate (int newRate)
{
	//changes the status.
	m_CurrentSamplingRate = newRate;
	return (eNoErr);
}




//**********************************************************************************************
// WCMRAudioDevice::BufferSizes 
//
//! Retrieves supported buffer size information.
//!
//! \param none
//! 
//! \return A vector with supported buffer sizes.
//! 
//**********************************************************************************************
const std::vector<int>& WCMRAudioDevice::BufferSizes ()
{
	return (m_BufferSizes);
}



//**********************************************************************************************
// WCMRAudioDevice::CurrentBufferSize
//
//! The device's current buffer size in use. This may be overridden, if the device needs to 
//!		query the driver for the current size.
//!
//! \param none
//! 
//! \return The device's current buffer size. 0 on error.
//! 
//**********************************************************************************************
int WCMRAudioDevice::CurrentBufferSize ()
{
	return (m_CurrentBufferSize);
}

//**********************************************************************************************
// WCMRAudioDevice::CurrentBlockSize
//
//! Device's block size we use for holding the audio samples.
//! Usually this is equal to the buffer size, but in some cases the buffer size holds additional
//!   data other then the audio buffers, like frames info in SG, so it can be overriden
//!
//! \param none
//! 
//! \return The device's current block size. 0 on error.
//! 
//**********************************************************************************************
int WCMRAudioDevice::CurrentBlockSize()
{
    // By default - return the buffer size
    return CurrentBufferSize();
}


//**********************************************************************************************
// WCMRAudioDevice::SetCurrentBufferSize
//
//! Change the buffer size to be used by the device. This will most likely be overridden, 
//!		the base class simply updates the member variable.
//!
//! \param newSize : The buffer size to use (in sample-frames)
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SetCurrentBufferSize (int newSize)
{
	//This will most likely be overridden, the base class simply
	//changes the member.
	m_CurrentBufferSize = newSize;
	return (eNoErr);
}




//**********************************************************************************************
// WCMRAudioDevice::ConnectionStatus 
//
//! Retrieves the device's current connection status. This will most likely be overridden,
//!		in case some driver communication is required to query the status.
//!
//! \param none
//! 
//! \return A ConnectionStates value.
//! 
//**********************************************************************************************
WCMRAudioDevice::ConnectionStates WCMRAudioDevice::ConnectionStatus ()
{
	return (m_ConnectionStatus);
	
}




//**********************************************************************************************
// WCMRAudioDevice::Active 
//
//! Retrieves Device activation status.
//!
//! \param none
//! 
//! \return true if device is active, false otherwise.
//! 
//**********************************************************************************************
bool WCMRAudioDevice::Active ()
{
	return (m_IsActive);
	
}



//**********************************************************************************************
// WCMRAudioDevice::SetActive 
//
//! Sets the device's activation status.
//!
//! \param newState : Should be true to activate, false to deactivate. This roughly corresponds
//!		to opening and closing the device handle/stream/audio unit.
//! 
//! \return eNoErr always, the derived classes may return appropriate error code.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SetActive (bool newState)
{
	//This will most likely be overridden, the base class simply
	//changes the member.
	m_IsActive = newState;
	return (eNoErr);
}




//**********************************************************************************************
// WCMRAudioDevice::Streaming 
//
//! Retrieves Device streaming status.
//!
//! \param none
//! 
//! \return true if device is streaming, false otherwise.
//! 
//**********************************************************************************************
bool WCMRAudioDevice::Streaming ()
{
	return (m_IsStreaming);
}



//**********************************************************************************************
// WCMRAudioDevice::SetStreaming
//
//! Sets the device's streaming status.
//!
//! \param newState : Should be true to start streaming, false to stop streaming. This roughly
//!		corresponds to calling Start/Stop on the lower level interface.
//! 
//! \return eNoErr always, the derived classes may return appropriate error code.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SetStreaming (bool newState)
{
	//This will most likely be overridden, the base class simply
	//changes the member.
	m_IsStreaming = newState;
	return (eNoErr);
}


WTErr WCMRAudioDevice::ResetDevice ()
{
	// Keep device sates
	bool wasStreaming = Streaming();
	bool wasActive = Active();

	WTErr err = SetStreaming(false);
	
	if (err == eNoErr)
		SetActive(false);

	if (err == eNoErr && wasActive)
		SetActive(true);

	if (err == eNoErr && wasStreaming)
		SetStreaming(true);

	return err;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
// IsProcessActive - returns true if process code is running.
// A normal audio device should return the Streaming() value
///////////////////////////////////////////////////////////////////////////////////////////////////////
bool WCMRAudioDevice::IsProcessActive()
{
    return Streaming();
}





//**********************************************************************************************
// WCMRAudioDevice::DoIdle 
//
//! A place for doing idle time processing. The derived classes will probably do something
//!		meaningful.
//!
//! \param none
//! 
//! \return eNoErr always.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::DoIdle ()
{
	//We don't need to do anything here...
	//the derived classes may want to use this however.
	return (eNoErr);
}




//**********************************************************************************************
// WCMRAudioDevice::InputLevels 
//
//! Retrieve current input levels.
//!
//! \param none
//! 
//! \return A vector (the same size as input channels list) that contains current input levels.
//! 
//**********************************************************************************************
const std::vector<float>& WCMRAudioDevice::InputLevels ()
{
	//The derived classes may override if they need to query
	//the driver for the levels.
	return (m_InputLevels);
}



//**********************************************************************************************
// WCMRAudioDevice::OutputLevels 
//
//! Retrieve current output levels.
//!
//! \param none
//! 
//! \return A vector (the same size as output channels list) that contains current output levels.
//! 
//**********************************************************************************************
const std::vector<float>& WCMRAudioDevice::OutputLevels ()
{
	//The derived classes may override if they need to query
	//the driver for the levels.
	return (m_OutputLevels);
}



//**********************************************************************************************
// WCMRAudioDevice::GetMonitorInfo 
//
//! Retrieves current monitoring information.
//!
//! \param *pLeftChannel : Pointer to receive left monitor channel index.
//! \param *pRightChannel : Pointer to receive right monitor channel index.
//! \param *pGain : Pointer to receive the gain (linear) to be applied.
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
void WCMRAudioDevice::GetMonitorInfo (int *pLeftChannel, int *pRightChannel, float *pGain)
{
	if (pLeftChannel)
		*pLeftChannel = m_LeftMonitorChannel;
	if (pRightChannel)	
		*pRightChannel = m_RightMonitorChannel;
	if (pGain)	
		*pGain = m_MonitorGain;
	return;	
}



//**********************************************************************************************
// WCMRAudioDevice::SetMonitorChannels 
//
//! Used to set the channels to be used for monitoring.
//!
//! \param leftChannel : Left monitor channel index.
//! \param rightChannel : Right monitor channel index.
//! 
//! \return eNoErr always, the derived classes may return appropriate errors.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SetMonitorChannels (int leftChannel, int rightChannel)
{
	//This will most likely be overridden, the base class simply
	//changes the member.
	m_LeftMonitorChannel = leftChannel;
	m_RightMonitorChannel = rightChannel;
	return (eNoErr);
}



//**********************************************************************************************
// WCMRAudioDevice::SetMonitorGain 
//
//! Used to set monitor gain (or atten).
//!
//! \param newGain : The new gain or atten. value to use. Specified as a linear multiplier (not dB) 
//! 
//! \return eNoErr always, the derived classes may return appropriate errors.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SetMonitorGain (float newGain)
{
	//This will most likely be overridden, the base class simply
	//changes the member.
	m_MonitorGain = newGain;
	return (eNoErr);
}




//**********************************************************************************************
// WCMRAudioDevice::ShowConfigPanel 
//
//! Used to show device specific config/control panel. Some interfaces may not support it.
//!		Some interfaces may require the device to be active before it can display a panel.
//!
//! \param pParam : A device/interface specific parameter - optional.
//! 
//! \return eNoErr always, the derived classes may return errors.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::ShowConfigPanel (void *WCUNUSEDPARAM(pParam))
{
	//This will most likely be overridden...
	return (eNoErr);
}


//**********************************************************************************************
// WCMRAudioDevice::SendCustomCommand 
//
//! Used to Send a custom command to the audiodevice. Some interfaces may require the device 
//!		to be active before it can do anything in this.
//!
//! \param customCommand : A device/interface specific command.
//! \param pCommandParam : A device/interface/command specific parameter - optional.
//! 
//! \return eNoErr always, the derived classes may return errors.
//! 
//**********************************************************************************************
WTErr WCMRAudioDevice::SendCustomCommand (int WCUNUSEDPARAM(customCommand), void *WCUNUSEDPARAM(pCommandParam))
{
	//This will most likely be overridden...
	return (eNoErr);
}

//**********************************************************************************************
// WCMRAudioDevice::GetLatency
//
//! Get Latency for device.
//!
//! Use 'kAudioDevicePropertyLatency' and 'kAudioDevicePropertySafetyOffset' + GetStreamLatencies
//!
//! \param isInput : Return latency for the input if isInput is true, otherwise the output latency
//!                  wiil be returned.
//! \return Latency in samples.
//!
//**********************************************************************************************
uint32_t WCMRAudioDevice::GetLatency (bool isInput)
{
    //This will most likely be overridden...
    return 0;
}


//**********************************************************************************************
// WCMRAudioDeviceManager::WCMRAudioDeviceManager
//
//! The constructuor, most of the work will be done in the derived class' constructor.
//!
//! \param *pTheClient : 
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRAudioDeviceManager::WCMRAudioDeviceManager(WCMRAudioDeviceManagerClient *pTheClient, eAudioDeviceFilter eCurAudioDeviceFilter)
    : m_eAudioDeviceFilter(eCurAudioDeviceFilter)
    , m_CurrentDevice(0)
    , m_pTheClient (pTheClient)
{
}


//**********************************************************************************************
// WCMRAudioDeviceManager::~WCMRAudioDeviceManager
//
//! It clears the device list, releasing each of the device.
//!
//! \param none
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRAudioDeviceManager::~WCMRAudioDeviceManager()
{
    AUTO_FUNC_DEBUG;

	std::cout << "API::Destroying AudioDeviceManager " << std::endl;
	try
	{
		// clean up device info list
        {
            wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceInfoVecMutex);
            while( m_DeviceInfoVec.size() )
            {
                DeviceInfo* devInfo = m_DeviceInfoVec.back();
                m_DeviceInfoVec.pop_back();
                delete devInfo;
            }
        }
		delete m_CurrentDevice;

	}
	catch (...)
	{
		//destructors should absorb exceptions, no harm in logging though!!
		DEBUG_MSG ("Exception during destructor");
	}
}


WCMRAudioDevice* WCMRAudioDeviceManager::InitNewCurrentDevice(const std::string & deviceName)
{
	return initNewCurrentDeviceImpl(deviceName);
}


void WCMRAudioDeviceManager::DestroyCurrentDevice()
{
	return destroyCurrentDeviceImpl();
}


const DeviceInfoVec WCMRAudioDeviceManager::DeviceInfoList() const
{
    wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceInfoVecMutex);
	return m_DeviceInfoVec;
}


WTErr WCMRAudioDeviceManager::GetDeviceInfoByName(const std::string & nameToMatch, DeviceInfo & devInfo) const
{
    wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceInfoVecMutex);
	DeviceInfoVecConstIter iter = m_DeviceInfoVec.begin();
	for (; iter != m_DeviceInfoVec.end(); ++iter)
	{
		if (nameToMatch == (*iter)->m_DeviceName)
        {
			devInfo = *(*iter);
            return eNoErr;
        }
	}

	return eRMResNotFound;
}


WTErr WCMRAudioDeviceManager::GetDeviceSampleRates(const std::string & nameToMatch, std::vector<int>& sampleRates) const
{
	return getDeviceSampleRatesImpl(nameToMatch, sampleRates);
}



WTErr WCMRAudioDeviceManager::GetDeviceBufferSizes(const std::string & nameToMatch, std::vector<int>& bufferSizes) const
{
	return getDeviceBufferSizesImpl(nameToMatch, bufferSizes);
}


//**********************************************************************************************
// WCMRAudioDeviceManager::NotifyClient 
//
//! A helper routine used to call the client for notification.
//!
//! \param forReason : The reason for notification.
//! \param *pParam : A parameter (if required) for notification.
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
void WCMRAudioDeviceManager::NotifyClient (WCMRAudioDeviceManagerClient::NotificationReason forReason, void *pParam)
{
	if (m_pTheClient)
		m_pTheClient->AudioDeviceManagerNotification (forReason, pParam);
	return;	
}
