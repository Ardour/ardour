//----------------------------------------------------------------------------------
//
// Copyright (c) 2008 Waves Audio Ltd. All rights reserved.
//
//! \file   WCMRCoreAudioDeviceManager.cpp
//!
//! WCMRCoreAudioDeviceManager and related class declarations
//!
//---------------------------------------------------------------------------------*/
#include "WCMRCoreAudioDeviceManager.h"
#include <CoreServices/CoreServices.h>
#include "MiscUtils/safe_delete.h"
#include <sstream>
#include <syslog.h>

// This flag is turned to 1, but it does not work with aggregated devices.
// due to problems with aggregated devices this flag is not functional there
#define ENABLE_DEVICE_CHANGE_LISTNER 1

#define PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS 10
#define PROPERTY_CHANGE_TIMEOUT_SECONDS 5 
#define USE_IOCYCLE_TIMES 1 ///< Set this to 0 to use individual thread cpu measurement

using namespace wvNS;
///< Supported Sample rates
static const double gAllSampleRates[] =
{
    44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, -1 /* negative terminated  list */
};


///< Default Supported Buffer Sizes.
static const int gAllBufferSizes[] =
{
    32, 64, 96, 128, 192, 256, 512, 1024, 2048, -1 /* negative terminated  list */
};
    

///< The default SR.
static const int DEFAULT_SR = 44100;
///< The default buffer size.
static const int DEFAULT_BUFFERSIZE = 128;

static const int NONE_DEVICE_ID = -1;

///< Number of stalls to wait before notifying user...
static const int NUM_STALLS_FOR_NOTIFICATION = 2 * 50; // 2*50 corresponds to 2 * 50 x 42 ms idle timer - about 4 seconds.
static const int CHANGE_CHECK_COUNTER_PERIOD = 100; // 120 corresponds to 120 x 42 ms idle timer - about 4 seconds.

#define AUHAL_OUTPUT_ELEMENT 0  
#define AUHAL_INPUT_ELEMENT 1

#include <sys/sysctl.h>

static int getProcessorCount() 
{
    int     count = 1;
    size_t size = sizeof(count);

    if (sysctlbyname("hw.ncpu", &count, &size, NULL, 0)) 
        return 1;

    //if something did not work, let's revert to a safe value...
    if (count == 0)
        count = 1;
        
    return count; 
}


//**********************************************************************************************
// WCMRCoreAudioDevice::WCMRCoreAudioDevice 
//
//! Constructor for the audio device. Opens the PA device and gets information about the device.
//!     such as determining supported sampling rates, buffer sizes, and channel counts.
//!
//! \param *pManager : The audio device manager that's managing this device.
//! \param deviceID : The port audio device ID.
//! \param useMultithreading : Whether to use multi-threading for audio processing. Default is true.
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRCoreAudioDevice::WCMRCoreAudioDevice (WCMRCoreAudioDeviceManager *pManager, AudioDeviceID deviceID, bool useMultithreading, bool bNocopy) 
  : WCMRNativeAudioDevice (pManager, useMultithreading, bNocopy)
  , m_SampleCountAtLastIdle (0)
  , m_StalledSampleCounter(0)
  , m_SampleCounter(0)
  , m_BufferSizeChangeRequested (0)
  , m_BufferSizeChangeReported (0)
  , m_ResetRequested (0)
  , m_ResetReported (0)
  , m_ResyncRequested (0)
  , m_ResyncReported (0)
  , m_SRChangeRequested (0)
  , m_SRChangeReported (0)
  , m_ChangeCheckCounter(0)
  , m_IOProcThreadPort (0)
  , m_DropsDetected(0)
  , m_DropsReported(0)
  , m_IgnoreThisDrop(true)
  , m_LastCPULog(0)
#if WV_USE_TONE_GEN
  , m_pToneData(0)
  , m_ToneDataSamples (0)
  , m_NextSampleToUse (0)
#endif //WV_USE_TONE_GEN
{
    AUTO_FUNC_DEBUG;
    UInt32 propSize = 0;
    OSStatus err = kAudioHardwareNoError;

    //Update device info...
    m_DeviceID = deviceID;
    
    m_CurrentSamplingRate = DEFAULT_SR;
    m_CurrentBufferSize = DEFAULT_BUFFERSIZE;
    m_DefaultBufferSize = DEFAULT_BUFFERSIZE;
    m_StopRequested = true;
    m_pInputData = NULL;
    
    m_CPUCount = getProcessorCount();
    m_LastCPULog = wvThread::now() - 10 * wvThread::ktdOneSecond;
    
    

    /*
      @constant       kAudioDevicePropertyNominalSampleRate
      A Float64 that indicates the current nominal sample rate of the AudioDevice.
    */
    Float64 currentNominalRate;
    propSize = sizeof (currentNominalRate);
    err = kAudioHardwareNoError;
    if (AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyNominalSampleRate, &propSize, &currentNominalRate) != kAudioHardwareNoError)
        err = AudioDeviceGetProperty(m_DeviceID, 0, 1, kAudioDevicePropertyNominalSampleRate, &propSize, &currentNominalRate);
        
    if (err == kAudioHardwareNoError)
        m_CurrentSamplingRate = (int)currentNominalRate;
        
    /*
      @constant       kAudioDevicePropertyBufferFrameSize
      A UInt32 whose value indicates the number of frames in the IO buffers.
    */

    UInt32 bufferSize;
    propSize = sizeof (bufferSize);
    err = kAudioHardwareNoError;
    if (AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyBufferFrameSize, &propSize, &bufferSize) != kAudioHardwareNoError)
        err = AudioDeviceGetProperty(m_DeviceID, 0, 1, kAudioDevicePropertyBufferFrameSize, &propSize, &bufferSize);
        
    if (err == kAudioHardwareNoError)
        m_CurrentBufferSize = (int)bufferSize;
    
    
    UpdateDeviceInfo();

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
    
}



//**********************************************************************************************
// WCMRCoreAudioDevice::~WCMRCoreAudioDevice 
//
//! Destructor for the audio device. The base release all the connections that were created, if
//!     they have not been already destroyed! Here we simply stop streaming, and close device
//!     handles if necessary.
//!
//! \param none
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRCoreAudioDevice::~WCMRCoreAudioDevice ()
{
    AUTO_FUNC_DEBUG;

    try
    {
        //If device is streaming, need to stop it!
        if (Streaming())
        {
            SetStreaming (false);
        }
        
        //If device is active (meaning stream is open) we need to close it.
        if (Active())
        {
            SetActive (false);
        }
        
    }
    catch (...)
    {
        //destructors should absorb exceptions, no harm in logging though!!
        DEBUG_MSG ("Exception during destructor");
    }

}


//**********************************************************************************************
// WCMRCoreAudioDevice::UpdateDeviceInfo 
//
//! Updates Device Information about channels, sampling rates, buffer sizes.
//! 
//! \return WTErr.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::UpdateDeviceInfo ()
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;  
    
    // Update all devices parts regardless of errors
    WTErr errName = UpdateDeviceName();
    WTErr errIn =   UpdateDeviceInputs();
    WTErr errOut =  UpdateDeviceOutputs();
    WTErr errSR =   eNoErr; 
    WTErr errBS =   eNoErr; 
    
    errSR = UpdateDeviceSampleRates();
    errBS = UpdateDeviceBufferSizes();

    if(errName != eNoErr || errIn != eNoErr || errOut != eNoErr || errSR != eNoErr || errBS != eNoErr)
    {
        retVal = eCoreAudioFailed;
    }
    
    return retVal;  
}

//**********************************************************************************************
// WCMRCoreAudioDevice::UpdateDeviceName 
//
//! Updates Device name.
//!
//! Use 'kAudioDevicePropertyDeviceName'
//!
//! 1. Get property name size.
//! 2. Get property: name.
//! 
//! \return WTErr.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::UpdateDeviceName()
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;  
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    // Initiate name to unknown.
    m_DeviceName = "Unknown";
    
    //! 1. Get property name size.
    err = AudioDeviceGetPropertyInfo(m_DeviceID, 0, 0, kAudioDevicePropertyDeviceName, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: name.
        char* deviceName = new char[propSize];
        err = AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyDeviceName, &propSize, deviceName);
        if (err == kAudioHardwareNoError)
        {
            m_DeviceName = deviceName;
        }
        else
        {
            retVal = eCoreAudioFailed;
            DEBUG_MSG("Failed to get device name. Device ID: " << m_DeviceID);
        }
        
        delete [] deviceName;
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device name property size. Device ID: " << m_DeviceID);
    }
    
    return retVal;
}

//**********************************************************************************************
// WCMRCoreAudioDevice::UpdateDeviceInputs 
//
//! Updates Device Inputs.
//!
//! Use 'kAudioDevicePropertyStreamConfiguration'
//! This property returns the stream configuration of the device in an
//! AudioBufferList (with the buffer pointers set to NULL) which describes the
//! list of streams and the number of channels in each stream. This corresponds
//! to what will be passed into the IOProc.
//!
//! 1. Get property cannels input size.
//! 2. Get property: cannels input.
//! 3. Update input channels
//! 
//! \return WTErr.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::UpdateDeviceInputs()
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;  
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    int maxInputChannels = 0;
    
    // 1. Get property cannels input size.
    err = AudioDeviceGetPropertyInfo (m_DeviceID, 0, 1/* Input */, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels input.

        // Allocate size according to the property size. Note that this is a variable sized struct...
        AudioBufferList *pStreamBuffers = (AudioBufferList *)malloc(propSize);
        
        if (pStreamBuffers)
        {
            memset (pStreamBuffers, 0, propSize);
        
            // Get the Input channels
            err = AudioDeviceGetProperty (m_DeviceID, 0, true/* Input */, kAudioDevicePropertyStreamConfiguration, &propSize, pStreamBuffers);
            if (err == kAudioHardwareNoError)
            {
                // Calculate the number of input channels
                for (UInt32 streamIndex = 0; streamIndex < pStreamBuffers->mNumberBuffers; streamIndex++)
                {
                    maxInputChannels += pStreamBuffers->mBuffers[streamIndex].mNumberChannels;
                }
            }
            else
            {
                retVal = eCoreAudioFailed;
                DEBUG_MSG("Failed to get device Input channels. Device Name: " << m_DeviceName.c_str());
            }
            
            free (pStreamBuffers);
        }
        else
        {
            retVal = eMemOutOfMemory;
            DEBUG_MSG("Faild to allocate memory. Device Name: " << m_DeviceName.c_str());
        }
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Input channels property size. Device Name: " << m_DeviceName.c_str());
    }
    
    // Update input channels
    m_InputChannels.clear();
    
    for (int channel = 0; channel < maxInputChannels; channel++)
    {
        CFStringRef cfName;
        std::stringstream chNameStream;
        UInt32 nameSize = 0;
        OSStatus error = kAudioHardwareNoError;
        
        error = AudioDeviceGetPropertyInfo (m_DeviceID,
                                            channel + 1,
                                            true /* Input */,
                                            kAudioDevicePropertyChannelNameCFString,
                                            &nameSize,
                                            NULL);
        
        if (error == kAudioHardwareNoError)
        {
            error = AudioDeviceGetProperty (m_DeviceID,
                                            channel + 1,
                                            true /* Input */,
                                            kAudioDevicePropertyChannelNameCFString,
                                            &nameSize,
                                            &cfName);
        }
  
        bool decoded = false;
        char* cstr_name = 0;
        if (error == kAudioHardwareNoError)
        {
            CFIndex length = CFStringGetLength(cfName);
            CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
            cstr_name = new char[maxSize];
            decoded = CFStringGetCString(cfName, cstr_name, maxSize, kCFStringEncodingUTF8);
        }
        
        chNameStream << (channel+1) << " - ";
        
        if (cstr_name && decoded && (0 != std::strlen(cstr_name) ) ) {
            chNameStream << cstr_name;
        }
        else
        {
            chNameStream << "Input " << (channel+1);
        }

        m_InputChannels.push_back (chNameStream.str());
        
        delete [] cstr_name;
    }
    
    return retVal;
}

