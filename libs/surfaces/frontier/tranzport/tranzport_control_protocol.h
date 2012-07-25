
#ifndef ardour_tranzport_control_protocol_h
#define ardour_tranzport_control_protocol_h

#include <vector>

#include <sys/time.h>
#include <pthread.h>
#include <usb.h>

#include <glibmm/threads.h>

#include "ardour/types.h"

#include "control_protocol/control_protocol.h"

class TranzportControlProtocol : public ARDOUR::ControlProtocol
{
  public:
	TranzportControlProtocol (ARDOUR::Session&);
	virtual ~TranzportControlProtocol();

	int set_active (bool yn);

	static bool probe ();

	XMLNode& get_state ();
	int set_state (const XMLNode&);

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
		WheelShiftMaster,
		WheelShiftMarker
	};
		
	enum WheelMode {
		WheelTimeline,
		WheelScrub,
		WheelShuttle
	};

	// FIXME - look at gtk2_ardour for snap settings

	enum WheelIncrement {
	       WheelIncrSlave,
	       WheelIncrScreen,
	       WheelIncrSample,
	       WheelIncrBeat,
	       WheelIncrBar,
	       WheelIncrSecond,
	       WheelIncrMinute
	};
	  
	enum DisplayMode {
		DisplayNormal,
		DisplayRecording,
		DisplayRecordingMeter,
		DisplayBigMeter,
		DisplayConfig,
		DisplayBling,
		DisplayBlingMeter
	};

	enum BlingMode {
	        BlingOff,
	        BlingKit,
	        BlingRotating,
	        BlingPairs,
	        BlingRows,
	        BlingFlashAll
	};
	
	pthread_t       thread;
	uint32_t        buttonmask;
	uint32_t        timeout;
	uint32_t        inflight;
	uint8_t        _datawheel;
	uint8_t        _device_status;
	uint32_t        current_track_id;
	WheelMode       wheel_mode;
	WheelShiftMode  wheel_shift_mode;
	DisplayMode     display_mode;
	BlingMode       bling_mode;
	WheelIncrement  wheel_increment;
	usb_dev_handle* udev;

	ARDOUR::gain_t  gain_fraction;

        Glib::Threads::Mutex update_lock;

	bool screen_invalid[2][20];
	char screen_current[2][20];
	char screen_pending[2][20];
	char screen_flash[2][20];

	bool lights_invalid[7];
	bool lights_current[7];
	bool lights_pending[7];
	bool lights_flash[7];

	uint32_t       last_bars;
	uint32_t       last_beats;
	uint32_t       last_ticks;

	bool           last_negative;
	uint32_t       last_hrs;
	uint32_t       last_mins;
	uint32_t       last_secs;
	uint32_t       last_frames;
	framepos_t     last_where;
	ARDOUR::gain_t last_track_gain;
	uint32_t       last_meter_fill;
	struct timeval last_wheel_motion;
	int            last_wheel_dir;

	Glib::Mutex io_lock;

	int open ();
	int read (uint8_t *buf,uint32_t timeout_override = 0);
	int write (uint8_t* cmd, uint32_t timeout_override = 0);
	int write_noretry (uint8_t* cmd, uint32_t timeout_override = 0);
	int close ();
	int save(char *name = "default");
	int load(char *name = "default");
	void print (int row, int col, const char* text);
	void print_noretry (int row, int col, const char* text);

	int rtpriority_set(int priority = 52);
	int rtpriority_unset(int priority = 0);

	int open_core (struct usb_device*);

	static void* _monitor_work (void* arg);
	void* monitor_work ();

	int process (uint8_t *);
	int update_state();
	void invalidate();
	int flush();
	// bool isuptodate(); // think on this. It seems futile to update more than 30/sec

	// A screen is a cache of what should be on the lcd

	void screen_init();
	void screen_validate();
	void screen_invalidate();
	int  screen_flush();
	void screen_clear();
	// bool screen_isuptodate(); // think on this - 

	// Commands to write to the lcd 

	int  lcd_init();
        bool lcd_damage();
	bool lcd_isdamaged();

        bool lcd_damage(int row, int col = 0, int length = 20);
	bool lcd_isdamaged(int row, int col = 0, int length = 20);

	int  lcd_flush();
	int  lcd_write(uint8_t* cmd, uint32_t timeout_override = 0); // pedantic alias for write
	void lcd_fill (uint8_t fill_char);
	void lcd_clear ();
	void lcd_print (int row, int col, const char* text);
	void lcd_print_noretry (int row, int col, const char* text);

	// Commands to write to the lights
	// FIXME - on some devices lights can have intensity and colors

	void lights_init();
	void lights_validate();
	void lights_invalidate();
	void light_validate(LightID light);
	void light_invalidate(LightID light);
	int  lights_flush();
	int  lights_write(uint8_t* cmd,uint32_t timeout_override = 0); // pedantic alias to write

	// a cache of what should be lit

	void lights_off ();
	void lights_on ();
	int  light_set(LightID, bool offon = true);
	int  light_on (LightID);
	int  light_off (LightID);

	// some modes for the lights, should probably be renamed

	int  lights_show_normal();
	int  lights_show_recording();
	int  lights_show_tempo();
	int  lights_show_bling();

	void enter_big_meter_mode ();
	void enter_normal_display_mode ();
	void enter_config_mode();
	void enter_recording_mode();
	void enter_bling_mode();

	void next_display_mode ();
	void normal_update ();

	void show_current_track ();
	void show_track_gain ();
	void show_transport_time ();
	void show_bbt (framepos_t where);	
	void show_smpte (framepos_t where);
	void show_wheel_mode ();
	void show_gain ();
	void show_pan ();
	void show_meter ();

	void datawheel ();
	void scrub ();
	void scroll ();
	void shuttle ();
	void config ();

	void next_wheel_mode ();
	void next_wheel_shift_mode ();

	void set_current_track (ARDOUR::Route*);
	void next_track ();
	void prev_track ();
	void step_gain_up ();
	void step_gain_down ();
	void step_pan_right ();
	void step_pan_left ();


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

	// new api
	void button_event_mute (bool pressed, bool shifted);
};


#endif // ardour_tranzport_control_protocol_h
