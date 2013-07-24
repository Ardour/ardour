namespace ARDOUR

class PortManager 
{
  public:
	PortManager() {}
	virtual ~PortManager() {}

	/* Port registration */
	
	virtual boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname) = 0;
	virtual boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname) = 0;
	virtual int unregister_port (boost::shared_ptr<Port>) = 0;
	
	/* Port connectivity */
	
	virtual int connect (const std::string& source, const std::string& destination) = 0;
	virtual int disconnect (const std::string& source, const std::string& destination) = 0;
	virtual int disconnect (boost::shared_ptr<Port>) = 0;
	
	/* other Port management */
	
	virtual bool port_is_physical (const std::string&) const = 0;
	virtual void get_physical_outputs (DataType type, std::vector<std::string>&) = 0;
	virtual void get_physical_inputs (DataType type, std::vector<std::string>&) = 0;
	virtual boost::shared_ptr<Port> get_port_by_name (const std::string &) = 0;
	virtual void port_renamed (const std::string&, const std::string&) = 0;
	virtual ChanCount n_physical_outputs () const = 0;
	virtual ChanCount n_physical_inputs () const = 0;
	virtual const char ** get_ports (const std::string& port_name_pattern, const std::string& type_name_pattern, uint32_t flags) = 0;

	/* per-Port monitoring */
	
	virtual bool can_request_input_monitoring () const = 0;
	virtual void request_input_monitoring (const std::string&, bool) const = 0;

	class PortRegistrationFailure : public std::exception {
	public:
		PortRegistrationFailure (std::string const & why = "")
			: reason (why) {}

		~PortRegistrationFailure () throw () {}

		virtual const char *what() const throw () { return reason.c_str(); }

	private:
		std::string reason;
	};

};
