//----------------------------------------------------------------------------------
//
// Copyright (c) 2008 Waves Audio Ltd. All rights reserved.
//
//! \file	WCMRPortAudioDeviceManager.cpp
//!
//! WCMRPortAudioDeviceManager and related class declarations
//!
//---------------------------------------------------------------------------------*/
#include "WCMRPortAudioDeviceManager.h"
#include "MiscUtils/safe_delete.h"
#include "UMicroseconds.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <list>
using namespace wvNS;
#include "IncludeWindows.h"
#include <mmsystem.h>
#include "pa_asio.h"
#include "asio.h"

#define PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS 200
#define DEVICE_INFO_UPDATE_SLEEP_TIME_MILLISECONDS 500
#define PROPERTY_CHANGE_TIMEOUT_SECONDS 2
#define PROPERTY_CHANGE_RETRIES 3

///< Supported Sample rates                                                                                                  
static const double gAllSampleRates[] =
	{
		44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, -1 /* negative terminated  list */
	};



///< Default Supported Buffer Sizes.
static const int gAllBufferSizes[] =
	{
		32, 64, 96, 128, 192, 256, 512, 1024, 2048
	};
	

///< The default SR.
static const int DEFAULT_SR = 44100;
///< The default buffer size.
static const int DEFAULT_BUFFERSIZE = 128;

static const int NONE_DEVICE_ID = -1;

///< Number of stalls to wait before notifying user...
static const int NUM_STALLS_FOR_NOTIFICATION = 100; // 100 corresponds to 100 x 42 ms idle timer - about 4 seconds.
static const int CHANGE_CHECK_COUNTER_PERIOD = 100; // 120 corresponds to 120 x 42 ms idle timer - about 4 seconds.
	
#define HUNDRED_NANO_TO_MILLI_CONSTANT 10000
#define CONSUMPTION_CALCULATION_INTERVAL 500 // Milli Seconds


// This wrapper is used to adapt device DoIdle method as entry point for MS thread
DWORD WINAPI WCMRPortAudioDevice::__DoIdle__(LPVOID lpThreadParameter)
{
	WCMRPortAudioDevice* pDevice = (WCMRPortAudioDevice*)lpThreadParameter;
	pDevice->DoIdle();
	return 0;
}