//**********************************************************************************************
// WCMRCoreAudioDevice::UpdateDeviceOutputs 
//
//! Updates Device Outputs.
//!
//! Use 'kAudioDevicePropertyStreamConfiguration'
//! This property returns the stream configuration of the device in an
//! AudioBufferList (with the buffer pointers set to NULL) which describes the
//! list of streams and the number of channels in each stream. This corresponds
//! to what will be passed into the IOProc.
//!
//! 1. Get property cannels output size.
//! 2. Get property: cannels output.
//! 3. Update output channels
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::UpdateDeviceOutputs()
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;  
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    int maxOutputChannels = 0;
    
    //! 1. Get property cannels output size.
    err = AudioDeviceGetPropertyInfo (m_DeviceID, 0, 0/* Output */, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels output.
        
        // Allocate size according to the property size. Note that this is a variable sized struct...
        AudioBufferList *pStreamBuffers = (AudioBufferList *)malloc(propSize);
        if (pStreamBuffers)
        {
            memset (pStreamBuffers, 0, propSize);
        
            // Get the Output channels
            err = AudioDeviceGetProperty (m_DeviceID, 0, 0/* Output */, kAudioDevicePropertyStreamConfiguration, &propSize, pStreamBuffers);
            if (err == kAudioHardwareNoError)
            {
                // Calculate the number of output channels
                for (UInt32 streamIndex = 0; streamIndex < pStreamBuffers->mNumberBuffers; streamIndex++)
                {
                    maxOutputChannels += pStreamBuffers->mBuffers[streamIndex].mNumberChannels;
                }
            }
            else
            {
                retVal = eCoreAudioFailed;
                DEBUG_MSG("Failed to get device Output channels. Device Name: " << m_DeviceName.c_str());
            }
            free (pStreamBuffers);
        }
        else
        {
            retVal = eMemOutOfMemory;
            DEBUG_MSG("Faild to allocate memory. Device Name: " << m_DeviceName.c_str());
        }
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Output channels property size. Device Name: " << m_DeviceName.c_str());
    }
    
    // Update output channels
    m_OutputChannels.clear();
    for (int channel = 0; channel < maxOutputChannels; channel++)
    {
        CFStringRef cfName;
        std::stringstream chNameStream;
        UInt32 nameSize = 0;
        OSStatus error = kAudioHardwareNoError;
        
        error = AudioDeviceGetPropertyInfo (m_DeviceID,
                                            channel + 1,
                                            false /* Output */,
                                            kAudioDevicePropertyChannelNameCFString,
                                            &nameSize,
                                            NULL);
        
        if (error == kAudioHardwareNoError)
        {
            error = AudioDeviceGetProperty (m_DeviceID,
                                                channel + 1,
                                                false /* Output */,
                                                kAudioDevicePropertyChannelNameCFString,
                                                &nameSize,
                                                &cfName);
        }
        
        bool decoded = false;
        char* cstr_name = 0;
        if (error == kAudioHardwareNoError )
        {
            CFIndex length = CFStringGetLength(cfName);
            CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
            cstr_name = new char[maxSize];
            decoded = CFStringGetCString(cfName, cstr_name, maxSize, kCFStringEncodingUTF8);
        }
        
        chNameStream << (channel+1) << " - ";
        
        if (cstr_name && decoded && (0 != std::strlen(cstr_name) ) ) {
            chNameStream << cstr_name;
        }
        else
        {
            chNameStream << "Output " << (channel+1);
        }
        
        m_OutputChannels.push_back (chNameStream.str());
        
        delete [] cstr_name;
    }
    
    return retVal;
}

//**********************************************************************************************
// WCMRCoreAudioDevice::UpdateDeviceSampleRates 
//
//! Updates Device Sample rates.
//!
//! Use 'kAudioDevicePropertyAvailableNominalSampleRates'
//!
//! 1. Get sample rate property size.
//! 2. Get property: sample rates.
//! 3. Update sample rates
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::UpdateDeviceSampleRates()
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;  
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    m_SamplingRates.clear();
    
    //! 1. Get sample rate property size.
    err = AudioDeviceGetPropertyInfo(m_DeviceID, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels output.
        
        // Allocate size accrding to the number of audio values
        int numRates = propSize / sizeof(AudioValueRange);
        AudioValueRange* supportedRates = new AudioValueRange[numRates];
        
        // Get sampling rates from Audio device
        err = AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &propSize, supportedRates);
        if (err == kAudioHardwareNoError)
        {
            //! 3. Update sample rates
            
            // now iterate through our standard SRs
            for(int ourSR=0; gAllSampleRates[ourSR] > 0; ourSR++)
            {
                //check to see if our SR is in the supported rates...
                for (int deviceSR = 0; deviceSR < numRates; deviceSR++)
                {
                    if ((supportedRates[deviceSR].mMinimum <= gAllSampleRates[ourSR]) && 
                        (supportedRates[deviceSR].mMaximum >= gAllSampleRates[ourSR]))
                    {
                        m_SamplingRates.push_back ((int)gAllSampleRates[ourSR]);
                        break;
                    }
                }
            }
        }
        else
        {
            retVal = eCoreAudioFailed;
            DEBUG_MSG("Failed to get device Sample rates. Device Name: " << m_DeviceName.c_str());
        }
        
        delete [] supportedRates;
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Sample rates property size. Device Name: " << m_DeviceName.c_str());
    }
    
    return retVal;
}


//**********************************************************************************************
// WCMRCoreAudioDevice::UpdateDeviceBufferSizes_Simple 
//
// Use kAudioDevicePropertyBufferFrameSizeRange
//
// in case of 'eMatchedDuplexDevices' and a matching device exists return common device name
// in all other cases retur base class function implementation
//
// 1. Get buffer size range
// 2. Run on all ranges and add them to the list
// 
// \return error code
// 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::UpdateDeviceBufferSizes ()
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    // Clear buffer sizes
    m_BufferSizes.clear();
    
    // 1. Get buffer size range
    AudioValueRange bufferSizesRange;
    propSize = sizeof (AudioValueRange);
    err = AudioDeviceGetProperty (m_DeviceID, 0, 0, kAudioDevicePropertyBufferFrameSizeRange, &propSize, &bufferSizesRange);
    if(err == kAudioHardwareNoError)
    {
        // 2. Run on all ranges and add them to the list
        for(int bsize=0; gAllBufferSizes[bsize] > 0; bsize++)
        {
            if ((bufferSizesRange.mMinimum <= gAllBufferSizes[bsize]) && (bufferSizesRange.mMaximum >= gAllBufferSizes[bsize]))
            {
                m_BufferSizes.push_back (gAllBufferSizes[bsize]);
            }
        }
        
        //if we didn't get a single hit, let's simply add the min. and the max...
        if (m_BufferSizes.empty())
        {
            m_BufferSizes.push_back ((int)bufferSizesRange.mMinimum);
            m_BufferSizes.push_back ((int)bufferSizesRange.mMaximum);
        }
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device buffer sizes range. Device Name: " << m_DeviceName.c_str());
    }
    
    return retVal;
}


//**********************************************************************************************
// WCMRCoreAudioDevice::DeviceName 
//
//! in case of 'eMatchedDuplexDevices' and a matching device exists return common device name
//! in all other cases retur base class function implementation
//!
//! \param none
//! 
//! \return current device name
//! 
//**********************************************************************************************
const std::string& WCMRCoreAudioDevice::DeviceName() const
{
    return WCMRAudioDevice::DeviceName();
}

//**********************************************************************************************
// WCMRCoreAudioDevice::InputChannels 
//
//! return base class function implementation
//!
//! \param none
//! 
//! \return base class function implementation
//! 
//**********************************************************************************************
const std::vector<std::string>& WCMRCoreAudioDevice::InputChannels()
{
    return WCMRAudioDevice::InputChannels();
}

//**********************************************************************************************
// WCMRCoreAudioDevice::OutputChannels 
//
//! in case of 'eMatchedDuplexDevices' return matching device output channel if there is one
//! in all other cases retur base class function implementation
//!
//! \param none
//! 
//! \return list of output channels of current device
//! 
//**********************************************************************************************
const std::vector<std::string>& WCMRCoreAudioDevice::OutputChannels()
{
    return WCMRAudioDevice::OutputChannels();
}


//**********************************************************************************************
// WCMRCoreAudioDevice::SamplingRates 
//
//! in case of 'eMatchedDuplexDevices' and a matching device exists return common sample rate
//! in all other cases retur base class function implementation
//!
//! \param none
//! 
//! \return current sample rate
//! 
//**********************************************************************************************
const std::vector<int>& WCMRCoreAudioDevice::SamplingRates()
{
    return WCMRAudioDevice::SamplingRates();
}

//**********************************************************************************************
// WCMRCoreAudioDevice::CurrentSamplingRate 
//
//! The device's current sampling rate. This may be overridden, if the device needs to 
//!     query the driver for the current rate.
//!
//! \param none
//! 
//! \return The device's current sampling rate. -1 on error.
//! 
//**********************************************************************************************
int WCMRCoreAudioDevice::CurrentSamplingRate ()
{
    AUTO_FUNC_DEBUG;
    //ToDo: Perhaps for ASIO devices that are active, we should retrive the SR from the device...
    UInt32 propSize = 0;
    OSStatus err = kAudioHardwareNoError;

    Float64 currentNominalRate;
    propSize = sizeof (currentNominalRate);
    err = kAudioHardwareNoError;
    if (AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyNominalSampleRate, &propSize, &currentNominalRate) != kAudioHardwareNoError)
        err = AudioDeviceGetProperty(m_DeviceID, 0, 1, kAudioDevicePropertyNominalSampleRate, &propSize, &currentNominalRate);
        
    if (err == kAudioHardwareNoError)
        m_CurrentSamplingRate = (int)currentNominalRate;
    else
    {
        DEBUG_MSG("Unable to get sampling rate!");
    }

    return (m_CurrentSamplingRate);
}




