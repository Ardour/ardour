#ifndef ardour_tranzport_control_protocol_h
#define ardour_tranzport_control_protocol_h

#include <vector>

#include <sys/time.h>
#include <pbd/lockmonitor.h>
#include <pthread.h>
#include <usb.h>
#include <ardour/control_protocol.h>
#include <ardour/types.h>

namespace ARDOUR {

class TranzportControlProtocol : public ControlProtocol {
  public:
	TranzportControlProtocol (Session&);
	virtual ~TranzportControlProtocol();

	int init ();
	bool active() const;

	void send_route_feedback (std::list<Route*>&);
	void send_global_feedback ();

  private:
	static const int VENDORID = 0x165b;
	static const int PRODUCTID = 0x8101;
	static const int READ_ENDPOINT  = 0x81;
	static const int WRITE_ENDPOINT = 0x02;
	const static int STATUS_OFFLINE  = 0xff;
	const static int STATUS_ONLINE = 0x01;
	const static uint8_t WheelDirectionThreshold = 0x3f;

	enum LightID {
		LightRecord = 0,
		LightTrackrec,
		LightTrackmute,
		LightTracksolo,
		LightAnysolo,
		LightLoop,
		LightPunch
	};

	enum ButtonID {
		ButtonBattery = 0x00004000,
		ButtonBacklight = 0x00008000,
		ButtonTrackLeft = 0x04000000,
		ButtonTrackRight = 0x40000000,
		ButtonTrackRec = 0x00040000,
		ButtonTrackMute = 0x00400000,
		ButtonTrackSolo = 0x00000400,
		ButtonUndo = 0x80000000,
		ButtonIn = 0x02000000,
		ButtonOut = 0x20000000,
		ButtonPunch = 0x00800000,
		ButtonLoop = 0x00080000,
		ButtonPrev = 0x00020000,
		ButtonAdd = 0x00200000,
		ButtonNext = 0x00000200,
		ButtonRewind = 0x01000000,
		ButtonFastForward = 0x10000000,
		ButtonStop = 0x00010000,
		ButtonPlay = 0x00100000,
		ButtonRecord = 0x00000100,
		ButtonShift = 0x08000000
	};

	enum WheelShiftMode {
		WheelShiftGain,
		WheelShiftPan,
		WheelShiftMaster
	};
		
	enum WheelMode {
		WheelTimeline,
		WheelScrub,
		WheelShuttle
	};

	enum DisplayMode {
		DisplayNormal,
		DisplayBigMeter
	};
	
	pthread_t       thread;
	uint32_t        buttonmask;
	uint32_t        timeout;
	uint8_t        _datawheel;
	uint8_t        _device_status;
	usb_dev_handle* udev;
	Route*          current_route;
	uint32_t        current_track_id;
	char            current_screen[2][20];
	bool            lights[7];
	WheelMode       wheel_mode;
	WheelShiftMode  wheel_shift_mode;
	struct timeval  last_wheel_motion;
	int             last_wheel_dir;
	DisplayMode     display_mode;
	DisplayMode     requested_display_mode;

	std::vector<sigc::connection> track_connections;

	bool     last_negative;
	uint32_t last_hrs;
	uint32_t last_mins;
	uint32_t last_secs;
	uint32_t last_frames;
	jack_nframes_t last_where;

	PBD::Lock write_lock;
	PBD::Lock print_lock;

	int open ();
	int read (uint32_t timeout_override = 0);
	int write (uint8_t* cmd, uint32_t timeout_override = 0);
	int close ();

	int open_core (struct usb_device*);

	void lcd_clear ();
	void print (int row, int col, const char* text);
	int  light_on (LightID);
	int  light_off (LightID);
	void lights_off ();

	void enter_big_meter_mode ();
	void enter_normal_display_mode ();
	void next_display_mode ();

	void show_current_track ();
	void show_transport_time ();
	void show_wheel_mode ();
	void show_gain ();
	void show_pan ();
	void show_meter ();

	void track_solo_changed (void*);
	void track_rec_changed (void*);
	void track_mute_changed (void*);
	void track_gain_changed (void*);
	void record_status_changed ();

	void datawheel ();
	void scrub ();
	void scroll ();
	void shuttle ();

	void next_wheel_mode ();
	void next_wheel_shift_mode ();

	void next_track ();
	void prev_track ();
	void next_marker ();
	void prev_marker ();
	void step_gain_up ();
	void step_gain_down ();
	void step_pan_right ();
	void step_pan_left ();

	static void* _thread_work (void* arg);
	void* thread_work ();

	void button_event_battery_press (bool shifted);
	void button_event_battery_release (bool shifted);
	void button_event_backlight_press (bool shifted);
	void button_event_backlight_release (bool shifted);
	void button_event_trackleft_press (bool shifted);
	void button_event_trackleft_release (bool shifted);
	void button_event_trackright_press (bool shifted);
	void button_event_trackright_release (bool shifted);
	void button_event_trackrec_press (bool shifted);
	void button_event_trackrec_release (bool shifted);
	void button_event_trackmute_press (bool shifted);
	void button_event_trackmute_release (bool shifted);
	void button_event_tracksolo_press (bool shifted);
	void button_event_tracksolo_release (bool shifted);
	void button_event_undo_press (bool shifted);
	void button_event_undo_release (bool shifted);
	void button_event_in_press (bool shifted);
	void button_event_in_release (bool shifted);
	void button_event_out_press (bool shifted);
	void button_event_out_release (bool shifted);
	void button_event_punch_press (bool shifted);
	void button_event_punch_release (bool shifted);
	void button_event_loop_press (bool shifted);
	void button_event_loop_release (bool shifted);
	void button_event_prev_press (bool shifted);
	void button_event_prev_release (bool shifted);
	void button_event_add_press (bool shifted);
	void button_event_add_release (bool shifted);
	void button_event_next_press (bool shifted);
	void button_event_next_release (bool shifted);
	void button_event_rewind_press (bool shifted);
	void button_event_rewind_release (bool shifted);
	void button_event_fastforward_press (bool shifted);
	void button_event_fastforward_release (bool shifted);
	void button_event_stop_press (bool shifted);
	void button_event_stop_release (bool shifted);
	void button_event_play_press (bool shifted);
	void button_event_play_release (bool shifted);
	void button_event_record_press (bool shifted);
	void button_event_record_release (bool shifted);
};

} // namespace

#endif // ardour_tranzport_control_protocol_h
