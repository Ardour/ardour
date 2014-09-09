//----------------------------------------------------------------------------------
//
// Copyright (c) 2008 Waves Audio Ltd. All rights reserved.
//
//! \file	WCMRNativeAudio.cpp
//!
//! WCMRNativeAudioConnection and related class defienitions
//!
//---------------------------------------------------------------------------------*/
#if defined(__APPLE__)
#include <CoreAudio/CoreAudio.h>
#endif

#include "WCMRNativeAudio.h"
#include "MiscUtils/pthread_utils.h"
#include "MiscUtils/safe_delete.h"
#include <iostream>
#include <sstream>
#include <boost/assign/list_of.hpp>

#define NONE_DEVICE_NAME "None"
#define NONE_DEVICE_INPUT_NAMES "Input "
#define NONE_DEVICE_OUTPUT_NAMES "Output "

//**********************************************************************************************
// WCMRNativeAudioNoneDevice::WCMRNativeAudioNoneDevice
//
//! Constructor for the dummy "None" device. This constructor simply adds supported SRs,
//!		buffer sizes, and channels, so that it may look like a real native device to
//!		the applications.
//!
//! \param pManager : The managing device manager - simply passed on to the base class.
//! 
//! 
//**********************************************************************************************
WCMRNativeAudioNoneDevice::WCMRNativeAudioNoneDevice (WCMRAudioDeviceManager *pManager)
	: WCMRNativeAudioDevice (pManager, false /*useMultiThreading*/)
#if defined (PLATFORM_WINDOWS)
    , _waitableTimerForUsleep (CreateWaitableTimer(NULL, TRUE, NULL))
#endif
{
	mark_pthread_inactive (m_SilenceThread);

	m_DeviceName = NONE_DEVICE_NAME;

	m_SamplingRates = boost::assign::list_of (m_CurrentSamplingRate=44100)(48000)(88200)(96000)(176400)(192000);

	m_BufferSizes = boost::assign::list_of (32)(64)(128)(m_CurrentBufferSize=256)(512)(1024);

	for (int channel = 0; channel < __m_NumInputChannels; channel++)
	{
        std::stringstream name;
        name << NONE_DEVICE_INPUT_NAMES;
		name << (channel + 1);
		m_InputChannels.push_back(name.str());
	}

	for (int channel = 0; channel < __m_NumOutputChannels; channel++)
	{
        std::stringstream name;
        name << NONE_DEVICE_INPUT_NAMES;
		name << (channel + 1);
		m_OutputChannels.push_back(name.str());
	}
	_m_inputBuffer = new float[__m_NumInputChannels * m_BufferSizes.back()];
	_m_outputBuffer = new float[__m_NumOutputChannels * m_BufferSizes.back()];
}


WCMRNativeAudioNoneDevice::~WCMRNativeAudioNoneDevice ()
{
#if defined (PLATFORM_WINDOWS)
    if(_waitableTimerForUsleep) {
        CloseHandle(_waitableTimerForUsleep);
    }
#endif
}

WTErr WCMRNativeAudioNoneDevice::SetActive (bool newState)
{
	//This will most likely be overridden, the base class simply
	//changes the member.
	if (Active() == newState)
	{
		return (eNoErr);
	}

	if (Active() && Streaming())
	{
		SetStreaming(false);
	}
	return WCMRAudioDevice::SetActive(newState);
}

WTErr WCMRNativeAudioNoneDevice::SetCurrentBufferSize (int newSize)
{

	//changes the status.
	int oldSize = CurrentBufferSize();
	bool oldActive = Active();

	//same size, nothing to do.
	if (oldSize == newSize)
		return eNoErr;
	
	//see if this is one of our supported rates...
	std::vector<int>::iterator intIter = find(m_BufferSizes.begin(), m_BufferSizes.end(), newSize);
	if (intIter == m_BufferSizes.end())
	{
		//Can't change, perhaps use an "invalid param" type of error
		return eCommandLineParameter;
	}
	
	if (Streaming())
	{
		//Can't change, perhaps use an "in use" type of error
		return eGenericErr;
	}

	
	return WCMRAudioDevice::SetCurrentBufferSize(newSize);
}


WTErr WCMRNativeAudioNoneDevice::UpdateDeviceInfo ()
{
	return eNoErr;
}