//**********************************************************************************************
// WCMRCoreAudioDevice::SetCurrentSamplingRate 
//
//! Change the sampling rate to be used by the device. 
//!
//! \param newRate : The rate to use (samples per sec).
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetCurrentSamplingRate (int newRate)
{
    AUTO_FUNC_DEBUG;
    std::vector<int>::iterator intIter;
    WTErr retVal = eNoErr;

    //changes the status.
    int oldRate = CurrentSamplingRate();
    bool oldActive = Active();
    
    //no change, nothing to do
    if (oldRate == newRate)
        goto Exit;

    //see if this is one of our supported rates...
    intIter = find(m_SamplingRates.begin(), m_SamplingRates.end(), newRate);
    if (intIter == m_SamplingRates.end())
    {
        //Can't change, perhaps use an "invalid param" type of error
        retVal = eCommandLineParameter;
        goto Exit;
    }
    
    if (Streaming())
    {
        //Can't change, perhaps use an "in use" type of error
        retVal = eGenericErr;
        goto Exit;
    }

    if (oldActive)
    {
        //Deactivate it for the change...
        SetActive (false);
    }
    
    retVal = SetAndCheckCurrentSamplingRate (newRate);
    if(retVal == eNoErr)
    {
        retVal = UpdateDeviceInfo ();
    }

    //reactivate it.    
    if (oldActive)
    {
        retVal = SetActive (true);
    }
    
Exit:

    return (retVal);
        
}

//**********************************************************************************************
// WCMRCoreAudioDevice::SetAndCheckCurrentSamplingRate 
//
//! Change the sampling rate to be used by the device. 
//!
//! \param newRate : The rate to use (samples per sec).
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetAndCheckCurrentSamplingRate (int newRate)
{
    AUTO_FUNC_DEBUG;
    std::vector<int>::iterator intIter;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    // 1. Set new sampling rate
    Float64 newNominalRate = newRate;
    propSize = sizeof (Float64);
    err = AudioDeviceSetProperty(m_DeviceID, NULL, 0, 0, kAudioDevicePropertyNominalSampleRate, propSize, &newNominalRate);
    
    m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Changed the Sampling Rate.");
    
    if (err != kAudioHardwareNoError)
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG ("Unable to set SR! Device name: " << m_DeviceName.c_str());
    }
    else
    {
        // 2. wait for the SR to actually change...
        
        // Set total time out time
        int tryAgain = ((PROPERTY_CHANGE_TIMEOUT_SECONDS * 1000) / PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS) ;
        int actualWait = 0;
        Float64 actualSamplingRate = 0.0;
        
        // Run as ling as time out is not finished
        while (tryAgain)
        {
            // Get current sampling rate
            err = AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyNominalSampleRate, &propSize, &actualSamplingRate);
            if (err == kAudioHardwareNoError)
            {
                if (actualSamplingRate == newNominalRate)
                {
                    //success, let's get out!
                    break;
                }
            }
            else
            {
                //error reading rate, but let's not complain too much!
                m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Could not read Sampling Rate for verification.");
                DEBUG_MSG ("Unable to get SR. Device name: " << m_DeviceName.c_str());
            }
            
            // oh well...there's always another millisecond...
            wvThread::sleep_milliseconds (PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS);
            tryAgain--;
            actualWait++;
        }
        
        // If sample rate actually changed
        if (tryAgain != 0)
        {
            // Update member with new rate
            m_CurrentSamplingRate = newRate;
            
            char debugMsg[128];
            snprintf (debugMsg, sizeof(debugMsg), "Actual Wait for SR Change was %d milliseconds", actualWait * PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS);
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)debugMsg);
        }
        // If sample rate did not change after time out
        else
        {
            // Update member with last read value
            m_CurrentSamplingRate = static_cast<int>(actualSamplingRate);
            
            char debugMsg[128];
            snprintf (debugMsg, sizeof(debugMsg), "Unable to change SR, even after waiting for %d milliseconds", actualWait * PROPERTY_CHANGE_SLEEP_TIME_MILLISECONDS);
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)debugMsg);
        }
    }
    
    return (retVal);
}


//**********************************************************************************************
// WCMRCoreAudioDevice::BufferSizes 
//
//! in case of 'eMatchedDuplexDevices' and a matching device exists return common buffer sizes
//! in all other cases retur base class function implementation
//!
//! \param none
//! 
//! \return current sample rate
//! 
//**********************************************************************************************
const std::vector<int>& WCMRCoreAudioDevice::BufferSizes()
{
    return WCMRAudioDevice::BufferSizes();
}


//**********************************************************************************************
// WCMRCoreAudioDevice::CurrentBufferSize
//
//! The device's current buffer size in use. This may be overridden, if the device needs to 
//!     query the driver for the current size.
//!
//! \param none
//! 
//! \return The device's current buffer size. 0 on error.
//! 
//**********************************************************************************************
int WCMRCoreAudioDevice::CurrentBufferSize ()
{
    AUTO_FUNC_DEBUG;

    return (m_CurrentBufferSize);
}



//**********************************************************************************************
// WCMRCoreAudioDevice::SetCurrentBufferSize
//
//! Change the buffer size to be used by the device. This will most likely be overridden, 
//!     the base class simply updates the member variable.
//!
//! \param newSize : The buffer size to use (in sample-frames)
//! 
//! \return eNoErr always. The derived classes may return error codes.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetCurrentBufferSize (int newSize)
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    std::vector<int>::iterator intIter;

    //changes the status.
    int oldSize = CurrentBufferSize();
    bool oldActive = Active();

    //same size, nothing to do.
    if (oldSize == newSize)
        goto Exit;
    
    if (Streaming())
    {
        //Can't change, perhaps use an "in use" type of error
        retVal = eGenericErr;
        goto Exit;
    }
    
    if (oldActive)
    {
        //Deactivate it for the change...
        SetActive (false);
    }
    
    // when audio device is inactive it is safe to set a working buffer size according to new buffer size
    // if 'newSize' is not a valid buffer size, another valid buffer size will be set
    retVal = SetWorkingBufferSize(newSize);
    if(retVal != eNoErr)
    {
        DEBUG_MSG("Unable to set a working buffer size. Device Name: " << DeviceName().c_str());
        goto Exit;
    }

    //reactivate it.    
    if (oldActive)
    {
        retVal = SetActive (true);
        if(retVal != eNoErr)
        {
            DEBUG_MSG("Unable to activate device. Device Name: " << DeviceName().c_str());
            goto Exit;
        }
    }
    
Exit:
    
    return (retVal);
}

WTErr WCMRCoreAudioDevice::SetWorkingBufferSize(int newSize)
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    
    // 1. Set new buffer size
    err = SetBufferSizesByIO(newSize);
    
    // If there's no error it means this buffer size is supported
    if(err == kAudioHardwareNoError)
    {
        m_CurrentBufferSize = newSize;
    }
    // If there was an error it means that this buffer size was not supported
    else
    {
        // In case the new buffer size could not be set, set another working buffer size

        // Run on all buffer sizes:
        
        // Try setting buffer sizes that are bigger then selected buffer size first,
        // Since bigger buffer sizes usually work safer 
        for(std::vector<int>::const_iterator iter = m_BufferSizes.begin();iter != m_BufferSizes.end();++iter)
        {
            int nCurBS = *iter;
            
            if(nCurBS > newSize)
            {
                // Try setting current buffer size
                err = SetBufferSizesByIO(nCurBS);
                
                // in case buffer size is valid
                if(err == kAudioHardwareNoError)
                {
                    // Set current buffer size
                    m_CurrentBufferSize = nCurBS;
                    break;
                }
            }
        }
        
        // If bigger buffer sizes failed, go to smaller buffer sizes
        if(err != kAudioHardwareNoError)
        {
            for(std::vector<int>::const_iterator iter = m_BufferSizes.begin();iter != m_BufferSizes.end();++iter)
            {
                int nCurBS = *iter;
                
                if(nCurBS < newSize)
                {
                    // Try setting current buffer size
                    err = SetBufferSizesByIO(*iter);
                    
                    // in case buffer size is valid
                    if(err == kAudioHardwareNoError)
                    {
                        // Set current buffer size
                        m_CurrentBufferSize = *iter;
                        break;
                    }
                }
            }
        }
        
        // Check if a valid buffer size was found
        if(err == kAudioHardwareNoError)
        {
            // Notify that a different sample rate is set
            char debugMsg[256];
            snprintf (debugMsg, sizeof(debugMsg), "Could not set buffer size: %d, Set buffer size to: %d.", newSize, m_CurrentBufferSize);
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)debugMsg);
        }
        // if there was no buffer size that could be set
        else
        {
            // Set the parameter buffer size by default, set a debug message
            m_CurrentBufferSize = newSize;
            DEBUG_MSG("Unable to set any buffer size. Device Name: " << m_DeviceName.c_str());
        }
    }
    
    return retVal;
}

OSStatus WCMRCoreAudioDevice::SetBufferSizesByIO(int newSize)
{
    OSStatus err = kAudioHardwareNoError;
    
    // 1. Set new buffer size
    UInt32 bufferSize = (UInt32)newSize;
    UInt32 propSize = sizeof (UInt32);
    
    // Set new buffer size to input
    if (!m_InputChannels.empty())
    {
        err = AudioDeviceSetProperty(m_DeviceID, NULL, 0, 1, kAudioDevicePropertyBufferFrameSize, propSize, &bufferSize);
    }
    else
    {
        err = AudioDeviceSetProperty(m_DeviceID, NULL, 0, 0, kAudioDevicePropertyBufferFrameSize, propSize, &bufferSize);
    }
    
    return err;
}

//**********************************************************************************************
// WCMRCoreAudioDevice::ConnectionStatus 
//
//! Retrieves the device's current connection status. This will most likely be overridden,
//!     in case some driver communication is required to query the status.
//!
//! \param none
//! 
//! \return A ConnectionStates value.
//! 
//**********************************************************************************************
WCMRCoreAudioDevice::ConnectionStates WCMRCoreAudioDevice::ConnectionStatus ()
{
    AUTO_FUNC_DEBUG;
    //ToDo: May want to do something more to extract the actual status!
    return (m_ConnectionStatus);
    
}


