#ifndef MIDI_DEVICE_INFO_H
#define MIDI_DEVICE_INFO_H

/* midi settings */
struct MidiDeviceInfo {
	MidiDeviceInfo(const std::string& dev_name)
	    : device_name(dev_name)
	    , enable(true)
	    , systemic_input_latency(0)
	    , systemic_output_latency(0)
	{
	}

	std::string device_name;
	bool enable;
	uint32_t systemic_input_latency;
	uint32_t systemic_output_latency;
};

#endif // MIDI_DEVICE_INFO_H