//**********************************************************************************************
// WCMRPortAudioDevice::WCMRPortAudioDevice 
//
//! Constructor for the audio device. Opens the PA device
//! and gets information about the device.
//! Starts the thread which will process requests to this device
//!	such as determining supported sampling rates, buffer sizes, and channel counts.
//!
//! \param *pManager	: The audio device manager that's managing this device.
//! \param deviceID		: The port audio device ID.
//! \param useMultithreading : Whether to use multi-threading for audio processing. Default is true.
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRPortAudioDevice::WCMRPortAudioDevice (WCMRPortAudioDeviceManager *pManager, unsigned int deviceID, bool useMultithreading, bool bNoCopy) :
	WCMRNativeAudioDevice (pManager, useMultithreading, bNoCopy)
	, m_SampleCounter(0)
	, m_BufferSizeChangeRequested (0)
	, m_BufferSizeChangeReported (0)
	, m_ResetRequested (0)
	, m_ResetReported (0)
	, m_ResyncRequested (0)
	, m_ResyncReported (0)
	, m_DropsDetected(0)
	, m_DropsReported(0)
	, m_IgnoreThisDrop(true)
	, m_hDeviceProcessingThread(NULL)
	, m_DeviceProcessingThreadID(0)
	, m_hUpdateDeviceInfoRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hUpdateDeviceInfoDone(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hActivateRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hActivationDone(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hDeActivateRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hDeActivationDone(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hStartStreamingRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hStartStreamingDone(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hStopStreamingRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hStopStreamingDone(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hResetRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hResetDone(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hResetFromDevRequestedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hBufferSizeChangedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hSampleRateChangedEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hExitIdleThread(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_hDeviceInitialized(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_lastErr(eNoErr)
{
    AUTO_FUNC_DEBUG;

	//Set initial device info...
	m_DeviceID = deviceID;
	m_PortAudioStream = NULL;
	m_CurrentSamplingRate = DEFAULT_SR;
	m_CurrentBufferSize = DEFAULT_BUFFERSIZE;
	m_DefaultBufferSize = DEFAULT_BUFFERSIZE;
	m_StopRequested = true;
	m_pInputData = NULL;

	//initialize device processing thread
	//the divice become alive and now is able to process requests
	m_hDeviceProcessingThread = CreateThread( NULL, 0, __DoIdle__, (LPVOID)this, 0, &m_DeviceProcessingThreadID );

	if (!m_hDeviceProcessingThread)
	{
		DEBUG_MSG("API::Device " << m_DeviceName << " cannot create processing thread");
		throw eGenericErr;
	}

	WaitForSingleObject(m_hDeviceInitialized, INFINITE);

	if (ConnectionStatus() == DeviceErrors)
	{
		throw m_lastErr;
	}
}


void WCMRPortAudioDevice::initDevice()
{
	// Initialize COM for this thread
	std::cout << "API::Device " << m_DeviceID << " initializing COM" << std::endl;

	if (S_OK == CoInitialize(NULL) )
	{
		// Initialize PA
		Pa_Initialize();

		updateDeviceInfo();

		//should use a valid current SR...
		if (m_SamplingRates.size())
		{
			//see if the current sr is present in the sr list, if not, use the first one!
			std::vector<int>::iterator intIter = find(m_SamplingRates.begin(), m_SamplingRates.end(), m_CurrentSamplingRate);
			if (intIter == m_SamplingRates.end())
			{
				//not found... use the first one
				m_CurrentSamplingRate = m_SamplingRates[0];
			}
		}
		else
			std::cout << "API::Device " << m_DeviceName << " Device does not support any sample rate of ours" << std::endl;
	
		//should use a valid current buffer size
		if (m_BufferSizes.size())
		{
			//see if the current sr is present in the buffersize list, if not, use the first one!
			std::vector<int>::iterator intIter = find(m_BufferSizes.begin(), m_BufferSizes.end(), m_CurrentBufferSize);
			if (intIter == m_BufferSizes.end())
			{
				//not found... use the first one
				m_CurrentBufferSize = m_BufferSizes[0];
			}
		}
	
		//build our input/output level lists
		for (unsigned int currentChannel = 0; currentChannel < m_InputChannels.size(); currentChannel++)
		{
			m_InputLevels.push_back (0.0);
		}

		//build our input/output level lists
		for (unsigned int currentChannel = 0; currentChannel < m_OutputChannels.size(); currentChannel++)
		{
			m_OutputLevels.push_back (0.0);
		}

		std::cout << "API::Device " << m_DeviceName << " Device has been initialized" << std::endl;
		m_ConnectionStatus = DeviceDisconnected;
		m_lastErr = eNoErr;
	}
	else
	{
		/*Replace with debug trace*/std::cout << "API::Device " << m_DeviceName << " cannot initialize COM" << std::endl;
		DEBUG_MSG("Device " << m_DeviceName << " cannot initialize COM");
		m_ConnectionStatus = DeviceErrors;
		m_lastErr = eSomeThingNotInitailzed;
		SetEvent(m_hExitIdleThread);
	}

	SetEvent(m_hDeviceInitialized);
}

void WCMRPortAudioDevice::terminateDevice()
{
	std::cout << "API::Device " << m_DeviceName << " Terminating DEVICE" << std::endl;

	//If device is streaming, need to stop it!
	if (Streaming())
	{
		stopStreaming();
	}
		
	//If device is active (meaning stream is open) we need to close it.
	if (Active())
	{
		deactivateDevice();
	}

	std::cout << "API::Device " << m_DeviceName << " Terminating PA" << std::endl;

	//Deinitialize PA
	Pa_Terminate();
}


//**********************************************************************************************
// WCMRPortAudioDevice::~WCMRPortAudioDevice 
//
//! Destructor for the audio device. The base release all the connections that were created, if
//!		they have not been already destroyed! Here we simply stop streaming, and close device
//!		handles if necessary.
//!
//! \param none
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRPortAudioDevice::~WCMRPortAudioDevice ()
{
    AUTO_FUNC_DEBUG;

	std::cout << "API::Destroying Device Instance: " << DeviceName() << std::endl;
	try
	{
		//Stop deviceprocessing thread
		SignalObjectAndWait(m_hExitIdleThread, m_hDeviceProcessingThread, INFINITE, false);

		std::cout << "API::Device " << m_DeviceName << " Processing Thread is stopped" << std::endl;

		CloseHandle(m_hDeviceProcessingThread);

		//Now it's safe to free all event handlers
		CloseHandle(m_hUpdateDeviceInfoRequestedEvent);
		CloseHandle(m_hUpdateDeviceInfoDone);
		CloseHandle(m_hActivateRequestedEvent);
		CloseHandle(m_hActivationDone);
		CloseHandle(m_hDeActivateRequestedEvent);
		CloseHandle(m_hDeActivationDone);
		CloseHandle(m_hStartStreamingRequestedEvent);
		CloseHandle(m_hStartStreamingDone);
		CloseHandle(m_hStopStreamingRequestedEvent);
		CloseHandle(m_hStopStreamingDone);
		CloseHandle(m_hResetRequestedEvent);
		CloseHandle(m_hResetDone);
		CloseHandle(m_hResetFromDevRequestedEvent);
		CloseHandle(m_hBufferSizeChangedEvent);
		CloseHandle(m_hSampleRateChangedEvent);
		CloseHandle(m_hExitIdleThread);
		CloseHandle(m_hDeviceInitialized);
	}
	catch (...)
	{
		//destructors should absorb exceptions, no harm in logging though!!
		DEBUG_MSG ("Exception during destructor");
	}
}


WTErr WCMRPortAudioDevice::UpdateDeviceInfo ()
{
	std::cout << "API::Device (ID:)" << m_DeviceID << " Updating device info" << std::endl;
	
	SignalObjectAndWait(m_hUpdateDeviceInfoRequestedEvent, m_hUpdateDeviceInfoDone, INFINITE, false);

	return eNoErr;
}


//**********************************************************************************************
// WCMRPortAudioDevice::updateDeviceInfo 
//
//! Must be called be device processing thread
//! Updates Device Information about channels, sampling rates, buffer sizes.
//!
//! \return Nothing.
//! 
//**********************************************************************************************
void WCMRPortAudioDevice::updateDeviceInfo (bool callerIsWaiting/*=false*/)
{
    AUTO_FUNC_DEBUG;

	//get device info
	const PaDeviceInfo *pDeviceInfo = Pa_GetDeviceInfo(m_DeviceID);
	
	//update name.
	m_DeviceName = pDeviceInfo->name;

	//following parameters are needed opening test stream and for sample rates validation
	PaStreamParameters inputParameters, outputParameters;
	PaStreamParameters *pInS = NULL, *pOutS = NULL;

	inputParameters.device = m_DeviceID;
	inputParameters.channelCount = pDeviceInfo->maxInputChannels;
	inputParameters.sampleFormat = paFloat32 | paNonInterleaved;
	inputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
	inputParameters.hostApiSpecificStreamInfo = 0;

	if (inputParameters.channelCount)
		pInS = &inputParameters;

	outputParameters.device = m_DeviceID;
	outputParameters.channelCount = pDeviceInfo->maxOutputChannels;
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
	outputParameters.hostApiSpecificStreamInfo = 0;

	if (outputParameters.channelCount)
		pOutS = &outputParameters;

	////////////////////////////////////////////////////////////////////////////////////
	//update list of supported SRs...
	m_SamplingRates.clear();
		
	// now iterate through our standard SRs and check if they are supported by device
	// store them for this device
	for(int sr=0; gAllSampleRates[sr] > 0; sr++)
	{
		PaError err = Pa_IsFormatSupported(pInS, pOutS, gAllSampleRates[sr]);
		if( err == paFormatIsSupported)
		{
			m_SamplingRates.push_back ((int)gAllSampleRates[sr]);
		}
	}

	///////////////////////////////////////////////////////////////////////////////////
	//update buffer sizes
	m_BufferSizes.clear();
	bool useDefaultBuffers = true;

	// In ASIO Windows, the buffer size is set from the sound device manufacturer's control panel 
	long minSize, maxSize, preferredSize, granularity;
	PaError err = PaAsio_GetAvailableBufferSizes(m_DeviceID, &minSize, &maxSize, &preferredSize, &granularity);
	
	if (err == paNoError)
	{
		std::cout << "API::Device " << m_DeviceName << " Buffers: " << minSize << " " << maxSize << " " << preferredSize << std::endl;
			
		m_DefaultBufferSize = preferredSize;
		m_BufferSizes.push_back (preferredSize);
		useDefaultBuffers = false;
	}
	else
	{
		std::cout << "API::Device" << m_DeviceName << " Preffered buffer size is not supported" << std::endl;
	}
	
	if (useDefaultBuffers)
	{
		std::cout << "API::Device" << m_DeviceName << " Using default buffer sizes " <<std::endl;
		for(int bsize=0; bsize < (sizeof(gAllBufferSizes)/sizeof(gAllBufferSizes[0])); bsize++)
			m_BufferSizes.push_back (gAllBufferSizes[bsize]);
	}

	/////////////////////////////////////////////////////////////////////////////////////////
	//update channels info
	{
		int maxInputChannels = pDeviceInfo->maxInputChannels;
		int maxOutputChannels = pDeviceInfo->maxOutputChannels;

		//Update input channels
		m_InputChannels.clear();
		for (int channel = 0; channel < maxInputChannels; channel++)
		{
			const char* channelName[32]; // 32 is max leth declared by PortAudio for this operation
			std::stringstream chNameStream;

			PaError error = PaAsio_GetInputChannelName(m_DeviceID, channel, channelName);

			chNameStream << (channel+1) << " - ";

			if (error == paNoError)
			{
				chNameStream << *channelName;
			}
			else
			{
				chNameStream << "Input " << (channel+1);
			}

			m_InputChannels.push_back (chNameStream.str());
		}
	
	
		//Update output channels
		m_OutputChannels.clear();
		for (int channel = 0; channel < maxOutputChannels; channel++)
		{
			const char* channelName[32]; // 32 is max leth declared by PortAudio for this operation
			std::stringstream chNameStream;
			
			PaError error = PaAsio_GetOutputChannelName(m_DeviceID, channel, channelName);
			
			chNameStream << (channel+1) << " - ";

			if (error == paNoError)
			{
				chNameStream << *channelName;
			}
			else
			{
				chNameStream << "Output " << (channel+1);
			}
			
			m_OutputChannels.push_back (chNameStream.str());
		}
	}

	std::cout << "API::Device" << m_DeviceName << " Device info update has been finished" << std::endl;

	if (callerIsWaiting)
		SetEvent(m_hUpdateDeviceInfoDone);
}


PaError WCMRPortAudioDevice::testStateValidness(int sampleRate, int bufferSize)
{
	PaError paErr = paNoError;

	//get device info
	const PaDeviceInfo *pDeviceInfo = Pa_GetDeviceInfo(m_DeviceID);

	//following parameters are needed opening test stream and for sample rates validation
	PaStreamParameters inputParameters, outputParameters;
	PaStreamParameters *pInS = NULL, *pOutS = NULL;

	inputParameters.device = m_DeviceID;
	inputParameters.channelCount = pDeviceInfo->maxInputChannels;
	inputParameters.sampleFormat = paFloat32 | paNonInterleaved;
	inputParameters.suggestedLatency = 0;
	inputParameters.hostApiSpecificStreamInfo = 0;

	if (inputParameters.channelCount)
		pInS = &inputParameters;

	outputParameters.device = m_DeviceID;
	outputParameters.channelCount = pDeviceInfo->maxOutputChannels;
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = 0;
	outputParameters.hostApiSpecificStreamInfo = 0;

	if (outputParameters.channelCount)
		pOutS = &outputParameters;

	PaStream *portAudioStream = NULL;
		
	//sometimes devices change buffer size if sample rate changes
	//it updates buffer size during stream opening
	//we need to find out how device would behave with current sample rate
	//try opening test stream to load device driver for current sample rate and buffer size
	paErr = Pa_OpenStream (&portAudioStream, pInS, pOutS, sampleRate, bufferSize, paDitherOff, NULL, NULL);
	
	if (portAudioStream)
	{
		// close test stream
		Pa_CloseStream (portAudioStream);
		portAudioStream = NULL;
	}

	return paErr;
}


//**********************************************************************************************
// WCMRPortAudioDevice::CurrentSamplingRate 
//
//! The device's current sampling rate. This may be overridden, if the device needs to 
//!		query the driver for the current rate.
//!
//! \param none
//! 
//! \return The device's current sampling rate. -1 on error.
//! 
//**********************************************************************************************
int WCMRPortAudioDevice::CurrentSamplingRate ()
{
    AUTO_FUNC_DEBUG;
	//ToDo: Perhaps for ASIO devices that are active, we should retrive the SR from the device...
	
	return (m_CurrentSamplingRate);
}


WTErr WCMRPortAudioDevice::SetActive (bool newState)
{
	if (newState == true)
	{
		std::cout << "API::Device " << m_DeviceName << " Activation requested" << std::endl;
		SignalObjectAndWait(m_hActivateRequestedEvent, m_hActivationDone, INFINITE, false);
	}
	else
	{
		std::cout << "API::Device " << m_DeviceName << " Deactivation requested" << std::endl;
		SignalObjectAndWait(m_hDeActivateRequestedEvent, m_hDeActivationDone, INFINITE, false);
	}

	if (newState == Active() )
		return eNoErr;
	else
		return eGenericErr;
}


WTErr WCMRPortAudioDevice::SetStreaming (bool newState)
{
	if (newState == true)
	{
		std::cout << "API::Device " << m_DeviceName << " Stream start requested" << std::endl;
		SignalObjectAndWait(m_hStartStreamingRequestedEvent, m_hStartStreamingDone, INFINITE, false);
	}
	else
	{
		std::cout << "API::Device " << m_DeviceName << " Stream stop requested" << std::endl;
		SignalObjectAndWait(m_hStopStreamingRequestedEvent, m_hStopStreamingDone, INFINITE, false);
	}

	if (newState == Streaming() )
		return eNoErr;
	else
		return eGenericErr;
}


WTErr WCMRPortAudioDevice::ResetDevice()
{
	std::cout << "API::Device: " << m_DeviceName << " Reseting device" << std::endl;
	
	SignalObjectAndWait(m_hResetRequestedEvent, m_hResetDone, INFINITE, false);

	if (ConnectionStatus() == DeviceErrors)
	{
		return m_lastErr;
	}

	return eNoErr;
}


//**********************************************************************************************
// WCMRPortAudioDevice::SetCurrentSamplingRate 
//
//! Change the sampling rate to be used by the device. 
//!
//! \param newRate : The rate to use (samples per sec).
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRPortAudioDevice::SetCurrentSamplingRate (int newRate)
{
    AUTO_FUNC_DEBUG;
	std::vector<int>::iterator intIter;
	WTErr retVal = eNoErr;

	//changes the status.
	int oldRate = CurrentSamplingRate();
	bool oldActive = Active();
	
	//no change, nothing to do
	if (oldRate == newRate)
		return (retVal);

	//see if this is one of our supported rates...
	intIter = find(m_SamplingRates.begin(), m_SamplingRates.end(), newRate);

	if (intIter == m_SamplingRates.end())
	{
		//Can't change, perhaps use an "invalid param" type of error
		retVal = eCommandLineParameter;
		return (retVal);
	}
	
	if (Streaming())
	{
		//Can't change, perhaps use an "in use" type of error
		retVal = eGenericErr;
		return (retVal);
	}
	
	//make the change...
	m_CurrentSamplingRate = newRate;
	PaError paErr = PaAsio_SetStreamSampleRate (m_PortAudioStream, m_CurrentSamplingRate);
	Pa_Sleep(PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS); // sleep some time to make sure the change has place

	if (paErr != paNoError)
	{
		std::cout << "Sample rate change failed: " <<  Pa_GetErrorText (paErr) << std::endl;
		if (paErr ==  paUnanticipatedHostError)
			std::cout << "Details: "<< Pa_GetLastHostErrorInfo ()->errorText << "; code: " << Pa_GetLastHostErrorInfo ()->errorCode << std::endl;

		retVal = eWrongObjectState;
	}
	
	return (retVal);
}


//**********************************************************************************************
// WCMRPortAudioDevice::CurrentBufferSize
//
//! The device's current buffer size in use. This may be overridden, if the device needs to 
//!		query the driver for the current size.
//!
//! \param none
//! 
//! \return The device's current buffer size. 0 on error.
//! 
//**********************************************************************************************
int WCMRPortAudioDevice::CurrentBufferSize ()
{
	return m_CurrentBufferSize;
}


//**********************************************************************************************
// WCMRPortAudioDevice::SetCurrentBufferSize
//
//! Change the buffer size to be used by the device. This will most likely be overridden, 
//!		the base class simply updates the member variable.
//!
//! \param newSize : The buffer size to use (in sample-frames)
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRPortAudioDevice::SetCurrentBufferSize (int newSize)
{
    AUTO_FUNC_DEBUG;
	WTErr retVal = eNoErr;
	std::vector<int>::iterator intIter;

	if (Streaming())
	{
		//Can't change, perhaps use an "in use" type of error
		retVal = eGenericErr;
		return (retVal);
	}

	// Buffer size for ASIO devices can be changed from the control panel only
	// We have driver driven logi here
	if (m_CurrentBufferSize != newSize )
	{
		// we have only one aloved buffer size which is preffered by PA
		// this is the only value which could be set
		newSize = m_BufferSizes[0];
		int bufferSize = newSize;
		// notify client to update buffer size
		m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::BufferSizeChanged, (void *)&bufferSize);
		return retVal;
	}

	return (retVal);
}


//**********************************************************************************************
// WCMRPortAudioDevice::ConnectionStatus 
//
//! Retrieves the device's current connection status. This will most likely be overridden,
//!		in case some driver communication is required to query the status.
//!
//! \param none
//! 
//! \return A ConnectionStates value.
//! 
//**********************************************************************************************
WCMRPortAudioDevice::ConnectionStates WCMRPortAudioDevice::ConnectionStatus ()
{
    AUTO_FUNC_DEBUG;
	//ToDo: May want to do something more to extract the actual status!
	return (m_ConnectionStatus);
	
}


//**********************************************************************************************
// WCMRPortAudioDevice::activateDevice
//
//!	IS CALLED BY PROCESS THREAD
//! Sets the device into "active" state. Essentially, opens the PA device. 
//!		If it's an ASIO device it may result in buffer size change in some cases.
//!
//**********************************************************************************************
void WCMRPortAudioDevice::activateDevice (bool callerIsWaiting/*=false*/)
{
	AUTO_FUNC_DEBUG;

	PaError paErr = paNoError;
	
	// if device is not active activate it
	if (!Active() )
	{
		PaStreamParameters inputParameters, outputParameters;
		PaStreamParameters *pInS = NULL, *pOutS = NULL;

		const PaDeviceInfo *pDeviceInfo = Pa_GetDeviceInfo(m_DeviceID);
		const PaHostApiInfo *pHostApiInfo = Pa_GetHostApiInfo(pDeviceInfo->hostApi);

		inputParameters.device = m_DeviceID;
		inputParameters.channelCount = (int)m_InputChannels.size();
		inputParameters.sampleFormat = paFloat32 | paNonInterleaved;
		inputParameters.suggestedLatency = Pa_GetDeviceInfo(m_DeviceID)->defaultLowInputLatency;
		inputParameters.hostApiSpecificStreamInfo = 0;

		if (inputParameters.channelCount)
			pInS = &inputParameters;

		outputParameters.device = m_DeviceID;
		outputParameters.channelCount = (int)m_OutputChannels.size();
		outputParameters.sampleFormat = paFloat32;
		outputParameters.suggestedLatency = Pa_GetDeviceInfo(m_DeviceID)->defaultLowOutputLatency;
		outputParameters.hostApiSpecificStreamInfo = 0;

		if (outputParameters.channelCount)
			pOutS = &outputParameters;

		std::cout << "API::Device " << m_DeviceName << " Opening device stream " << std::endl;
		std::cout << "Sample rate: " << m_CurrentSamplingRate << " buffer size: " << m_CurrentBufferSize << std::endl;
		paErr = Pa_OpenStream(&m_PortAudioStream, 
								pInS,
								pOutS,
								m_CurrentSamplingRate,
								m_CurrentBufferSize,
								paDitherOff,
								WCMRPortAudioDevice::TheCallback,
								this);
			
		if(paErr != paNoError)
		{
			std::cout << "Cannot open streamm with buffer: "<< m_CurrentBufferSize << " Error: " << Pa_GetErrorText (paErr) << std::endl;
			
			if (paErr ==  paUnanticipatedHostError)
				std::cout << "Error details: "<< Pa_GetLastHostErrorInfo ()->errorText << "; code: " << Pa_GetLastHostErrorInfo ()->errorCode << std::endl;
		}

		if(paErr == paNoError)
		{
			std::cout << "Stream has been opened! "<< std::endl;

			// check for possible changes
			long minSize, maxSize, preferredSize, granularity;
			PaError paErr = PaAsio_GetAvailableBufferSizes(m_DeviceID, &minSize, &maxSize, &preferredSize, &granularity);

			std::cout << "Checked if buffer size changed "<< std::endl;
			if (paErr == paNoError && m_CurrentBufferSize != preferredSize)
			{
				std::cout << "Buffer size has changed "<< std::endl;
				m_CurrentBufferSize = preferredSize;
				m_BufferSizes.clear();
				m_BufferSizes.push_back(preferredSize);
				m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::BufferSizeChanged, (void *)&preferredSize);
			}

			m_DropsDetected = 0;
			m_DropsReported = 0;
			m_IgnoreThisDrop = true;

			if (pHostApiInfo->type == paASIO)
			{
				m_BufferSizeChangeRequested = 0;
				m_BufferSizeChangeReported = 0;
				m_ResetRequested = 0;
				m_ResetReported = 0;
				m_ResyncRequested = 0;
				m_ResyncReported = 0;
				std::cout << "Installing new mesage hook "<< std::endl;
				PaAsio_SetMessageHook (StaticASIOMessageHook, this);
			}
			m_IsActive = true;
			m_ConnectionStatus = DeviceAvailable;
			m_lastErr = eNoErr;
		}
		else
		{
			//failed, do not update device state
			std::cout << "Failed to open pa stream: " <<  Pa_GetErrorText (paErr) << std::endl;
			DEBUG_MSG( "Failed to open pa stream: " << Pa_GetErrorText (paErr) );
			m_ConnectionStatus = DeviceErrors;
			m_lastErr = eAsioFailed;
		}

	
	}

	std::cout << "Activation is DONE "<< std::endl;

	if (callerIsWaiting)
		SetEvent(m_hActivationDone);
}


//**********************************************************************************************
// WCMRPortAudioDevice::deactivateDevice
//
//!	IS CALLED BY PROCESS THREAD
//! Sets the device into "inactive" state. Essentially, closes the PA device. 
//!
//**********************************************************************************************
void WCMRPortAudioDevice::deactivateDevice (bool callerIsWaiting/*=false*/)
{
    AUTO_FUNC_DEBUG;

	PaError paErr = paNoError;
	
	if (Active() )
	{
		if (Streaming())
		{
			stopStreaming ();
		}
		
		if (m_PortAudioStream)
		{
			//close the stream first
			std::cout << "API::Device" << m_DeviceName << " Closing device stream" << std::endl;
			paErr = Pa_CloseStream (m_PortAudioStream);
			if(paErr == paNoError)
			{
				m_PortAudioStream = NULL;
				m_DropsDetected = 0;
				m_DropsReported = 0;
				m_IgnoreThisDrop = true;
				m_BufferSizeChangeRequested = 0;
				m_BufferSizeChangeReported = 0;
				m_ResetRequested = 0;
				m_ResetReported = 0;
				m_ResyncRequested = 0;
				m_ResyncReported = 0;
				PaAsio_SetMessageHook (NULL, NULL);

				//finaly set device state to "not active"
				m_IsActive = false;
				m_ConnectionStatus = DeviceDisconnected;
				m_lastErr = eNoErr;
			}
			else
			{
				//failed, do not update device state
				std::cout << "Failed to close pa stream stream " <<  Pa_GetErrorText (paErr) << std::endl;
				DEBUG_MSG( "Failed to open pa stream stream " << Pa_GetErrorText (paErr) );
				m_ConnectionStatus = DeviceErrors;
				m_lastErr = eAsioFailed;
			}
		}
	}

	if (callerIsWaiting)
		SetEvent(m_hDeActivationDone);
}


//**********************************************************************************************
// WCMRPortAudioDevice::startStreaming
//
//! Sets the devices into "streaming" state. Calls PA's Start stream routines.
//! This roughly corresponds to calling Start on the lower level interface.
//! 
//**********************************************************************************************
void WCMRPortAudioDevice::startStreaming (bool callerIsWaiting/*=false*/)
{
    AUTO_FUNC_DEBUG;

	// proceed if the device is not streaming
	if (!Streaming () )
	{
		PaError paErr = paNoError;
		m_StopRequested = false;
		m_SampleCounter = 0;

		std::cout << "API::Device" << m_DeviceName << " Starting device stream" << std::endl;
		
		//get device info
		const PaDeviceInfo *pDeviceInfo = Pa_GetDeviceInfo(m_DeviceID);
	
		unsigned int inChannelCount = pDeviceInfo->maxInputChannels;
		unsigned int outChannelCount = pDeviceInfo->maxOutputChannels;
		
		paErr = Pa_StartStream( m_PortAudioStream );

		if(paErr == paNoError)
		{
			// if the stream was started successfully
			m_IsStreaming = true;
			std::cout << "API::Device" << m_DeviceName << " Device is streaming" << std::endl;
		}
		else
		{
			std::cout << "Failed to start PA stream: " <<  Pa_GetErrorText (paErr) << std::endl;
			DEBUG_MSG( "Failed to start PA stream: " << Pa_GetErrorText (paErr) );
			m_lastErr = eGenericErr;
		}
	}
		
	if (callerIsWaiting)
		SetEvent(m_hStartStreamingDone);
}


//**********************************************************************************************
// WCMRPortAudioDevice::stopStreaming
//
//! Sets the devices into "not streaming" state. Calls PA's Stop stream routines.
//! This roughly corresponds to calling Stop on the lower level interface.
//! 
//**********************************************************************************************
void WCMRPortAudioDevice::stopStreaming (bool callerIsWaiting/*=false*/)
{
    AUTO_FUNC_DEBUG;

	// proceed if the device is streaming
	if (Streaming () )
	{
		PaError paErr = paNoError;
		m_StopRequested = true;

		std::cout << "API::Device " << m_DeviceName << " Stopping device stream" << std::endl;
		paErr = Pa_StopStream( m_PortAudioStream );

		if(paErr == paNoError || paErr == paStreamIsStopped)
		{
			// if the stream was stopped successfully
			m_IsStreaming = false;
			m_pInputData = NULL;
		}
		else
		{
			std::cout << "Failed to stop PA stream normaly! Error:" <<  Pa_GetErrorText (paErr) << std::endl;
			DEBUG_MSG( "Failed to stop PA stream normaly! Error:" << Pa_GetErrorText (paErr) );
			m_lastErr = eGenericErr;
		}
	}

	if (callerIsWaiting)
		SetEvent(m_hStopStreamingDone);
}


//**********************************************************************************************
// WCMRPortAudioDevice::resetDevice 
//
//! Resets the device, updates device info. Importnat: does PA reinitialization calling
//! Pa_terminate/Pa_initialize functions.
//!
//! \param none
//! 
//! \return nothing
//! 
//**********************************************************************************************
void WCMRPortAudioDevice::resetDevice (bool callerIsWaiting /*=false*/ )
{
	PaError paErr = paNoError;

	// Keep device sates
	bool wasStreaming = Streaming();
	bool wasActive = Active();

	// Reset the device
	stopStreaming();
	deactivateDevice();

	// Cache device buffer size as it might be changed during reset
	int oldBufferSize = m_CurrentBufferSize;

	// Now, validate the state and update device info if required
	unsigned int retry = PROPERTY_CHANGE_RETRIES;
	while (retry-- )
	{
		// Reinitialize PA
		Pa_Terminate();
		Pa_Initialize();
			
		std::cout << "Updating device state... " << std::endl;
		// update device info
		updateDeviceInfo();

		// take up buffers
		long minSize, maxSize, preferredSize, granularity;
		PaError paErr = PaAsio_GetAvailableBufferSizes(m_DeviceID, &minSize, &maxSize, &preferredSize, &granularity);

		if (paErr != paNoError)
		{
			continue;
		} 
		m_CurrentBufferSize = preferredSize;

		paErr = testStateValidness(m_CurrentSamplingRate, m_CurrentBufferSize);
		if (paNoError ==  paErr)
		{
			std::cout << "Device state is valid" << std::endl;
			break;
		}

		std::cout << "Cannot start with current state: sr: " << m_CurrentSamplingRate << " bs:" << m_CurrentBufferSize \
					<< "\nReason: " <<  Pa_GetErrorText (paErr) << std::endl;
		if (paErr ==  paUnanticipatedHostError)
			std::cout << "Details: "<< Pa_GetLastHostErrorInfo ()->errorText << "; code: " << Pa_GetLastHostErrorInfo ()->errorCode << std::endl;

		std::cout << "Will try again in " << DEVICE_INFO_UPDATE_SLEEP_TIME_MILLISECONDS << "msec" << std::endl;

		Pa_Sleep(DEVICE_INFO_UPDATE_SLEEP_TIME_MILLISECONDS);
	}

	if (paErr == paNoError)
	{
		// Notify the Application about device setting changes
		if (oldBufferSize != m_CurrentBufferSize)
		{
			std::cout << "API::Device" << m_DeviceName << " buffer size changed" << std::endl;
			int bufferSize = m_CurrentBufferSize;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::BufferSizeChanged, (void *)&bufferSize);
		}

		// Activate the device if it was active before
		if (wasActive)
			activateDevice();

		// Resume streaming if the device was streaming before
		if(wasStreaming && m_lastErr == eNoErr && m_ConnectionStatus == DeviceAvailable)
		{
			// Notify the Application to prepare for the stream start
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceStartsStreaming);
			startStreaming();
		}
	} else {
		m_ConnectionStatus = DeviceErrors;
		m_lastErr = eWrongObjectState;
	}

	if (callerIsWaiting)
		SetEvent(m_hResetDone);
}


#ifdef PLATFORM_WINDOWS

long WCMRPortAudioDevice::StaticASIOMessageHook (void *pRefCon, long selector, long value, void* message, double* opt)
{
	if (pRefCon)
	{
		return ((WCMRPortAudioDevice*)(pRefCon))->ASIOMessageHook (selector, value, message, opt);
	}
	else
		return -1;
}

long WCMRPortAudioDevice::ASIOMessageHook (long selector, long WCUNUSEDPARAM(value), void* WCUNUSEDPARAM(message), double* WCUNUSEDPARAM(opt))
{
	switch(selector)
	{
		case kAsioResyncRequest:
			m_ResyncRequested++;
			std::cout << "\t\t\tWCMRPortAudioDevice::ASIOMessageHook -- kAsioResyncRequest" << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
			break;

		case kAsioLatenciesChanged:
			m_BufferSizeChangeRequested++;
			std::cout << "\t\t\tWCMRPortAudioDevice::ASIOMessageHook -- kAsioLatenciesChanged" << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
			break;

		case kAsioBufferSizeChange:
			m_BufferSizeChangeRequested++;
			std::cout << "\t\t\tWCMRPortAudioDevice::ASIOMessageHook -- m_BufferSizeChangeRequested" << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
			break;

		case kAsioResetRequest:
			m_ResetRequested++;
			std::cout << "\t\t\tWCMRPortAudioDevice::ASIOMessageHook -- kAsioResetRequest" << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
			break;

        case kAsioOverload:
			m_DropsDetected++;
			std::cout << "\t\t\tWCMRPortAudioDevice::ASIOMessageHook -- kAsioOverload" << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::Dropout);
            break;
	}
	return 0;
}

#endif


//**********************************************************************************************
// WCMRPortAudioDevice::DoIdle 
//
//! A place for doing idle time processing. The other derived classes will probably do something
//!		meaningful.
//!
//! \param none
//! 
//! \return eNoErr always.
//! 
//**********************************************************************************************
WTErr WCMRPortAudioDevice::DoIdle ()
{
	WTErr retVal = eNoErr;

	std::cout << "WCMRPortAudioDevice::DoIdle ()" << std::endl;
	HANDLE hEvents[] = 
	{
		m_hUpdateDeviceInfoRequestedEvent,
		m_hActivateRequestedEvent,
		m_hDeActivateRequestedEvent,
		m_hStartStreamingRequestedEvent,
		m_hStopStreamingRequestedEvent,
		m_hBufferSizeChangedEvent,
		m_hSampleRateChangedEvent,
		m_hResetRequestedEvent,
		m_hResetFromDevRequestedEvent,
		m_hExitIdleThread
	};

	const size_t hEventsSize = sizeof(hEvents)/sizeof(hEvents[0]);
	
	initDevice();

	for(;;)
	{
		DWORD result = WaitForMultipleObjects (hEventsSize, hEvents, FALSE, INFINITE);
		result = result - WAIT_OBJECT_0;

		if ((result < 0) || (result >= hEventsSize)) {
			std::cout << "\t\t\t\t\t\t\tWCMRPortAudioDevice::DoIdle () -> (result < 0) || (result >= hEventsSize):" << result << std::endl;
			retVal = eGenericErr;
			break;
		}

		if (hEvents[result] == m_hExitIdleThread) {
			std::cout << "\t\t\t\t\t\t\tWCMRPortAudioDevice::DoIdle () -> m_hExitIdleThread" << result << std::endl;
			retVal = eNoErr;
			break;
		}

		if (hEvents[result] == m_hUpdateDeviceInfoRequestedEvent) {
			std::cout << "\t\t\t\t\t\tupdate requested ..." << std::endl;
			updateDeviceInfo(true);
		}

		if (hEvents[result] == m_hActivateRequestedEvent) {
			std::cout << "\t\t\t\t\t\tactivation requested ..." << std::endl;
			activateDevice(true);
		}

		if (hEvents[result] == m_hDeActivateRequestedEvent) {
			std::cout << "\t\t\t\t\t\tdeactivation requested ..." << std::endl;
			deactivateDevice(true);
		}

		if (hEvents[result] == m_hStartStreamingRequestedEvent) {
			std::cout << "\t\t\t\t\t\tStart stream requested ..." << std::endl;
			startStreaming(true);
		}

		if (hEvents[result] == m_hStopStreamingRequestedEvent) {
			std::cout << "\t\t\t\t\t\tStop stream requested ..." << std::endl;
			stopStreaming(true);
		}

		if (hEvents[result] == m_hResetRequestedEvent) {
			std::cout << "\t\t\t\t\t\treset requested ..." << std::endl;
			resetDevice(true);
		}

		if (hEvents[result] == m_hResetFromDevRequestedEvent) {
			std::cout << "\t\t\t\t\t\treset requested from device..." << std::endl;
			resetDevice();
		}

		if (hEvents[result] == m_hBufferSizeChangedEvent) {
			std::cout << "\t\t\t\t\t\tbuffer size changed from device..." << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::BufferSizeChanged);
		}

		if (hEvents[result] == m_hSampleRateChangedEvent) {
			std::cout << "\t\t\t\t\t\tsample rate changed from device..." << std::endl;
			m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::SamplingRateChanged);
		}
	}

	terminateDevice();

	return retVal;
}


//**********************************************************************************************
// WCMRPortAudioDevice::SetMonitorChannels 
//
//! Used to set the channels to be used for monitoring.
//!
//! \param leftChannel : Left monitor channel index.
//! \param rightChannel : Right monitor channel index.
//! 
//! \return eNoErr always, the derived classes may return appropriate errors.
//! 
//**********************************************************************************************
WTErr WCMRPortAudioDevice::SetMonitorChannels (int leftChannel, int rightChannel)
{
    AUTO_FUNC_DEBUG;
	//This will most likely be overridden, the base class simply
	//changes the member.
	m_LeftMonitorChannel = leftChannel;
	m_RightMonitorChannel = rightChannel;
	return (eNoErr);
}



//**********************************************************************************************
// WCMRPortAudioDevice::SetMonitorGain 
//
//! Used to set monitor gain (or atten).
//!
//! \param newGain : The new gain or atten. value to use. Specified as a linear multiplier (not dB) 
//! 
//! \return eNoErr always, the derived classes may return appropriate errors.
//! 
//**********************************************************************************************
WTErr WCMRPortAudioDevice::SetMonitorGain (float newGain)
{
    AUTO_FUNC_DEBUG;
	//This will most likely be overridden, the base class simply
	//changes the member.
	
	m_MonitorGain = newGain;
	return (eNoErr);
}




//**********************************************************************************************
// WCMRPortAudioDevice::ShowConfigPanel 
//
//! Used to show device specific config/control panel. Some interfaces may not support it.
//!		Some interfaces may require the device to be active before it can display a panel.
//!
//! \param pParam : A device/interface specific parameter, should be the app window handle for ASIO.
//! 
//! \return eNoErr always, the derived classes may return errors.
//! 
//**********************************************************************************************
WTErr WCMRPortAudioDevice::ShowConfigPanel (void *pParam)
{
    AUTO_FUNC_DEBUG;
	WTErr retVal = eNoErr;
	
	if (Active() && !m_ResetRequested )
	{
#ifdef PLATFORM_WINDOWS
		if(Pa_GetHostApiInfo(Pa_GetDeviceInfo(m_DeviceID)->hostApi)->type == paASIO)
		{
			// stop and deactivate the device
			bool wasStreaming = Streaming();
			SetActive(false);

			// show control panel for the device
			if (PaAsio_ShowControlPanel (m_DeviceID, pParam) != paNoError)
				retVal = eGenericErr;
			
			// restore previous state for the device
			SetActive(true);
			if (wasStreaming)
				SetStreaming(true);


			// reset device to pick up changes
			if (!m_ResetRequested) {
				m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
			}
		}
#else
	pParam = pParam;
#endif //_windows		
	}
	
	return (retVal);
}


//*****************************************************************************************************
// WCMRPortAudioDevice::TheCallback
//
//! The (static) Port Audio Callback function. This is a static member. It calls on the AudioCallback in the 
//!		WCMRPortAudioDevice to do the real work.
//!
//! \param pInputBuffer: pointer to input buffer.
//! \param pOutputBuffer: pointer to output buffer.
//! \param framesPerBuffer: number of sample frames per buffer.
//! \param pTimeInfo: time info for PaStream callback.
//! \param statusFlags:
//! \param pUserData: pointer to user data, in our case the WCMRPortAudioDevice object.
//! 
//! \return true to stop streaming else returns false.
//******************************************************************************************************
int WCMRPortAudioDevice::TheCallback (const void *pInputBuffer, void *pOutputBuffer, unsigned long framesPerBuffer, 
	const PaStreamCallbackTimeInfo* /*pTimeInfo*/, PaStreamCallbackFlags statusFlags, void *pUserData )
{
	WCMRPortAudioDevice *pMyDevice = (WCMRPortAudioDevice *)pUserData;
	if (pMyDevice)
		return pMyDevice->AudioCallback ((float *)pInputBuffer, (float *)pOutputBuffer, framesPerBuffer,
			(statusFlags & (paInputOverflow | paOutputUnderflow)) != 0);
	else
		return (true);
			
}



//**********************************************************************************************
// WCMRPortAudioDevice::AudoiCallback 
//
//! Here's where the actual audio processing happens. We call upon all the active connections' 
//!		sinks to provide data to us which can be put/mixed in the output buffer! Also, we make the 
//!		input data available to any sources	that may call upon us during this time!
//!
//! \param *pInputBuffer : Points to a buffer with recorded data.
//! \param *pOutputBuffer : Points to a buffer to receive playback data.
//! \param framesPerBuffer : Number of sample frames in input and output buffers. Number of channels,
//!		which are interleaved, is fixed at Device Open (Active) time. In this implementation,
//!		the number of channels are fixed to use the maximum available.
//!	\param dropsDetected : True if dropouts were detected in input or output. Can be used to signal the GUI.
//! 
//! \return true
//! 
//**********************************************************************************************
int WCMRPortAudioDevice::AudioCallback( const float *pInputBuffer, float *pOutputBuffer, unsigned long framesPerBuffer, bool dropsDetected )
{
	UMicroseconds theStartTime;

    // detect drops
	if (dropsDetected)
	{
		if (m_IgnoreThisDrop)
			m_IgnoreThisDrop = false; //We'll ignore once, just once!
		else
			m_DropsDetected++;
	}

	m_pInputData = pInputBuffer;

    // VKamyshniy: Is this a right place to call the client???:
    struct WCMRAudioDeviceManagerClient::AudioCallbackData audioCallbackData =
    {
        m_pInputData,
        pOutputBuffer,
        framesPerBuffer,
		m_SampleCounter,
		theStartTime.MicroSeconds()*1000
    };
    
    m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::AudioCallback, (void *)&audioCallbackData );

	//Don't try to 	access after this call returns!
	m_pInputData = NULL;

	m_SampleCounter += framesPerBuffer;	

	return m_StopRequested;
}