//**********************************************************************************************
// WCMRCoreAudioDevice::EnableAudioUnitIO
//
//! Sets up the AUHAL for IO, allowing changes to the devices to be used by the AudioUnit.
//!
//! \param none
//! 
//! \return eNoErr on success, an error code on failure.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::EnableAudioUnitIO()
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    
    UInt32 enableIO = 1;
    if (!m_InputChannels.empty())
    {
        ///////////////
        //ENABLE IO (INPUT)
        //You must enable the Audio Unit (AUHAL) for input 
        
        //Enable input on the AUHAL
        err =  AudioUnitSetProperty(m_AUHALAudioUnit, 
                                    kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input,
                                    AUHAL_INPUT_ELEMENT,
                                    &enableIO, sizeof(enableIO));

        if (err)
        {
            DEBUG_MSG("Couldn't Enable IO on input scope of input element, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
    }
    
    //disable Output on the AUHAL if there's no output
    if (m_OutputChannels.empty())
        enableIO = 0;
    else
        enableIO = 1;
        
    err = AudioUnitSetProperty(m_AUHALAudioUnit,
                               kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                               AUHAL_OUTPUT_ELEMENT,
                               &enableIO, sizeof(enableIO));
        
    if (err)
    {
        DEBUG_MSG("Couldn't Enable/Disable IO on output scope of output element, error = " << err);
        retVal = eGenericErr;
        goto Exit;
    }

Exit:
    return retVal;
}


//**********************************************************************************************
// WCMRCoreAudioDevice::EnableListeners
//
//! Sets up listeners to listen for Audio Device property changes, so that app can be notified.
//!
//! \param none
//! 
//! \return eNoErr on success, an error code on failure.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::EnableListeners()
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;

    //listner for SR change...
    err = AudioDeviceAddPropertyListener(m_DeviceID, 0, 0, kAudioDevicePropertyNominalSampleRate,
                                         StaticPropertyChangeProc, this);
    
    if (err)
    {
        DEBUG_MSG("Couldn't Setup SR Property Listner, error = " << err);
        retVal = eGenericErr;
        goto Exit;
    }

#if ENABLE_DEVICE_CHANGE_LISTNER
    {
        //listner for device change...
        
        err = AudioDeviceAddPropertyListener (m_DeviceID,
                                              kAudioPropertyWildcardChannel,
                                              true,
                                              kAudioDevicePropertyDeviceHasChanged,
                                              StaticPropertyChangeProc,
                                              this);
                
        if (err)
        {
            DEBUG_MSG("Couldn't Setup device change Property Listner, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
    }
#endif //ENABLE_DEVICE_CHANGE_LISTNER   
    
    //listner for dropouts...
    err = AudioDeviceAddPropertyListener(m_DeviceID, 0, 0, kAudioDeviceProcessorOverload,
                                         StaticPropertyChangeProc, this);
        
    if (err)
    {
        DEBUG_MSG("Couldn't Setup Processor Overload Property Listner, error = " << err);
        retVal = eGenericErr;
        goto Exit;
    }
    

Exit:   
    return retVal;
}



//**********************************************************************************************
// WCMRCoreAudioDevice::DisableListeners
//
//! Undoes the work done by EnableListeners
//!
//! \param none
//! 
//! \return eNoErr on success, an error code on failure.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::DisableListeners()
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;

    //listner for SR change...
    err = AudioDeviceRemovePropertyListener(m_DeviceID, 0, 0, kAudioDevicePropertyNominalSampleRate,
                                            StaticPropertyChangeProc);
        
    if (err)
    {
        DEBUG_MSG("Couldn't Cleanup SR Property Listner, error = " << err);
        //not sure if we need to report this...
    }

#if ENABLE_DEVICE_CHANGE_LISTNER    
    {
        err = AudioDeviceRemovePropertyListener (m_DeviceID,
                                                 kAudioPropertyWildcardChannel,
                                                 true/* Input */,
                                                 kAudioDevicePropertyDeviceHasChanged,
                                                 StaticPropertyChangeProc);
        
        if (err)
        {
            DEBUG_MSG("Couldn't Cleanup device input stream change Property Listner, error = " << err);
            //not sure if we need to report this...
        }
        
    }
#endif //ENABLE_DEVICE_CHANGE_LISTNER   

    err = AudioDeviceRemovePropertyListener(m_DeviceID, 0, 0, kAudioDeviceProcessorOverload,
                                            StaticPropertyChangeProc);
        
    if (err)
    {
        DEBUG_MSG("Couldn't Cleanup device change Property Listner, error = " << err);
        //not sure if we need to report this...
    }
    

    return retVal;
}


//**********************************************************************************************
// WCMRCoreAudioDevice::StaticPropertyChangeProc
//
//! The property change function called (as a result of EnableListeners) when device properties change.
//!     It calls upon the non-static PropertyChangeProc to do the work.
//!
//! \param inDevice : The audio device in question.
//! \param inChannel : The channel on which the property has change.
//! \param isInput : If the change is for Input.
//! \param inPropertyID : The property that has changed.
//! \param inClientData: What was passed when listener was enabled, in our case teh WCMRCoreAudioDevice object.
//! 
//! \return 0 always.
//! 
//**********************************************************************************************
OSStatus WCMRCoreAudioDevice::StaticPropertyChangeProc (AudioDeviceID /*inDevice*/, UInt32 /*inChannel*/, Boolean /*isInput*/,
                                                        AudioDevicePropertyID inPropertyID, void *inClientData)
{
    if (inClientData)
    {
        WCMRCoreAudioDevice* pCoreDevice = (WCMRCoreAudioDevice *)inClientData;
        pCoreDevice->PropertyChangeProc (inPropertyID);
    }
        
    return 0;
}



//**********************************************************************************************
// WCMRCoreAudioDevice::PropertyChangeProc
//
//! The non-static property change proc. Gets called when properties change. Since this gets called
//!     on an arbitrary thread, we simply update the request counters and return.
//!
//! \param none
//! 
//! \return nothing.
//! 
//**********************************************************************************************
void WCMRCoreAudioDevice::PropertyChangeProc (AudioDevicePropertyID inPropertyID)
{
    switch (inPropertyID)
    {
    case kAudioDevicePropertyNominalSampleRate:
        m_SRChangeRequested++;
        break;
#if ENABLE_DEVICE_CHANGE_LISTNER    
    case kAudioDevicePropertyDeviceHasChanged:
        {
            m_ResetRequested++;
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
        }
        break;
#endif //ENABLE_DEVICE_CHANGE_LISTNER   
    case kAudioDeviceProcessorOverload:
        {
        if (m_IgnoreThisDrop)
            m_IgnoreThisDrop = false; //We'll ignore once, just once!
        else
            m_DropsDetected++;
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::Dropout );
        break;
        }
    default:
        break;
    }
}


