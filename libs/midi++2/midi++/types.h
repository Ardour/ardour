#ifndef __midi_types_h__
#define __midi_types_h__

namespace MIDI {

	typedef char           channel_t;
	typedef float          controller_value_t;
	typedef unsigned char  byte;
	typedef unsigned short pitchbend_t;
	typedef unsigned int   timestamp_t;
	typedef unsigned int   nframes_t;

	enum eventType {
	    none = 0x0,
	    raw = 0xF4, /* undefined in MIDI spec */
	    any = 0xF5, /* undefined in MIDI spec */
	    off = 0x80,
	    on = 0x90,
	    controller = 0xB0,
	    program = 0xC0,
	    chanpress = 0xD0,
	    polypress = 0xA0,
	    pitchbend = 0xE0,
	    sysex = 0xF0,
	    mtc_quarter = 0xF1,
	    position = 0xF2,
	    song = 0xF3,
	    tune = 0xF6,
	    eox = 0xF7,
	    timing = 0xF8,
	    start = 0xFA,
	    contineu = 0xFB, /* note spelling */
	    stop = 0xFC,
	    active = 0xFE,
	    reset = 0xFF
    };

    extern const char *controller_names[];
	byte decode_controller_name (const char *name);

    struct EventTwoBytes {
	union {
	    byte note_number;
	    byte controller_number;
	};
	union {
	    byte velocity;
	    byte value;
	};
    };

    enum MTC_FPS {
	    MTC_24_FPS = 0,
	    MTC_25_FPS = 1,
	    MTC_30_FPS_DROP = 2,
	    MTC_30_FPS = 3
    };

    enum MTC_Status {
	    MTC_Stopped = 0,
	    MTC_Forward,
	    MTC_Backward,
    };

}; /* namespace MIDI */

#endif // __midi_types_h__