//**********************************************************************************************
// WCMRPortAudioDeviceManager::WCMRPortAudioDeviceManager
//
//! The constructuor, we initialize PA, and build the device list.
//!
//! \param *pTheClient : The manager's client object (which receives notifications).
//! \param interfaceType : The PortAudio interface type to use for this manager - acts as a filter.
//! \param useMultithreading : Whether to use multi-threading for audio processing. Default is true.
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRPortAudioDeviceManager::WCMRPortAudioDeviceManager (WCMRAudioDeviceManagerClient *pTheClient, 
														eAudioDeviceFilter eCurAudioDeviceFilter, bool useMultithreading, bool bNocopy)
	: WCMRAudioDeviceManager (pTheClient, eCurAudioDeviceFilter)
	, m_NoneDevice(0)
	, m_UseMultithreading(useMultithreading)
	, m_bNoCopyAudioBuffer(bNocopy)
{
    AUTO_FUNC_DEBUG;
	std::cout << "API::PortAudioDeviceManager::PA Device manager constructor" << std::endl;
	
	//Always create the None device first...
	m_NoneDevice = new WCMRNativeAudioNoneDevice(this);

	WTErr err = generateDeviceListImpl();

	if (eNoErr != err)
		throw err;

	timeBeginPeriod (1);
}