//**********************************************************************************************
// WCMRCoreAudioDevice::SetupAUHAL
//
//! Sets up the AUHAL AudioUnit for device IO.
//!
//! \param none
//! 
//! \return eNoErr on success, an error code on failure.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetupAUHAL()
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    Component comp;
    ComponentDescription desc;
    AudioStreamBasicDescription streamFormatToUse, auhalStreamFormat;

    //There are several different types of Audio Units.
    //Some audio units serve as Outputs, Mixers, or DSP
    //units. See AUComponent.h for listing
    desc.componentType = kAudioUnitType_Output;
    
    //Every Component has a subType, which will give a clearer picture
    //of what this components function will be.
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    
    //all Audio Units in AUComponent.h must use 
    //"kAudioUnitManufacturer_Apple" as the Manufacturer
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    //Finds a component that meets the desc spec's
    comp = FindNextComponent(NULL, &desc);
    if (comp == NULL)
    {
        DEBUG_MSG("Couldn't find AUHAL Component");
        retVal = eGenericErr;
        goto Exit;
    }
    
    //gains access to the services provided by the component
    OpenAComponent(comp, &m_AUHALAudioUnit);  

    
    retVal = EnableAudioUnitIO();
    if (retVal != eNoErr)
        goto Exit;

    //Now setup the device to use by the audio unit...
    
    //input
    if (!m_InputChannels.empty())
    {
        err = AudioUnitSetProperty(m_AUHALAudioUnit, kAudioOutputUnitProperty_CurrentDevice,
                                   kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT,
                                   &m_DeviceID, sizeof(m_DeviceID));

        if (err)
        {
            DEBUG_MSG("Couldn't Set the audio device property for Input Element Global scope, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
    }

    //output
    if (!m_OutputChannels.empty())
    {
        err = AudioUnitSetProperty(m_AUHALAudioUnit, kAudioOutputUnitProperty_CurrentDevice,
                                   kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT,
                                   &m_DeviceID, sizeof(m_DeviceID));

        if (err)
        {
            DEBUG_MSG("Couldn't Set the audio device property for Output Element Global scope, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
    }
    
    //also set Sample Rate...
    {
        retVal = SetAndCheckCurrentSamplingRate(m_CurrentSamplingRate);
        if(retVal != eNoErr)
        {
            DEBUG_MSG ("Unable to set SR, error = " << err);
            goto Exit;
        }
    }

    //now set the buffer size...
    {
        err = SetWorkingBufferSize(m_CurrentBufferSize);
        if (err)
        {
            DEBUG_MSG("Couldn't Set the buffer size property, error = " << err);
            //we don't really quit here..., just keep going even if this does not work,
            //the AUHAL is supposed to take care of this by way of slicing...
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Could not set buffer size.");
            
        }
    }
    
    //convertor quality
    {
        UInt32 quality = kAudioConverterQuality_Max;
        propSize = sizeof (quality);
        err = AudioUnitSetProperty(m_AUHALAudioUnit,
                                   kAudioUnitProperty_RenderQuality, kAudioUnitScope_Global,
                                   AUHAL_OUTPUT_ELEMENT,
                                   &quality, sizeof (quality));
            
        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to set Convertor Quality, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
    }
    
    memset (&auhalStreamFormat, 0, sizeof (auhalStreamFormat));
    propSize = sizeof (auhalStreamFormat);
    err = AudioUnitGetProperty(m_AUHALAudioUnit,
                               kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                               AUHAL_INPUT_ELEMENT,
                               &auhalStreamFormat, &propSize);
    if (err != kAudioHardwareNoError)
    {
        DEBUG_MSG ("Unable to get Input format, error = " << err);
        retVal = eGenericErr;
        goto Exit;
    }
    
    if (auhalStreamFormat.mSampleRate != (Float64)m_CurrentSamplingRate)
    {
        TRACE_MSG ("AUHAL's Input SR differs from expected SR, expected = " << m_CurrentSamplingRate << ", AUHAL's = " << (UInt32)auhalStreamFormat.mSampleRate);
    }
    
    //format, and slice size...
    memset (&streamFormatToUse, 0, sizeof (streamFormatToUse));
    streamFormatToUse.mFormatID = kAudioFormatLinearPCM;
    streamFormatToUse.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    streamFormatToUse.mFramesPerPacket = 1;
    streamFormatToUse.mBitsPerChannel = sizeof (float) * 8;
    streamFormatToUse.mSampleRate = auhalStreamFormat.mSampleRate;

    if (!m_InputChannels.empty())
    {
        streamFormatToUse.mChannelsPerFrame = m_InputChannels.size();
        streamFormatToUse.mBytesPerFrame = sizeof (float)*streamFormatToUse.mChannelsPerFrame;
        streamFormatToUse.mBytesPerPacket = streamFormatToUse.mBytesPerFrame;
        propSize = sizeof (streamFormatToUse);
        err = AudioUnitSetProperty(m_AUHALAudioUnit,
                                   kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                   AUHAL_INPUT_ELEMENT,
                                   &streamFormatToUse, sizeof (streamFormatToUse));

        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to set Input format, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
        
        UInt32 bufferSize = m_CurrentBufferSize;
        err = AudioUnitSetProperty(m_AUHALAudioUnit,
                                   kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Output,
                                   AUHAL_INPUT_ELEMENT,
                                   &bufferSize, sizeof (bufferSize));

        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to set Input frames, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
        
    }

    if (!m_OutputChannels.empty())
    {
        err = AudioUnitGetProperty(m_AUHALAudioUnit,
                                   kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                   AUHAL_OUTPUT_ELEMENT,
                                   &auhalStreamFormat, &propSize);
        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to get Output format, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
        
        if (auhalStreamFormat.mSampleRate != (Float64)m_CurrentSamplingRate)
        {
            TRACE_MSG ("AUHAL's Output SR differs from expected SR, expected = " << m_CurrentSamplingRate << ", AUHAL's = " << (UInt32)auhalStreamFormat.mSampleRate);
        }
        
        
        streamFormatToUse.mChannelsPerFrame = m_OutputChannels.size();
        streamFormatToUse.mBytesPerFrame = sizeof (float)*streamFormatToUse.mChannelsPerFrame;
        streamFormatToUse.mBytesPerPacket = streamFormatToUse.mBytesPerFrame;
        streamFormatToUse.mSampleRate = auhalStreamFormat.mSampleRate;
        propSize = sizeof (streamFormatToUse);
        err = AudioUnitSetProperty(m_AUHALAudioUnit,
                                   kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                   AUHAL_OUTPUT_ELEMENT,
                                   &streamFormatToUse, sizeof (streamFormatToUse));

        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to set Output format, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }

        UInt32 bufferSize = m_CurrentBufferSize;
        err = AudioUnitSetProperty(m_AUHALAudioUnit,
                                   kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Input,
                                   AUHAL_OUTPUT_ELEMENT,
                                   &bufferSize, sizeof (bufferSize));

        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to set Output frames, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }

    }
    
    //setup callback (IOProc)
    {
        AURenderCallbackStruct renderCallback;
        memset (&renderCallback, 0, sizeof (renderCallback));
        propSize = sizeof (renderCallback);
        renderCallback.inputProc = StaticAudioIOProc;
        renderCallback.inputProcRefCon = this;
        
        err = AudioUnitSetProperty(m_AUHALAudioUnit,
                                   (m_OutputChannels.empty() ? (AudioUnitPropertyID)kAudioOutputUnitProperty_SetInputCallback : (AudioUnitPropertyID)kAudioUnitProperty_SetRenderCallback),
                                   kAudioUnitScope_Output,
                                   m_OutputChannels.empty() ? AUHAL_INPUT_ELEMENT : AUHAL_OUTPUT_ELEMENT,
                                   &renderCallback, sizeof (renderCallback));
            
        if (err != kAudioHardwareNoError)
        {
            DEBUG_MSG ("Unable to set callback, error = " << err);
            retVal = eGenericErr;
            goto Exit;
        }
    }

    retVal = EnableListeners();
    if (retVal != eNoErr)
        goto Exit;
    
    //initialize the audio-unit now!
    err = AudioUnitInitialize(m_AUHALAudioUnit);
    if (err != kAudioHardwareNoError)
    {
        DEBUG_MSG ("Unable to Initialize AudioUnit = " << err);
        retVal = eGenericErr;
        goto Exit;
    }
    
Exit:
    if (retVal != eNoErr)
        TearDownAUHAL();
        
    return retVal;
}



//**********************************************************************************************
// WCMRCoreAudioDevice::TearDownAUHAL
//
//! Undoes the work done by SetupAUHAL
//!
//! \param none
//! 
//! \return eNoErr on success, an error code on failure.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::TearDownAUHAL()
{
    WTErr retVal = eNoErr;

    if (m_AUHALAudioUnit)
    {
        DisableListeners ();
        AudioUnitUninitialize(m_AUHALAudioUnit);
        CloseComponent(m_AUHALAudioUnit);
        m_AUHALAudioUnit = NULL;
    }

    return retVal;
}



//**********************************************************************************************
// WCMRCoreAudioDevice::SetActive 
//
//! Sets the device's activation status. Essentially, opens or closes the PA device. 
//!     If it's an ASIO device it may result in buffer size change in some cases.
//!
//! \param newState : Should be true to activate, false to deactivate. This roughly corresponds
//!     to opening and closing the device handle/stream/audio unit.
//! 
//! \return eNoErr on success, an error code otherwise.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetActive (bool newState)
{
    AUTO_FUNC_DEBUG;

    WTErr retVal = eNoErr;
    
    if (Active() == newState)
        goto Exit;


    if (newState)
    {
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Setting up AUHAL.");
        retVal = SetupAUHAL();

        if (retVal != eNoErr)
            goto Exit;

        m_BufferSizeChangeRequested = 0;
        m_BufferSizeChangeReported = 0;
        m_ResetRequested = 0;
        m_ResetReported = 0;
        m_ResyncRequested = 0;
        m_ResyncReported = 0;
        m_SRChangeRequested = 0;
        m_SRChangeReported = 0;
        m_DropsDetected = 0;
        m_DropsReported = 0;
        m_IgnoreThisDrop = true;
    }
    else
    {
        if (Streaming())
        {
            SetStreaming (false);
        }

        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Tearing down AUHAL.");
        retVal = TearDownAUHAL();
        if (retVal != eNoErr)
            goto Exit;

        m_BufferSizeChangeRequested = 0;
        m_BufferSizeChangeReported = 0;
        m_ResetRequested = 0;
        m_ResetReported = 0;
        m_ResyncRequested = 0;
        m_ResyncReported = 0;
        m_SRChangeRequested = 0;
        m_SRChangeReported = 0;
        m_DropsDetected = 0;
        m_DropsReported = 0;
        m_IgnoreThisDrop = true;

        UpdateDeviceInfo();

    }
    
    m_IsActive = newState;
    
Exit:   
    return (retVal);
}


#if WV_USE_TONE_GEN
//**********************************************************************************************
// WCMRCoreAudioDevice::SetupToneGenerator
//
//! Sets up the Tone generator - only if a file /tmp/tonegen.txt is present. If the file is
//!     present, it reads the value in the file and uses that as the frequency for the tone. This
//!     code attempts to create an array of samples that would constitute an integral number of
//!     cycles - for the currently active sampling rate. If tonegen is active, then the input
//!     from the audio device is ignored, instead a data is supplied from the tone generator's
//!     array - for all channels. The array is in m_pToneData, the size of the array is in
//!     m_ToneDataSamples, and m_NextSampleToUse holds the index in the array from where
//!     the next sample is going to be taken.
//!
//!
//! \return : Nothing
//!
//**********************************************************************************************
void WCMRCoreAudioDevice::SetupToneGenerator ()
{
    safe_delete_array(m_pToneData);
    m_ToneDataSamples = 0;

    //if tonegen exists?
    FILE *toneGenHandle = fopen ("/tmp/tonegen.txt", "r");
    if (toneGenHandle)
    {
        int toneFreq = 0;
        fscanf(toneGenHandle, "%d", &toneFreq);
        if ((toneFreq <= 0) || (toneFreq > (m_CurrentSamplingRate/2)))
        {
            toneFreq = 1000;    
        }
        
        
        m_ToneDataSamples = m_CurrentSamplingRate / toneFreq;
        int toneDataSamplesFrac = m_CurrentSamplingRate % m_ToneDataSamples;
        int powerOfTen = 1;
        while (toneDataSamplesFrac)
        {
            m_ToneDataSamples = (uint32_t)((pow(10, powerOfTen) * m_CurrentSamplingRate) / toneFreq);
            toneDataSamplesFrac = m_CurrentSamplingRate % m_ToneDataSamples;
            powerOfTen++;
        }
        
        //allocate
        m_pToneData = new float_t[m_ToneDataSamples];
        
        //fill with a -6dB Sine Tone
        uint32_t numSamplesLeft = m_ToneDataSamples;
        float_t *pNextSample = m_pToneData;
        double phase = 0;
        double phaseIncrement = (M_PI * 2.0 * toneFreq ) / ((double)m_CurrentSamplingRate);
        while (numSamplesLeft)
        {
            *pNextSample = (float_t)(0.5 * sin(phase));
            phase += phaseIncrement;
            pNextSample++;
            numSamplesLeft--;
        }
        
        m_NextSampleToUse = 0;
        
        fclose(toneGenHandle);
    }
}
#endif //WV_USE_TONE_GEN


//**********************************************************************************************
// WCMRCoreAudioDevice::SetStreaming
//
//! Sets the device's streaming status. Calls PA's Start/Stop stream routines.
//!
//! \param newState : Should be true to start streaming, false to stop streaming. This roughly
//!     corresponds to calling Start/Stop on the lower level interface.
//! 
//! \return eNoErr always, the derived classes may return appropriate error code.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetStreaming (bool newState)
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    ComponentResult err = 0;

    if (Streaming () == newState)
        goto Exit;

    if (newState)
    {
#if WV_USE_TONE_GEN
        SetupToneGenerator ();
#endif //WV_USE_TONE_GEN

        m_SampleCountAtLastIdle = 0;
        m_StalledSampleCounter = 0;
        m_SampleCounter = 0;
        m_IOProcThreadPort = 0;
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Starting AUHAL.");
        
        if (m_UseMultithreading)
        {
            //set thread constraints...
            unsigned int periodAndConstraintUS = (unsigned int)((1000000.0 * m_CurrentBufferSize) / m_CurrentSamplingRate);
            unsigned int computationUS = (unsigned int)(0.8 * periodAndConstraintUS); //assuming we may want to use up to 80% CPU
            //ErrandManager().SetRealTimeConstraintsForAllThreads (periodAndConstraintUS, computationUS, periodAndConstraintUS);
        }
        
        err = AudioOutputUnitStart (m_AUHALAudioUnit);
        
        m_StopRequested = false;
        
        if(err)
        {
            DEBUG_MSG( "Failed to start AudioUnit, err " << err );
            retVal = eGenericErr;
            goto Exit;
        }
    }
    else
    {
        m_StopRequested = true;
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDebugInfo, (void *)"Stopping AUHAL.");
        err = AudioOutputUnitStop (m_AUHALAudioUnit);
        if (!err)
        {
            //if (!m_InputChannels.empty());
            {
                err = AudioUnitReset (m_AUHALAudioUnit, kAudioUnitScope_Global, AUHAL_INPUT_ELEMENT);
            }
            //if (!m_OutputChannels.empty());
            {
                err = AudioUnitReset (m_AUHALAudioUnit, kAudioUnitScope_Global, AUHAL_OUTPUT_ELEMENT);
            }
        }
        
        if(err)
        {
            DEBUG_MSG( "Failed to stop AudioUnit " << err );
            retVal = eGenericErr;
            goto Exit;
        }
        m_IOProcThreadPort = 0;
    }

    // After units restart, reset request for reset and SR change
    m_SRChangeReported = m_SRChangeRequested;
    m_ResetReported = m_ResetRequested;
    
    m_IsStreaming = newState;

Exit:   
    return (retVal);
}


