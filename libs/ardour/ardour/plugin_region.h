#ifndef __ardour_plugin_region_h__
#define __ardour_plugin_region_h__

namespace ARDOUR {

class LIBARDOUR_API PluginRegion : public Region
{
public:
	~PluginRegion();

	XMLNode& state ();
	int      set_state (const XMLNode&, int version);

	/* Readable interface */
	virtual samplecnt_t read (Sample*, samplepos_t /*pos*/, samplecnt_t /*cnt*/, int /*channel*/) const;
	virtual samplecnt_t readable_length() const { return length(); }
	virtual uint32_t  n_channels () const;

	/* automation */
	boost::shared_ptr<Evoral::Control>
	control(const Evoral::Parameter& id, bool create=false) {
		return _automatable.control(id, create);
	}

	boost::shared_ptr<const Evoral::Control>
	control(const Evoral::Parameter& id) const {
		return _automatable.control(id);
	}

private:
	friend class RegionFactory;
	PluginRegion (Session& s, boost::shared_ptr<PluinInsert> pi, samplepos_t, samplecnt_t);

	virtual void recompute_at_start () = 0;
	virtual void recompute_at_end () = 0;

	Automatable  _automatable;
};

} /* namespace ARDOUR */
#endif /* __ardour_plugin_region_h__ */
