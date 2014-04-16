
// class TracksControlPanel : public WavesDialog {
  public:
	void set_desired_sample_rate (uint32_t sr);
	XMLNode& get_state ();
	
	void update_current_buffer_size (uint32_t new_buffer_size);
    void update_device_list ();
    void switch_to_device(const std::string& device_name);

  private:
// data types:
    struct State {
	std::string backend;
	std::string driver;
	std::string device;
	float sample_rate;
	uint32_t buffer_size;
	uint32_t input_latency;
	uint32_t output_latency;
	uint32_t input_channels;
	uint32_t output_channels;
	bool active;
	std::string midi_option;

	State() 
		: input_latency (0)
		, output_latency (0)
		, input_channels (0)
		, output_channels (0)
		, active (false) {}

    };
    typedef std::list<State> StateList;

// attributes
    uint32_t _desired_sample_rate;
    bool  _have_control;

	// this flag is set for emidiate return during combo-box change callbacks
	// when we don need to process current combo-box change
	uint32_t _ignore_changes;
    std::string _current_device;

	StateList states;

//	Sync stuff
	PBD::ScopedConnectionList update_connections;
    PBD::ScopedConnection running_connection;
    PBD::ScopedConnection stopped_connection;

// methods
	virtual void init();
	void on_audio_settings(WavesButton*);
	void on_midi_settings(WavesButton*);
	void on_session_settings(WavesButton*);
	void on_control_panel(WavesButton*);
    void on_multi_out (WavesButton*);
    void on_stereo_out (WavesButton*);
	void on_ok(WavesButton*);
	void on_cancel(WavesButton*);
	void on_apply(WavesButton*);
	void engine_changed ();
	void device_changed ();
	void buffer_size_changed();
	void sample_rate_changed();
    void engine_running ();
    void engine_stopped ();

	void populate_engine_combo();
	void populate_device_combo();
	void populate_sample_rate_combo();
	void populate_buffer_size_combo();

	std::string bufsize_as_string (uint32_t sz);
    void set_state (const XMLNode&);
    int push_state_to_backend (bool start);

	float get_rate() const;
	std::string get_device_name() const { return device_combo.get_active_text (); };
	uint32_t get_buffer_size() const;
	uint32_t get_input_channels() const { return 0; };
    uint32_t get_output_channels() const { return 0; };
    uint32_t get_input_latency() const { return 0; };
    uint32_t get_output_latency() const { return 0; };

	void show_buffer_duration ();
//};