//**********************************************************************************************
// WCMRCoreAudioDevice::DoIdle 
//
//! A place for doing idle time processing. The other derived classes will probably do something
//!     meaningful.
//!
//! \param none
//! 
//! \return eNoErr always.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::DoIdle ()
{
    /*
    if (m_BufferSizeChangeRequested != m_BufferSizeChangeReported)
    {
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::BufferSizeChanged);
        m_BufferSizeChangeReported = m_BufferSizeChangeRequested;
    }

    if (m_ResetRequested != m_ResetReported)
    {
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestReset);
        m_ResetReported = m_ResetRequested;
    }


    if (m_ResyncRequested != m_ResyncReported)
    {
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::RequestResync);
        m_ResyncReported = m_ResyncRequested;
    }
    
    if (m_SRChangeReported != m_SRChangeRequested)
    {
        m_SRChangeReported = m_SRChangeRequested;
        int newSR = CurrentSamplingRate();
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::SamplingRateChanged, (void *)newSR);
    }

    if (m_DropsReported != m_DropsDetected)
    {
        m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceDroppedSamples);
        m_DropsReported = m_DropsDetected;
    }

    
    //Perhaps add checks to make sure a stream counter is incrementing if
    //stream is supposed to be streaming!
    if (Streaming())
    {
        //latch the value
        int64_t currentSampleCount = m_SampleCounter;
        if (m_SampleCountAtLastIdle == currentSampleCount)
            m_StalledSampleCounter++;
        else
        {
            m_SampleCountAtLastIdle = (int)currentSampleCount;
            m_StalledSampleCounter = 0;
        }

        if (m_StalledSampleCounter > NUM_STALLS_FOR_NOTIFICATION)
        {
            m_StalledSampleCounter = 0;
            m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::DeviceStoppedStreaming, (void *)currentSampleCount);
        }
    }*/

    
    return (eNoErr);
}





//**********************************************************************************************
// WCMRCoreAudioDevice::SetMonitorChannels 
//
//! Used to set the channels to be used for monitoring.
//!
//! \param leftChannel : Left monitor channel index.
//! \param rightChannel : Right monitor channel index.
//! 
//! \return eNoErr always, the derived classes may return appropriate errors.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetMonitorChannels (int leftChannel, int rightChannel)
{
    AUTO_FUNC_DEBUG;
    //This will most likely be overridden, the base class simply
    //changes the member.
    m_LeftMonitorChannel = leftChannel;
    m_RightMonitorChannel = rightChannel;
    return (eNoErr);
}



//**********************************************************************************************
// WCMRCoreAudioDevice::SetMonitorGain 
//
//! Used to set monitor gain (or atten).
//!
//! \param newGain : The new gain or atten. value to use. Specified as a linear multiplier (not dB) 
//! 
//! \return eNoErr always, the derived classes may return appropriate errors.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::SetMonitorGain (float newGain)
{
    AUTO_FUNC_DEBUG;
    //This will most likely be overridden, the base class simply
    //changes the member.
    
    
    m_MonitorGain = newGain;
    return (eNoErr);
}




//**********************************************************************************************
// WCMRCoreAudioDevice::ShowConfigPanel 
//
//! Used to show device specific config/control panel. Some interfaces may not support it.
//!     Some interfaces may require the device to be active before it can display a panel.
//!
//! \param pParam : A device/interface specific parameter, should be the app window handle for ASIO.
//! 
//! \return eNoErr always, the derived classes may return errors.
//! 
//**********************************************************************************************
WTErr WCMRCoreAudioDevice::ShowConfigPanel (void */*pParam*/)
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    
    CFStringRef configAP;
    UInt32 propSize = sizeof (configAP);
    /*
      @constant       kAudioDevicePropertyConfigurationApplication
      A CFString that contains the bundle ID for an application that provides a
      GUI for configuring the AudioDevice. By default, the value of this property
      is the bundle ID for Audio MIDI Setup. The caller is responsible for
      releasing the returned CFObject.
    */
    
    if (AudioDeviceGetProperty(m_DeviceID, 0, 0, kAudioDevicePropertyConfigurationApplication, &propSize, &configAP) == kAudioHardwareNoError)
    {
        //  get the FSRef of the config app
        FSRef theAppFSRef;
        OSStatus theError = LSFindApplicationForInfo(kLSUnknownCreator, configAP, NULL, &theAppFSRef, NULL);
        if (!theError)
        {
            LSOpenFSRef(&theAppFSRef, NULL);
        }
        else
        {
            // open default AudioMIDISetup if device app is not found
            CFStringRef audiMidiSetupApp = CFStringCreateWithCString(kCFAllocatorDefault, "com.apple.audio.AudioMIDISetup", kCFStringEncodingMacRoman);
            theError = LSFindApplicationForInfo(kLSUnknownCreator, audiMidiSetupApp, NULL, &theAppFSRef, NULL);
            
            if (!theError)
            {
                LSOpenFSRef(&theAppFSRef, NULL);
            }
        }
        
        CFRelease (configAP);
    }
    
    return (retVal);
}


//**********************************************************************************************
// WCMRCoreAudioDevice::StaticAudioIOProc
//
//! The AudioIOProc that gets called when the AudioUnit is ready with recorded audio, and wants to get audio.
//!     This one simply calls the non-static member.
//!
//! \param inRefCon : What was passed when setting up the Callback (in our case a pointer to teh WCMRCoreAudioDevice object).
//! \param ioActionFlags : What actios has to be taken.
//! \param inTimeStamp: When the data will be played back.
//! \param inBusNumber : The AU element.
//! \param inNumberFrames: Number af Audio frames that are requested.
//! \param ioData : Where the playback data is to be placed.
//! 
//! \return 0 always
//! 
//**********************************************************************************************
OSStatus WCMRCoreAudioDevice::StaticAudioIOProc(void *inRefCon, AudioUnitRenderActionFlags *    ioActionFlags,
                                                const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames,
                                                AudioBufferList *ioData)
{
    WCMRCoreAudioDevice *pMyDevice = (WCMRCoreAudioDevice *)inRefCon;
    if (pMyDevice)
        return pMyDevice->AudioIOProc (ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
    else
        return 0;
}




//**********************************************************************************************
// WCMRCoreAudioDevice::AudioIOProc
//
//! The non-static AudioIOProc that gets called when the AudioUnit is ready with recorded audio, and wants to get audio.
//!     We retrieve the recorded audio, and then do our processing, to generate audio to be played back.
//!
//! \param ioActionFlags : What actios has to be taken.
//! \param inTimeStamp: When the data will be played back.
//! \param inBusNumber : The AU element.
//! \param inNumberFrames: Number af Audio frames that are requested.
//! \param ioData : Where the playback data is to be placed.
//! 
//! \return 0 always
//! 
//**********************************************************************************************
OSStatus WCMRCoreAudioDevice::AudioIOProc(AudioUnitRenderActionFlags *  ioActionFlags,
                                          const AudioTimeStamp *inTimeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames,
                                          AudioBufferList *ioData)
{
    UInt64 theStartTime = AudioGetCurrentHostTime();

    OSStatus retVal = 0;
    
    if (m_StopRequested)
        return retVal;

    if (m_IOProcThreadPort == 0)
        m_IOProcThreadPort = mach_thread_self ();
    
    //cannot really deal with it unless the number of frames are the same as our buffer size!
    if (inNumberFrames != (UInt32)m_CurrentBufferSize)
        return retVal;
    
    //Retrieve the input data...
    if (!m_InputChannels.empty())
    {
        UInt32 expectedDataSize = m_InputChannels.size() * m_CurrentBufferSize * sizeof(float);
        AudioBufferList inputAudioBufferList;
        inputAudioBufferList.mNumberBuffers = 1;
        inputAudioBufferList.mBuffers[0].mNumberChannels = m_InputChannels.size();
        inputAudioBufferList.mBuffers[0].mDataByteSize = expectedDataSize;
        inputAudioBufferList.mBuffers[0].mData = NULL;//new float[expectedDataSize]; // we are going to get buffer from CoreAudio
        
        retVal = AudioUnitRender(m_AUHALAudioUnit, ioActionFlags, inTimeStamp, AUHAL_INPUT_ELEMENT, inNumberFrames, &inputAudioBufferList);
        
        if (retVal == kAudioHardwareNoError &&
            inputAudioBufferList.mBuffers[0].mNumberChannels == m_InputChannels.size() &&
            inputAudioBufferList.mBuffers[0].mDataByteSize == expectedDataSize )
        {
            m_pInputData = (float*)inputAudioBufferList.mBuffers[0].mData;
        }
        else
        {
            m_pInputData = NULL;
            return retVal;
        }
    }
    
    //is this an input only device?
    if (m_OutputChannels.empty())
        AudioCallback (NULL, inNumberFrames, (int64_t)inTimeStamp->mSampleTime, theStartTime);
    else if ((!m_OutputChannels.empty()) && (ioData->mBuffers[0].mNumberChannels == m_OutputChannels.size()))
        AudioCallback ((float *)ioData->mBuffers[0].mData, inNumberFrames, (int64_t)inTimeStamp->mSampleTime, theStartTime);
    
    return retVal;
}


//**********************************************************************************************
// WCMRCoreAudioDevice::AudioCallback 
//
//! Here's where the actual audio processing happens. We call upon all the active connections' 
//!     sinks to provide data to us which can be put/mixed in the output buffer! Also, we make the 
//!     input data available to any sources that may call upon us during this time!
//!
//! \param *pOutputBuffer : Points to a buffer to receive playback data. For Input only devices, this will be NULL
//! \param framesPerBuffer : Number of sample frames in input and output buffers. Number of channels,
//!     which are interleaved, is fixed at Device Open (Active) time. In this implementation,
//!     the number of channels are fixed to use the maximum available.
//! 
//! \return true
//! 
//**********************************************************************************************
int WCMRCoreAudioDevice::AudioCallback (float *pOutputBuffer, unsigned long framesPerBuffer, int64_t inSampleTime, uint64_t inCycleStartTime)
{
    struct WCMRAudioDeviceManagerClient::AudioCallbackData audioCallbackData =
    {
        m_pInputData,
        pOutputBuffer,
        framesPerBuffer,
        inSampleTime,
        AudioConvertHostTimeToNanos(inCycleStartTime)
    };
    
    m_pMyManager->NotifyClient (WCMRAudioDeviceManagerClient::AudioCallback, (void *)&audioCallbackData);
    
    m_SampleCounter += framesPerBuffer;
    return m_StopRequested;
}


//**********************************************************************************************
// WCMRCoreAudioDevice::GetLatency
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
uint32_t WCMRCoreAudioDevice::GetLatency(bool isInput)
{
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;

    UInt32 propSize = sizeof(UInt32);
    UInt32 value1 = 0;
    UInt32 value2 = 0;
    
    UInt32 latency = 0;
    std::vector<int> streamLatencies;
    
    
    err = AudioDeviceGetProperty(m_DeviceID, 0, isInput, kAudioDevicePropertyLatency, &propSize, &value1);
    if (err != kAudioHardwareNoError)
    {
        DEBUG_MSG("GetLatency kAudioDevicePropertyLatency err = " << err);
    }

    err = AudioDeviceGetProperty(m_DeviceID, 0, isInput, kAudioDevicePropertySafetyOffset, &propSize, &value2);
    if (err != kAudioHardwareNoError)
    {
        DEBUG_MSG("GetLatency kAudioDevicePropertySafetyOffset err = " << err);
    }

    latency = value1 + value2;

    err = GetStreamLatency(m_DeviceID, isInput, streamLatencies);
    if (err == kAudioHardwareNoError)
    {
        for ( int i = 0; i < streamLatencies.size(); i++) {
            latency += streamLatencies[i];
        }
    }
    
    return latency;
}

//**********************************************************************************************
// WCMRCoreAudioDevice::GetStreamLatency
//
//! Get stream latency for device.
//!
//! \param deviceID : The audio device ID.
//!
//! \param isInput : Return latency for the input if isInput is true, otherwise the output latency
//!                  wiil be returned.
//**********************************************************************************************
OSStatus WCMRCoreAudioDevice::GetStreamLatency(AudioDeviceID device, bool isInput, std::vector<int>& latencies)
{
    OSStatus err = kAudioHardwareNoError;
    UInt32 outSize1, outSize2, outSize3;
    Boolean	outWritable;
    
    err = AudioDeviceGetPropertyInfo(device, 0, isInput, kAudioDevicePropertyStreams, &outSize1, &outWritable);
    if (err == noErr) {
        int stream_count = outSize1 / sizeof(UInt32);
        AudioStreamID streamIDs[stream_count];
        AudioBufferList bufferList[stream_count];
        UInt32 streamLatency;
        outSize2 = sizeof(UInt32);
        
        err = AudioDeviceGetProperty(device, 0, isInput, kAudioDevicePropertyStreams, &outSize1, streamIDs);
        if (err != noErr) {
            DEBUG_MSG("GetStreamLatencies kAudioDevicePropertyStreams err = " << err);
            return err;
        }
        
        err = AudioDeviceGetPropertyInfo(device, 0, isInput, kAudioDevicePropertyStreamConfiguration, &outSize3, &outWritable);
        if (err != noErr) {
            DEBUG_MSG("GetStreamLatencies kAudioDevicePropertyStreamConfiguration err = " << err);
            return err;
        }
        
        for (int i = 0; i < stream_count; i++) {
            err = AudioStreamGetProperty(streamIDs[i], 0, kAudioStreamPropertyLatency, &outSize2, &streamLatency);
            if (err != noErr) {
                DEBUG_MSG("GetStreamLatencies kAudioStreamPropertyLatency err = " << err);
                return err;
            }
            err = AudioDeviceGetProperty(device, 0, isInput, kAudioDevicePropertyStreamConfiguration, &outSize3, bufferList);
            if (err != noErr) {
                DEBUG_MSG("GetStreamLatencies kAudioDevicePropertyStreamConfiguration err = " << err);
                return err;
            }
            latencies.push_back(streamLatency);
        }
    }
    return err;
}


//**********************************************************************************************
// WCMRCoreAudioDeviceManager::WCMRCoreAudioDeviceManager
//
//! The constructuor, we initialize PA, and build the device list.
//!
//! \param *pTheClient : The manager's client object (which receives notifications).
//! \param useMultithreading : Whether to use multi-threading for audio processing. Default is true.
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRCoreAudioDeviceManager::WCMRCoreAudioDeviceManager(WCMRAudioDeviceManagerClient *pTheClient,
                            eAudioDeviceFilter eCurAudioDeviceFilter, bool useMultithreading, bool bNocopy)
  : WCMRAudioDeviceManager (pTheClient, eCurAudioDeviceFilter)
  , m_UseMultithreading (useMultithreading)
  , m_bNoCopyAudioBuffer(bNocopy)
{
    AUTO_FUNC_DEBUG;

    //first of all, tell HAL to use it's own run loop, not to wait for our runloop to do
    //it's dirty work...
    //Essentially, this makes the HAL on Snow Leopard behave like Leopard.
    //It's not yet (as of October 2009 documented), but the following discussion
    //has the information provided by Jeff Moore @ Apple:
    // http://lists.apple.com/archives/coreaudio-api/2009/Oct/msg00214.html
    //
    // As per Jeff's suggestion, opened an Apple Bug on this - ID# 7364011
    
    CFRunLoopRef nullRunLoop = 0;
    OSStatus err = AudioHardwareSetProperty (kAudioHardwarePropertyRunLoop, sizeof(CFRunLoopRef), &nullRunLoop);

    if (err != kAudioHardwareNoError)
    {
        syslog (LOG_NOTICE, "Unable to set RunLoop for Audio Hardware");
    }

    //add a listener to find out when devices change...
    AudioHardwareAddPropertyListener (kAudioHardwarePropertyDevices, HardwarePropertyChangeCallback, this);
    
    //Always add the None device first...
    m_NoneDevice = new WCMRNativeAudioNoneDevice(this);

    //prepare our initial list...
    generateDeviceListImpl();

    return;
}



//**********************************************************************************************
// WCMRCoreAudioDeviceManager::~WCMRCoreAudioDeviceManager
//
//! It clears the device list, releasing each of the device.
//!
//! \param none
//! 
//! \return Nothing.
//! 
//**********************************************************************************************
WCMRCoreAudioDeviceManager::~WCMRCoreAudioDeviceManager()
{
    AUTO_FUNC_DEBUG;

    try
    {
        delete m_NoneDevice;
    }
    catch (...)
    {
        //destructors should absorb exceptions, no harm in logging though!!
        DEBUG_MSG ("Exception during destructor");
    }

}


WCMRAudioDevice* WCMRCoreAudioDeviceManager::initNewCurrentDeviceImpl(const std::string & deviceName)
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
            
            m_CurrentDevice = new WCMRCoreAudioDevice (this, devInfo.m_DeviceId, m_UseMultithreading, m_bNoCopyAudioBuffer);
		}
		catch (...)
		{
			std::cout << "Unabled to create PA Device: " << devInfo.m_DeviceId << std::endl;
			DEBUG_MSG ("Unabled to create PA Device: " << devInfo.m_DeviceId);
		}
	}
    
	return m_CurrentDevice;
}


