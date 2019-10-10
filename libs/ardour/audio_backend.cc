/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/audio_backend.h"

#include "pbd/i18n.h"

namespace ARDOUR {

std::string
AudioBackend::get_error_string (ErrorCode error_code)
{
	switch (error_code) {
	case NoError: // to stop compiler warning
		return _("No Error occurred");
	case BackendInitializationError:
		return _("Failed to initialize audio backend");
	case BackendDeinitializationError:
		return _("Failed to deinitialize audio backend");
	case BackendReinitializationError:
		return _("Failed to reinitialize audio backend");
	case AudioDeviceOpenError:
		return _("Failed to open audio device\n(Typically caused by hardware parameter settings)");
	case AudioDeviceCloseError:
		return _("Failed to close audio device");
	case AudioDeviceInvalidError:
		return _("Audio device not valid");
	case AudioDeviceNotAvailableError:
		return _("Audio device unavailable");
	case AudioDeviceNotConnectedError:
		return _("Audio device not connected");
	case AudioDeviceReservationError:
		return _("Failed to request and reserve audio device");
	case AudioDeviceIOError:
		return _("Audio device Input/Output error");
	case MidiDeviceOpenError:
		return _("Failed to open MIDI device");
	case MidiDeviceCloseError:
		return _("Failed to close MIDI device");
	case MidiDeviceNotAvailableError:
		return _("MIDI device unavailable");
	case MidiDeviceNotConnectedError:
		return _("MIDI device not connected");
	case MidiDeviceIOError:
		return _("MIDI device Input/Output error");
	case SampleFormatNotSupportedError:
		return _("Sample format is not supported");
	case SampleRateNotSupportedError:
		return _("Sample rate is not supported");
	case RequestedInputLatencyNotSupportedError:
		return _("Requested input latency is not supported");
	case RequestedOutputLatencyNotSupportedError:
		return _("Requested output latency is not supported");
	case PeriodSizeNotSupportedError:
		return _("Period size is not supported");
	case PeriodCountNotSupportedError:
		return _("Period count is not supported");
	case DeviceConfigurationNotSupportedError:
		return _("Device configuration not supported");
	case ChannelCountNotSupportedError:
		return _("Channel count configuration not supported");
	case InputChannelCountNotSupportedError:
		return _("Input channel count configuration not supported");
	case OutputChannelCountNotSupportedError:
		return _("Output channel count configuration not supported");
	case AquireRealtimePermissionError:
		return _("Unable to acquire realtime permissions");
	case SettingAudioThreadPriorityError:
		return _("Setting audio device thread priorities failed");
	case SettingMIDIThreadPriorityError:
		return _("Setting MIDI device thread priorities failed");
	case ProcessThreadStartError:
		return _("Failed to start process thread");
	case FreewheelThreadStartError:
		return _("Failed to start freewheel thread");
	case PortRegistrationError:
		return _("Failed to register audio/midi ports");
	case PortReconnectError:
		return _("Failed to re-connect audio/midi ports");
	case OutOfMemoryError:
		return _("Out Of Memory Error");
	}
	return _("Could not reconnect to Audio/MIDI engine");
}

std::string
AudioBackend::get_standard_device_name (StandardDeviceName device_name)
{
	switch (device_name) {
	case DeviceNone:
		return _("None");
	case DeviceDefault:
		return _("Default");
	}
	return std::string();
}

} // namespace ARDOUR