//**********************************************************************************************
// WCMRPortAudioDeviceManager::~WCMRPortAudioDeviceManager
//
//! It clears the device list, releasing each of the device.
//!
//! \param none
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRPortAudioDeviceManager::~WCMRPortAudioDeviceManager()
{
    AUTO_FUNC_DEBUG;
	
	std::cout << "API::Destroying PortAudioDeviceManager " << std::endl;

	try
	{
		delete m_NoneDevice;
	}
	catch (...)
	{
		//destructors should absorb exceptions, no harm in logging though!!
		DEBUG_MSG ("Exception during destructor");
	}

	timeEndPeriod (1);
}


WCMRAudioDevice* WCMRPortAudioDeviceManager::initNewCurrentDeviceImpl(const std::string & deviceName)
{
    destroyCurrentDeviceImpl();
    
	std::cout << "API::PortAudioDeviceManager::initNewCurrentDevice " << deviceName << std::endl;
	if (deviceName == m_NoneDevice->DeviceName() )
	{
		m_CurrentDevice = m_NoneDevice;
		return m_CurrentDevice;
	}

	DeviceInfo devInfo;
	WTErr err = GetDeviceInfoByName(deviceName, devInfo);

	if (eNoErr == err)
	{
		try
		{
			std::cout << "API::PortAudioDeviceManager::Creating PA device: " << devInfo.m_DeviceId << ", Device Name: " << devInfo.m_DeviceName << std::endl;
			TRACE_MSG ("API::PortAudioDeviceManager::Creating PA device: " << devInfo.m_DeviceId << ", Device Name: " << devInfo.m_DeviceName);
		
			m_CurrentDevice = new WCMRPortAudioDevice (this, devInfo.m_DeviceId, m_UseMultithreading, m_bNoCopyAudioBuffer);
		}
		catch (...)
		{
			std::cout << "Unabled to create PA Device: " << devInfo.m_DeviceId << std::endl;
			DEBUG_MSG ("Unabled to create PA Device: " << devInfo.m_DeviceId);
		}
	}

	return m_CurrentDevice;
}