void WCMRCoreAudioDeviceManager::destroyCurrentDeviceImpl()
{
    if (m_CurrentDevice != m_NoneDevice)
        delete m_CurrentDevice;
    
    m_CurrentDevice = 0;
}
    
    
WTErr WCMRCoreAudioDeviceManager::getDeviceAvailableSampleRates(DeviceID deviceId, std::vector<int>& sampleRates)
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    sampleRates.clear();
    
    //! 1. Get sample rate property size.
    err = AudioDeviceGetPropertyInfo(deviceId, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels output.
        
        // Allocate size according to the number of audio values
        int numRates = propSize / sizeof(AudioValueRange);
        AudioValueRange* supportedRates = new AudioValueRange[numRates];
        
        // Get sampling rates from Audio device
        err = AudioDeviceGetProperty(deviceId, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &propSize, supportedRates);
        if (err == kAudioHardwareNoError)
        {
            //! 3. Update sample rates
            
            // now iterate through our standard SRs
            for(int ourSR=0; gAllSampleRates[ourSR] > 0; ourSR++)
            {
                //check to see if our SR is in the supported rates...
                for (int deviceSR = 0; deviceSR < numRates; deviceSR++)
                {
                    if ((supportedRates[deviceSR].mMinimum <= gAllSampleRates[ourSR]) &&
                        (supportedRates[deviceSR].mMaximum >= gAllSampleRates[ourSR]))
                    {
                        sampleRates.push_back ((int)gAllSampleRates[ourSR]);
                        break;
                    }
                }
            }
        }
        else
        {
            retVal = eCoreAudioFailed;
            DEBUG_MSG("Failed to get device Sample rates. Device Name: " << m_DeviceName.c_str());
        }
        
        delete [] supportedRates;
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Sample rates property size. Device Name: " << m_DeviceName.c_str());
    }
    
    return retVal;
}
    
    
WTErr WCMRCoreAudioDeviceManager::getDeviceMaxInputChannels(DeviceID deviceId, unsigned int& inputChannels)
{
    AUTO_FUNC_DEBUG;
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    inputChannels = 0;

    // 1. Get property cannels input size.
    err = AudioDeviceGetPropertyInfo (deviceId, 0, 1/* Input */, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels input.
        
        // Allocate size according to the property size. Note that this is a variable sized struct...
        AudioBufferList *pStreamBuffers = (AudioBufferList *)malloc(propSize);
        
        if (pStreamBuffers)
        {
            memset (pStreamBuffers, 0, propSize);
            
            // Get the Input channels
            err = AudioDeviceGetProperty (deviceId, 0, 1/* Input */, kAudioDevicePropertyStreamConfiguration, &propSize, pStreamBuffers);
            if (err == kAudioHardwareNoError)
            {
                // Calculate the number of input channels
                for (UInt32 streamIndex = 0; streamIndex < pStreamBuffers->mNumberBuffers; streamIndex++)
                {
                    inputChannels += pStreamBuffers->mBuffers[streamIndex].mNumberChannels;
                }
            }
            else
            {
                retVal = eCoreAudioFailed;
                DEBUG_MSG("Failed to get device Input channels. Device Name: " << m_DeviceName.c_str());
            }
            
            free (pStreamBuffers);
        }
        else
        {
            retVal = eMemOutOfMemory;
            DEBUG_MSG("Faild to allocate memory. Device Name: " << m_DeviceName.c_str());
        }
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Input channels property size. Device Name: " << m_DeviceName.c_str());
    }
    
    return retVal;
}
    

