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
//! \file	WCMRNativeAudio.h
//!
//! WCMRNativeAudio and related class declarations
//!
//---------------------------------------------------------------------------------*/
#ifndef __WCMRNativeAudio_h_
	#define __WCMRNativeAudio_h_

#if defined(PLATFORM_WINDOWS)
#include "windows.h"
#endif
#include "pthread.h"
#include "WCRefManager.h"
#include "WCMRAudioDeviceManager.h"

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_9
#include <unistd.h>
#endif

class WCMRNativeAudioDevice; //forward



class WCMRNativeAudioDevice : public WCMRAudioDevice
{
public:

	WCMRNativeAudioDevice (WCMRAudioDeviceManager *pManager, bool useMultithreading = true, bool bNoCopy = false) :
		WCMRAudioDevice (pManager)
		, m_UseMultithreading (useMultithreading)
        , m_bNoCopyAudioBuffer(bNoCopy)
		{}
	virtual ~WCMRNativeAudioDevice () {}

protected:
	bool m_UseMultithreading;
    bool m_bNoCopyAudioBuffer; ///< This flag determines whether the audio callback performs a copy of audio, or the source/sink perform the copy. It should be true to let source/sink do the copies.

};


//! A dummy device to allow apps to choose "None" in case no real device connection is required.
class WCMRNativeAudioNoneDevice : public WCMRNativeAudioDevice
{
public:
    WCMRNativeAudioNoneDevice (WCMRAudioDeviceManager *pManager);
    virtual ~WCMRNativeAudioNoneDevice ();
	virtual WTErr SetActive (bool newState);///<Prepare/Activate device.
	virtual WTErr SetStreaming (bool newState);///<Start/Stop Streaming - should reconnect connections when streaming starts!
	virtual WTErr SetCurrentBufferSize (int newSize);///<Change Current Buffer Size : This is a requset, might not be successful at run time!
	virtual WTErr UpdateDeviceInfo ();

private:

	static void* __SilenceThread(void *This);
	void _SilenceThread();
#if defined(PLATFORM_WINDOWS)
	void _usleep(uint64_t usec);
#else
	inline void _usleep(uint64_t usec) { ::usleep(usec); }
#endif
    static const size_t __m_NumInputChannels = 0;
    static const size_t __m_NumOutputChannels = 0;
	pthread_t m_SilenceThread;
    float *_m_inputBuffer;
    float *_m_outputBuffer;
    static uint64_t __get_time_nanos ();
#if defined (PLATFORM_WINDOWS)
	HANDLE _waitableTimerForUsleep;
#endif
};


#endif //#ifndef __WCMRNativeAudio_h_