void WCMRPortAudioDeviceManager::destroyCurrentDeviceImpl()
{
	if (m_CurrentDevice != m_NoneDevice)
		delete m_CurrentDevice;

	m_CurrentDevice = 0;
}


WTErr WCMRPortAudioDeviceManager::getDeviceAvailableSampleRates(DeviceID deviceId, std::vector<int>& sampleRates)
{
	WTErr retVal = eNoErr;

	sampleRates.clear();
	const PaDeviceInfo *pPaDeviceInfo = Pa_GetDeviceInfo(deviceId);

	//now find supported sample rates
	//following parameters are needed for sample rates validation
	PaStreamParameters inputParameters, outputParameters;
	PaStreamParameters *pInS = NULL, *pOutS = NULL;

	inputParameters.device = deviceId;
	inputParameters.channelCount = std::min<int>(2, pPaDeviceInfo->maxInputChannels);
	inputParameters.sampleFormat = paFloat32 | paNonInterleaved;
	inputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
	inputParameters.hostApiSpecificStreamInfo = 0;

	if (inputParameters.channelCount)
		pInS = &inputParameters;

	outputParameters.device = deviceId;
	outputParameters.channelCount = std::min<int>(2, pPaDeviceInfo->maxOutputChannels);
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
	outputParameters.hostApiSpecificStreamInfo = 0;

	if (outputParameters.channelCount)
		pOutS = &outputParameters;

	for(int sr=0; gAllSampleRates[sr] > 0; sr++)
	{
		if( paFormatIsSupported == Pa_IsFormatSupported(pInS, pOutS, gAllSampleRates[sr]) )
		{
			sampleRates.push_back ((int)gAllSampleRates[sr]);
		}
	}

	return retVal;
}