WTErr WCMRCoreAudioDeviceManager::getDeviceMaxOutputChannels(DeviceID deviceId, unsigned int& outputChannels)
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    outputChannels = 0;

    //! 1. Get property cannels output size.
    err = AudioDeviceGetPropertyInfo (deviceId, 0, 0/* Output */, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels output.
        
        // Allocate size according to the property size. Note that this is a variable sized struct...
        AudioBufferList *pStreamBuffers = (AudioBufferList *)malloc(propSize);
        if (pStreamBuffers)
        {
            memset (pStreamBuffers, 0, propSize);
            
            // Get the Output channels
            err = AudioDeviceGetProperty (deviceId, 0, 0/* Output */, kAudioDevicePropertyStreamConfiguration, &propSize, pStreamBuffers);
            if (err == kAudioHardwareNoError)
            {
                // Calculate the number of output channels
                for (UInt32 streamIndex = 0; streamIndex < pStreamBuffers->mNumberBuffers; streamIndex++)
                {
                    outputChannels += pStreamBuffers->mBuffers[streamIndex].mNumberChannels;
                }
            }
            else
            {
                retVal = eCoreAudioFailed;
                DEBUG_MSG("Failed to get device Output channels. Device Name: " << m_DeviceName.c_str());
            }
            free (pStreamBuffers);
        }
        else
        {
            retVal = eMemOutOfMemory;
            DEBUG_MSG("Faild to allocate memory. Device Name: " << m_DeviceName.c_str());
        }
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Output channels property size. Device Name: " << m_DeviceName.c_str());
    }
 
    return retVal;
}
    
    
WTErr WCMRCoreAudioDeviceManager::generateDeviceListImpl()
{
    AUTO_FUNC_DEBUG;
    
    // lock the list first
    wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceInfoVecMutex);
    m_DeviceInfoVec.clear();
    
    //First, get info from None device which is always present
    if (m_NoneDevice)
    {
        DeviceInfo *pDevInfo = new DeviceInfo(NONE_DEVICE_ID, m_NoneDevice->DeviceName() );
        pDevInfo->m_AvailableSampleRates = m_NoneDevice->SamplingRates();
        m_DeviceInfoVec.push_back(pDevInfo);
    }
    
    WTErr retVal = eNoErr;
    OSStatus osErr = noErr;
    AudioDeviceID* deviceIDs = 0;
    
    openlog("WCMRCoreAudioDeviceManager", LOG_PID | LOG_CONS, LOG_USER);
    
    try
    {
        //Get device count...
        UInt32 propSize = 0;
        osErr = AudioHardwareGetPropertyInfo (kAudioHardwarePropertyDevices, &propSize, NULL);
        ASSERT_ERROR(osErr, "AudioHardwareGetProperty 1");
        if (WUIsError(osErr))
            throw osErr;
        
        size_t numDevices = propSize / sizeof (AudioDeviceID);
        deviceIDs = new AudioDeviceID[numDevices];
        
        //retrieve the device IDs
        propSize = numDevices * sizeof (AudioDeviceID);
        osErr = AudioHardwareGetProperty (kAudioHardwarePropertyDevices, &propSize, deviceIDs);
        ASSERT_ERROR(osErr, "Error while getting audio devices: AudioHardwareGetProperty 2");
        if (WUIsError(osErr))
            throw osErr;
        
        //now add the ones that are not there...
        for (size_t deviceIndex = 0; deviceIndex < numDevices; deviceIndex++)
        {
            DeviceInfo* pDevInfo = 0;
            
            //Get device name and create new DeviceInfo entry
            //Get property name size.
            osErr = AudioDeviceGetPropertyInfo(deviceIDs[deviceIndex], 0, 0, kAudioDevicePropertyDeviceName, &propSize, NULL);
            if (osErr == kAudioHardwareNoError)
            {
                //Get property: name.
                char* deviceName = new char[propSize];
                osErr = AudioDeviceGetProperty(deviceIDs[deviceIndex], 0, 0, kAudioDevicePropertyDeviceName, &propSize, deviceName);
                if (osErr == kAudioHardwareNoError)
                {
                    pDevInfo = new DeviceInfo(deviceIDs[deviceIndex], deviceName);
                }
                else
                {
                    retVal = eCoreAudioFailed;
                    DEBUG_MSG("Failed to get device name. Device ID: " << m_DeviceID);
                }
                
                delete [] deviceName;
            }
            else
            {
                retVal = eCoreAudioFailed;
                DEBUG_MSG("Failed to get device name property size. Device ID: " << m_DeviceID);
            }
            
            if (pDevInfo)
            {
                //Retrieve all the information we need for the device
                WTErr wErr = eNoErr;
                
                //Get available sample rates for the device
                std::vector<int> availableSampleRates;
                wErr = getDeviceAvailableSampleRates(pDevInfo->m_DeviceId, availableSampleRates);
                
                if (wErr != eNoErr)
                {
                    DEBUG_MSG ("Failed to get device available sample rates. Device ID: " << m_DeviceID);
                    delete pDevInfo;
                    continue; //proceed to the next device
                }
                
                pDevInfo->m_AvailableSampleRates = availableSampleRates;
                
                //Get max input channels
                uint32_t maxInputChannels;
                wErr = getDeviceMaxInputChannels(pDevInfo->m_DeviceId, maxInputChannels);
                
                if (wErr != eNoErr)
                {
                    DEBUG_MSG ("Failed to get device max input channels count. Device ID: " << m_DeviceID);
                    delete pDevInfo;
                    continue; //proceed to the next device
                }
                
                pDevInfo->m_MaxInputChannels = maxInputChannels;
                
                //Get max output channels
                uint32_t maxOutputChannels;
                wErr = getDeviceMaxOutputChannels(pDevInfo->m_DeviceId, maxOutputChannels);
                
                if (wErr != eNoErr)
                {
                    DEBUG_MSG ("Failed to get device max output channels count. Device ID: " << m_DeviceID);
                    delete pDevInfo;
                    continue; //proceed to the next device
                }
                
                pDevInfo->m_MaxOutputChannels = maxOutputChannels;
                
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
                    syslog (LOG_NOTICE, "%s rejected, In Channels = %d, Out Channels = %d\n",
                            pDevInfo->m_DeviceName.c_str(), pDevInfo->m_MaxInputChannels, pDevInfo->m_MaxOutputChannels);
                    // In case of Input and Output both channels being Zero, we will release memory; since we created CoreAudioDevice but we are Not adding it in list.
                    delete pDevInfo;
                }
            }
        }
        
        
        //If no devices were found, that's not a good thing!
        if (m_DeviceInfoVec.empty())
        {
            DEBUG_MSG ("No matching CoreAudio devices were found\n");
        }        
    }
    catch (...)
    {
        if (WUNoError(retVal))
            retVal = eCoreAudioFailed;
    }
    
    delete[] deviceIDs;
    closelog();
    
    return retVal;
}


WTErr WCMRCoreAudioDeviceManager::updateDeviceListImpl()
{
    wvNS::wvThread::ThreadMutex::lock theLock(m_AudioDeviceInfoVecMutex);
    WTErr err = generateDeviceListImpl();
    
    if (eNoErr != err)
    {
        std::cout << "API::PortAudioDeviceManager::updateDeviceListImpl: Device list update error: "<< err << std::endl;
        return err;
    }
    
    if (m_CurrentDevice)
    {
        // if we have device initialized we should find out if this device is still connected
        DeviceInfo devInfo;
        WTErr deviceLookUpErr = GetDeviceInfoByName(m_CurrentDevice->DeviceName(), devInfo );
    
        if (eNoErr != deviceLookUpErr)
        {
            NotifyClient (WCMRAudioDeviceManagerClient::IODeviceDisconnected);
            return err;
        }
    }
    
    NotifyClient (WCMRAudioDeviceManagerClient::DeviceListChanged);
    
    return err;
}


WTErr WCMRCoreAudioDeviceManager::getDeviceSampleRatesImpl(const std::string & deviceName, std::vector<int>& sampleRates) const
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    sampleRates.clear();
    
    //first check if the request has been made for None device
	if (deviceName == m_NoneDevice->DeviceName() )
	{
		sampleRates = m_NoneDevice->SamplingRates();
		return retVal;
	}

    if (m_CurrentDevice && m_CurrentDevice->DeviceName () == deviceName) {
        sampleRates.assign(m_CurrentDevice->SamplingRates().begin(), m_CurrentDevice->SamplingRates().end() );
        return retVal;
    }
    
    DeviceInfo devInfo;
    retVal = GetDeviceInfoByName(deviceName, devInfo);
    
    //! 1. Get sample rate property size.
    err = AudioDeviceGetPropertyInfo(devInfo.m_DeviceId, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &propSize, NULL);
    
    if (err == kAudioHardwareNoError)
    {
        //! 2. Get property: cannels output.
        
        // Allocate size accrding to the number of audio values
        int numRates = propSize / sizeof(AudioValueRange);
        AudioValueRange* supportedRates = new AudioValueRange[numRates];
        
        // Get sampling rates from Audio device
        err = AudioDeviceGetProperty(devInfo.m_DeviceId, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates, &propSize, supportedRates);
        
        if (err == kAudioHardwareNoError)
        {
            //! 3. Update sample rates
            
            // now iterate through our standard SRs
            for(int ourSR=0; gAllSampleRates[ourSR] > 0; ourSR++)
            {
                //check to see if our SR is in the supported rates...
                for (int deviceSR = 0; deviceSR < numRates; deviceSR++)
                {
                    if ((supportedRates[deviceSR].mMinimum <= gAllSampleRates[ourSR]) &&
                        (supportedRates[deviceSR].mMaximum >= gAllSampleRates[ourSR]))
                    {
                        sampleRates.push_back ((int)gAllSampleRates[ourSR]);
                        break;
                    }
                }
            }
        }
        else
        {
            retVal = eCoreAudioFailed;
            DEBUG_MSG("Failed to get device Sample rates. Device Name: " << m_DeviceName.c_str());
        }
        
        delete [] supportedRates;
    }
    else
    {
        retVal = eCoreAudioFailed;
        DEBUG_MSG("Failed to get device Sample rates property size. Device Name: " << m_DeviceName.c_str());
    }

    devInfo.m_AvailableSampleRates.assign(sampleRates.begin(), sampleRates.end() );
    
    return retVal;
}


WTErr WCMRCoreAudioDeviceManager::getDeviceBufferSizesImpl(const std::string & deviceName, std::vector<int>& bufferSizes) const
{
    AUTO_FUNC_DEBUG;
    
    WTErr retVal = eNoErr;
    OSStatus err = kAudioHardwareNoError;
    UInt32 propSize = 0;
    
    bufferSizes.clear();
    
    //first check if the request has been made for None device
	if (deviceName == m_NoneDevice->DeviceName() )
	{
		bufferSizes = m_NoneDevice->BufferSizes();
		return retVal;
	}
    
    if (m_CurrentDevice && m_CurrentDevice->DeviceName () == deviceName) {
        bufferSizes.assign(m_CurrentDevice->BufferSizes().begin(), m_CurrentDevice->BufferSizes().end() );
        return retVal;
    }
    
    DeviceInfo devInfo;
    retVal = GetDeviceInfoByName(deviceName, devInfo);
    
    if (eNoErr == retVal)
    {
        // 1. Get buffer size range
        AudioValueRange bufferSizesRange;
        propSize = sizeof (AudioValueRange);
        err = AudioDeviceGetProperty (devInfo.m_DeviceId, 0, 0, kAudioDevicePropertyBufferFrameSizeRange, &propSize, &bufferSizesRange);
        if(err == kAudioHardwareNoError)
        {
            // 2. Run on all ranges and add them to the list
            for(int bsize=0; gAllBufferSizes[bsize] > 0; bsize++)
            {
                if ((bufferSizesRange.mMinimum <= gAllBufferSizes[bsize]) && (bufferSizesRange.mMaximum >= gAllBufferSizes[bsize]))
                {
                    bufferSizes.push_back (gAllBufferSizes[bsize]);
                }
            }
            
            //if we didn't get a single hit, let's simply add the min. and the max...
            if (bufferSizes.empty())
            {
                bufferSizes.push_back ((int)bufferSizesRange.mMinimum);
                bufferSizes.push_back ((int)bufferSizesRange.mMaximum);
            }
        }
        else
        {
            retVal = eCoreAudioFailed;
            DEBUG_MSG("Failed to get device buffer sizes range. Device Name: " << m_DeviceName.c_str());
        }
    }
    else
	{
		retVal = eRMResNotFound;
		std::cout << "API::PortAudioDeviceManager::GetBufferSizes: Device not found: "<< deviceName << std::endl;
	}

    
    return retVal;
}


OSStatus WCMRCoreAudioDeviceManager::HardwarePropertyChangeCallback (AudioHardwarePropertyID inPropertyID, void* inClientData)
{
    switch (inPropertyID)
    {
        case kAudioHardwarePropertyDevices:
            {
                WCMRCoreAudioDeviceManager* pManager = (WCMRCoreAudioDeviceManager*)inClientData;
                if (pManager)
                    pManager->updateDeviceListImpl();
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}