WTErr WCMRNativeAudioNoneDevice::SetStreaming (bool newState)
{
	if (Streaming() == newState)
	{
		return (eNoErr);
	}

	WCMRAudioDevice::SetStreaming(newState);

	if (Streaming())
	{
		if (is_pthread_active (m_SilenceThread))
			std::cerr << "\t\t\t\t\t !!!!!!!!!!!!!!! Warning: the inactive NONE-DEVICE was streaming!" << std::endl;

		pthread_attr_t attributes;
		size_t stack_size = 100000;
#ifdef __APPLE__
	    stack_size = (((stack_size - 1) / PTHREAD_STACK_MIN) + 1) * PTHREAD_STACK_MIN;
#endif
		if (pthread_attr_init (&attributes)) {
			std::cerr << "WCMRNativeAudioNoneDevice::SetStreaming (): pthread_attr_init () failed!" << std::endl;
			return eGenericErr;
		}
   
		if (pthread_attr_setstacksize (&attributes, stack_size)) {
			std::cerr << "WCMRNativeAudioNoneDevice::SetStreaming (): pthread_attr_setstacksize () failed!" << std::endl;
			return eGenericErr;
		}

		if (pthread_create (&m_SilenceThread, &attributes, __SilenceThread, this)) {
			mark_pthread_inactive (m_SilenceThread);
			std::cerr << "WCMRNativeAudioNoneDevice::SetStreaming (): pthread_create () failed!" << std::endl;
			return eGenericErr;
		}
	}
	else
	{
		if (!is_pthread_active (m_SilenceThread))
		{
			std::cerr << "\t\t\t\t\t !!!!!!!!!!!!!!! Warning: the active NONE-DEVICE was NOT streaming!" << std::endl;
		}

		while (is_pthread_active (m_SilenceThread))
		{
			_usleep(1); //now wait for ended  thread;
		}
	}

	return eNoErr;
}

void WCMRNativeAudioNoneDevice::_SilenceThread()
{
#if defined(PLATFORM_WINDOWS)
	float* theInpBuffers[__m_NumInputChannels];
	for(int i = 0; i < __m_NumInputChannels; ++i)
	{
		theInpBuffers[i] = _m_inputBuffer + m_BufferSizes.back() * i;
	}
#else
	float* theInpBuffers = _m_inputBuffer;
#endif

    uint32_t currentSampleTime = 0;    
	const size_t buffer_size = CurrentBufferSize();
    const uint64_t cyclePeriodNanos = (1000000000.0 * buffer_size) / CurrentSamplingRate();

	struct WCMRAudioDeviceManagerClient::AudioCallbackData audioCallbackData =
	{
		(const float*)theInpBuffers,
		_m_outputBuffer,
		buffer_size,
		0, 
		0
	};

	audioCallbackData.acdCycleStartTimeNanos =__get_time_nanos();

    // VERY ROUGH IMPLEMENTATION: 
    while(Streaming()) {
	
        uint64_t cycleEndTimeNanos = audioCallbackData.acdCycleStartTimeNanos + cyclePeriodNanos;

		m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::AudioCallback, (void *)&audioCallbackData);
		
        currentSampleTime += buffer_size;
		
		int64_t timeToSleepUsecs = ((int64_t)cycleEndTimeNanos - (int64_t)__get_time_nanos())/1000;
		
        if (timeToSleepUsecs > 0) {
            _usleep (timeToSleepUsecs);
        }
		audioCallbackData.acdCycleStartTimeNanos = cycleEndTimeNanos+1;
    }
	mark_pthread_inactive (m_SilenceThread);
}

void* WCMRNativeAudioNoneDevice::__SilenceThread(void *This)
{
	((WCMRNativeAudioNoneDevice*)This)->_SilenceThread();
	return 0;
}

#if defined(PLATFORM_WINDOWS)
void WCMRNativeAudioNoneDevice::_usleep(uint64_t duration_usec)
{ 
    LARGE_INTEGER ft; 

    ft.QuadPart = -(10*duration_usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    SetWaitableTimer(_waitableTimerForUsleep, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(_waitableTimerForUsleep, INFINITE); 
}
#endif

uint64_t
WCMRNativeAudioNoneDevice::__get_time_nanos ()
{
#ifdef __APPLE__
    // here we exploit the time counting API which is used by the WCMRCoreAudioDeviceManager. However,
    // the API should be a part of WCMRCoreAudioDeviceManager to give a chance of being tied to the
    // audio device transport timeﬂ.
    return AudioConvertHostTimeToNanos (AudioGetCurrentHostTime ());
    
#elif PLATFORM_WINDOWS
    
    LARGE_INTEGER Frequency, Count ;

    QueryPerformanceFrequency (&Frequency) ;
    QueryPerformanceCounter (&Count);
    return uint64_t ((Count.QuadPart * 1000000000.0 / Frequency.QuadPart));
#endif
}