WTErr WCMRPortAudioDeviceManager::generateDeviceListImpl()
{
	std::cout << "API::PortAudioDeviceManager::Generating device list" << std::endl;
	
	WTErr retVal = eNoErr;

	//Initialize PortAudio and ASIO first
	PaError paErr = Pa_Initialize();

	if (paErr != paNoError)
	{
		//ToDo: throw an exception here!
		retVal = eSomeThingNotInitailzed;
		return retVal;
	}

	// lock DeviceInfoVec firts
	wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceInfoVecMutex);

	if (m_NoneDevice)
	{
		DeviceInfo *pDevInfo = new DeviceInfo(NONE_DEVICE_ID, m_NoneDevice->DeviceName() );
		pDevInfo->m_AvailableSampleRates = m_NoneDevice->SamplingRates();
		m_DeviceInfoVec.push_back(pDevInfo);
	}

	//Get device count...
	int numDevices = Pa_GetDeviceCount();

	//for each device,
	for (int thisDeviceID = 0; thisDeviceID < numDevices; thisDeviceID++)
	{
		//if it's of the required type...
		const PaDeviceInfo *pPaDeviceInfo = Pa_GetDeviceInfo(thisDeviceID);
		
		if (Pa_GetHostApiInfo(pPaDeviceInfo->hostApi)->type == paASIO)
		{
			//build a device object...
			try
			{
				std::cout << "API::PortAudioDeviceManager::DeviceID: " << thisDeviceID << ", Device Name: " << pPaDeviceInfo->name << std::endl;
				TRACE_MSG ("PA DeviceID: " << thisDeviceID << ", Device Name: " << pPaDeviceInfo->name);

				DeviceInfo *pDevInfo = new DeviceInfo(thisDeviceID, pPaDeviceInfo->name);
				if (pDevInfo)
				{
					std::vector<int> availableSampleRates;
					WTErr wErr = WCMRPortAudioDeviceManager::getDeviceAvailableSampleRates(thisDeviceID, availableSampleRates);

					if (wErr != eNoErr)
					{
						DEBUG_MSG ("Failed to get device available sample rates. Device ID: " << m_DeviceID);
						delete pDevInfo;
						continue; //proceed to the next device
					}

					pDevInfo->m_AvailableSampleRates = availableSampleRates;
					pDevInfo->m_MaxInputChannels = pPaDeviceInfo->maxInputChannels;
					pDevInfo->m_MaxOutputChannels = pPaDeviceInfo->maxOutputChannels;

					//Now check if this device is acceptable according to current input/output settings
					bool bRejectDevice = false;
					switch(m_eAudioDeviceFilter)
					{
						case eInputOnlyDevices:
							if (pDevInfo->m_MaxInputChannels != 0)
							{
								m_DeviceInfoVec.push_back(pDevInfo);
							}
							else
							{
								// Delete unnecesarry device
								bRejectDevice = true;
							}
							break;
						case eOutputOnlyDevices:
							if (pDevInfo->m_MaxOutputChannels != 0)
							{
								m_DeviceInfoVec.push_back(pDevInfo);
							}
							else
							{
								// Delete unnecesarry device
								bRejectDevice = true;
							}
							break;
						case eFullDuplexDevices:
							if (pDevInfo->m_MaxInputChannels != 0 && pDevInfo->m_MaxOutputChannels != 0)
							{
								m_DeviceInfoVec.push_back(pDevInfo);
							}
							else
							{
								// Delete unnecesarry device
								bRejectDevice = true;
							}
							break;
						case eAllDevices:
						default:
							m_DeviceInfoVec.push_back(pDevInfo);
							break;
					}
                
					if(bRejectDevice)
					{
						TRACE_MSG ("API::PortAudioDeviceManager::Device " << pDevInfo->m_DeviceName << "Rejected. \
									In Channels = " << pDevInfo->m_MaxInputChannels << "Out Channels = " <<pDevInfo->m_MaxOutputChannels );
						delete pDevInfo;
					}
				}
			}
			catch (...)
			{
				std::cout << "API::PortAudioDeviceManager::Unabled to create PA Device: " << std::endl;
				DEBUG_MSG ("Unabled to create PA Device: " << thisDeviceID);
			}
		}
	}

	//If no devices were found, that's not a good thing!
	if (m_DeviceInfoVec.empty() )
	{
		std::cout << "API::PortAudioDeviceManager::No matching PortAudio devices were found, total PA devices = " << numDevices << std::endl;
		DEBUG_MSG ("No matching PortAudio devices were found, total PA devices = " << numDevices);
	}

	//we don't need PA initialized right now
	Pa_Terminate();

	return retVal;
}


WTErr WCMRPortAudioDeviceManager::getDeviceSampleRatesImpl(const std::string & deviceName, std::vector<int>& sampleRates) const
{
    sampleRates.clear ();
    
    WTErr retVal = eNoErr;
    
	if (m_CurrentDevice && deviceName == m_CurrentDevice->DeviceName() )
	{
		sampleRates.assign(m_CurrentDevice->SamplingRates().begin(), m_CurrentDevice->SamplingRates().end() );
		return retVal;
	}

    DeviceInfo devInfo;
	retVal = GetDeviceInfoByName(deviceName, devInfo);
    
	if (eNoErr == retVal)
	{
		sampleRates.assign(devInfo.m_AvailableSampleRates.begin(), devInfo.m_AvailableSampleRates.end() );
	}
	else
	{
		std::cout << "API::PortAudioDeviceManager::GetSampleRates: Device not found: "<< deviceName << std::endl;
	}

	return retVal;
}


WTErr WCMRPortAudioDeviceManager::getDeviceBufferSizesImpl(const std::string & deviceName, std::vector<int>& buffers) const
{
	WTErr retVal = eNoErr;
	
	buffers.clear();

	//first check if the request has been made for None device
	if (deviceName == m_NoneDevice->DeviceName() )
	{
		buffers.assign(m_NoneDevice->BufferSizes().begin(), m_NoneDevice->BufferSizes().end() );
		return retVal;
	}
	
	if (m_CurrentDevice && deviceName == m_CurrentDevice->DeviceName() )
	{
		buffers.assign(m_CurrentDevice->BufferSizes().begin(), m_CurrentDevice->BufferSizes().end() );
		return retVal;
	}

	Pa_Initialize();

	DeviceInfo devInfo; 
	retVal = GetDeviceInfoByName(deviceName, devInfo);

	if (eNoErr == retVal)
	{
		//make PA request to get actual device buffer sizes
		long minSize, maxSize, preferredSize, granularity;
		PaError paErr = PaAsio_GetAvailableBufferSizes(devInfo.m_DeviceId, &minSize, &maxSize, &preferredSize, &granularity);

		//for Windows ASIO devices we always use prefferes buffer size ONLY
		if (paNoError == paErr )
		{
			buffers.push_back(preferredSize);
		}
		else
		{
			retVal = eAsioFailed;
			std::cout << "API::PortAudioDeviceManager::GetBufferSizes: error: " <<  Pa_GetErrorText (paErr) << " getting buffer sizes for device: "<< deviceName << std::endl;
		}
	}
	else
	{
		std::cout << "API::PortAudioDeviceManager::GetBufferSizes: Device not found: "<< deviceName << std::endl;
	}

	Pa_Terminate();

	return retVal;
}
