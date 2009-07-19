#include "ardour/session.h"
#include "ardour/io.h"
#include "ardour/auditioner.h"
#include "ardour/audioengine.h"
#include "ardour/port.h"
#include "session_option_editor.h"
#include "port_matrix.h"
#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;

class OptionsPortMatrix : public PortMatrix
{
public:
	OptionsPortMatrix (ARDOUR::Session& session)
		: PortMatrix (session, DataType::AUDIO)
	{
		_port_group.reset (new PortGroup (""));
		_ports[OURS].add_group (_port_group);
		
		setup_all_ports ();
	}

	void setup_ports (int dim)
	{
		cerr << _session.the_auditioner()->output()->n_ports() << "\n";
		
		if (dim == OURS) {
			_port_group->clear ();
			_port_group->add_bundle (_session.click_io()->bundle());
			_port_group->add_bundle (_session.the_auditioner()->output()->bundle());
		} else {
			_ports[OTHER].gather (_session, true);
		}
	}

	void set_state (ARDOUR::BundleChannel c[2], bool s)
	{
		Bundle::PortList const & our_ports = c[OURS].bundle->channel_ports (c[OURS].channel);
		Bundle::PortList const & other_ports = c[OTHER].bundle->channel_ports (c[OTHER].channel);
		
		if (c[OURS].bundle == _session.click_io()->bundle()) {

			for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
				for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

					Port* f = _session.engine().get_port_by_name (*i);
					assert (f);
					
					if (s) {
						_session.click_io()->connect (f, *j, 0);
					} else {
						_session.click_io()->disconnect (f, *j, 0);
					}
				}
			}
		}
	}

	PortMatrixNode::State get_state (ARDOUR::BundleChannel c[2]) const
	{
		Bundle::PortList const & our_ports = c[OURS].bundle->channel_ports (c[OURS].channel);
		Bundle::PortList const & other_ports = c[OTHER].bundle->channel_ports (c[OTHER].channel);
		
		if (c[OURS].bundle == _session.click_io()->bundle()) {
			
			for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
				for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {
					Port* f = _session.engine().get_port_by_name (*i);
					assert (f);
					
					if (f->connected_to (*j)) {
						return PortMatrixNode::ASSOCIATED;
					} else {
						return PortMatrixNode::NOT_ASSOCIATED;
					}
				}
			}

		} else {

			/* XXX */

		}

		return PortMatrixNode::NOT_ASSOCIATED;
	}

	bool list_is_global (int dim) const
	{
		return (dim == OTHER);
	}

	bool can_remove_channels (int) const {
		return false;
	}
	void remove_channel (ARDOUR::BundleChannel) {}
	bool can_rename_channels (int) const {
		return false;
	}

	std::string disassociation_verb () const {
		return _("Disassociate");
	}
	
private:
	/* see PortMatrix: signal flow from 0 to 1 (out to in) */
	enum {
		OURS = 0,
		OTHER = 1,
	};

	boost::shared_ptr<PortGroup> _port_group;

};


class ConnectionOptions : public OptionEditorBox
{
public:
	ConnectionOptions (ARDOUR::Session* s)
		: _port_matrix (*s)
	{
		_box->pack_start (_port_matrix);
	}

	void parameter_changed (string const & p)
	{

	}

	void set_state_from_config ()
	{

	}

private:
	OptionsPortMatrix _port_matrix;
};

SessionOptionEditor::SessionOptionEditor (Session* s)
	: OptionEditor (&(s->config), _("Session Preferences")),
	  _session_config (&(s->config))
{
	/* FADES */

	ComboOption<CrossfadeModel>* cfm = new ComboOption<CrossfadeModel> (
		"xfade-model",
		_("Crossfades are created"),
		mem_fun (*_session_config, &SessionConfiguration::get_xfade_model),
		mem_fun (*_session_config, &SessionConfiguration::set_xfade_model)
		);

	cfm->add (FullCrossfade, _("to span entire overlap"));
	cfm->add (ShortCrossfade, _("short"));

	add_option (_("Fades"), cfm);

	add_option (_("Fades"), new SpinOption<float> (
		_("short-xfade-seconds"),
		_("Short crossfade length"),
		mem_fun (*_session_config, &SessionConfiguration::get_short_xfade_seconds),
		mem_fun (*_session_config, &SessionConfiguration::set_short_xfade_seconds),
		0, 1000, 1, 10,
		_("ms"), 0.001
			    ));

	add_option (_("Fades"), new SpinOption<float> (
		_("destructive-xfade-seconds"),
		_("Destructive crossfade length"),
		mem_fun (*_session_config, &SessionConfiguration::get_destructive_xfade_msecs),
		mem_fun (*_session_config, &SessionConfiguration::set_destructive_xfade_msecs),
		0, 1000, 1, 10,
		_("ms")
			    ));

	add_option (_("Fades"), new BoolOption (
			    "auto-xfade",
			    _("Create crossfades automatically"),
			    mem_fun (*_session_config, &SessionConfiguration::get_auto_xfade),
			    mem_fun (*_session_config, &SessionConfiguration::set_auto_xfade)
			    ));

        add_option (_("Fades"), new BoolOption (
			    "xfades-active",
			    _("Crossfades active"),
			    mem_fun (*_session_config, &SessionConfiguration::get_xfades_active),
			    mem_fun (*_session_config, &SessionConfiguration::set_xfades_active)
			    ));

	add_option (_("Fades"), new BoolOption (
			    "xfades-visible",
			    _("Crossfades visible"),
			    mem_fun (*_session_config, &SessionConfiguration::get_xfades_visible),
			    mem_fun (*_session_config, &SessionConfiguration::set_xfades_visible)
			    ));

	add_option (_("Fades"), new BoolOption (
			    "use-region-fades",
			    _("Region fades active"),
			    mem_fun (*_session_config, &SessionConfiguration::get_use_region_fades),
			    mem_fun (*_session_config, &SessionConfiguration::set_use_region_fades)
			    ));

	add_option (_("Fades"), new BoolOption (
			    "show-region-fades",
			    _("Region fades visible"),
			    mem_fun (*_session_config, &SessionConfiguration::get_show_region_fades),
			    mem_fun (*_session_config, &SessionConfiguration::set_show_region_fades)
			    ));

	/* SYNC */

	ComboOption<uint32_t>* spf = new ComboOption<uint32_t> (
		"subframes-per-frame",
		_("Subframes per frame"),
		mem_fun (*_session_config, &SessionConfiguration::get_subframes_per_frame),
		mem_fun (*_session_config, &SessionConfiguration::set_subframes_per_frame)
		);

	spf->add (80, _("80"));
	spf->add (100, _("100"));

	add_option (_("Sync"), spf);

	ComboOption<SmpteFormat>* smf = new ComboOption<SmpteFormat> (
		"smpte-format",
		_("Timecode frames-per-second"),
		mem_fun (*_session_config, &SessionConfiguration::get_smpte_format),
		mem_fun (*_session_config, &SessionConfiguration::set_smpte_format)
		);

	smf->add (smpte_23976, _("23.976"));
	smf->add (smpte_24, _("24"));
	smf->add (smpte_24976, _("24.976"));
	smf->add (smpte_25, _("25"));
	smf->add (smpte_2997, _("29.97"));
	smf->add (smpte_2997drop, _("29.97 drop"));
	smf->add (smpte_30, _("30"));
	smf->add (smpte_30drop, _("30 drop"));
	smf->add (smpte_5994, _("59.94"));
	smf->add (smpte_60, _("60"));

	add_option (_("Sync"), smf);
		
	add_option (_("Sync"), new BoolOption (
			    "timecode-source-is-synced",
			    _("Timecode source is synced"),
			    mem_fun (*_session_config, &SessionConfiguration::get_timecode_source_is_synced),
			    mem_fun (*_session_config, &SessionConfiguration::set_timecode_source_is_synced)
			    ));

	ComboOption<float>* vpu = new ComboOption<float> (
		"video-pullup",
		_("Pull-up / pull-down"),
		mem_fun (*_session_config, &SessionConfiguration::get_video_pullup),
		mem_fun (*_session_config, &SessionConfiguration::set_video_pullup)
		);

	vpu->add (4.1667 + 0.1, _("4.1667 + 0.1%"));
	vpu->add (4.1667, _("4.1667"));
	vpu->add (4.1667 - 0.1, _("4.1667 - 0.1%"));
	vpu->add (0.1, _("0.1"));
	vpu->add (0, _("none"));
	vpu->add (-0.1, _("-0.1"));
	vpu->add (-4.1667 + 0.1, _("-4.1667 + 0.1%"));
	vpu->add (-4.1667, _("-4.1667"));
	vpu->add (-4.1667 - 0.1, _("-4.1667 - 0.1%"));
		
	add_option (_("Sync"), vpu);
	
	/* MISC */

	add_option (_("Misc"), new OptionEditorHeading (_("Audio file format")));

	ComboOption<SampleFormat>* sf = new ComboOption<SampleFormat> (
		"native-file-data-format",
		_("Sample format"),
		mem_fun (*_session_config, &SessionConfiguration::get_native_file_data_format),
		mem_fun (*_session_config, &SessionConfiguration::set_native_file_data_format)
		);

	sf->add (FormatFloat, _("32-bit floating point"));
	sf->add (FormatInt24, _("24-bit integer"));
	sf->add (FormatInt16, _("16-bit integer"));

	add_option (_("Misc"), sf);

	ComboOption<HeaderFormat>* hf = new ComboOption<HeaderFormat> (
		"native-file-header-format",
		_("File type"),
		mem_fun (*_session_config, &SessionConfiguration::get_native_file_header_format),
		mem_fun (*_session_config, &SessionConfiguration::set_native_file_header_format)
		);

	hf->add (BWF, _("Broadcast WAVE"));
	hf->add (WAVE, _("WAVE"));
	hf->add (WAVE64, _("WAVE-64"));
	hf->add (CAF, _("CAF"));

	add_option (_("Misc"), hf);

	add_option (_("Misc"), new OptionEditorHeading (_("Layering")));

	ComboOption<LayerModel>* lm = new ComboOption<LayerModel> (
		"layer-model",
		_("Layering model"),
		mem_fun (*_session_config, &SessionConfiguration::get_layer_model),
		mem_fun (*_session_config, &SessionConfiguration::set_layer_model)
		);

	lm->add (LaterHigher, _("later is higher"));
	lm->add (MoveAddHigher, _("most recently moved or added is higher"));
	lm->add (AddHigher, _("most recently added is higher"));

	add_option (_("Misc"), lm);
	
	add_option (_("Misc"), new OptionEditorHeading (_("Broadcast WAVE metadata")));
	
	add_option (_("Misc"), new EntryOption (
			    "bwf-country-code",
			    _("Country code"),
			    mem_fun (*_session_config, &SessionConfiguration::get_bwf_country_code),
			    mem_fun (*_session_config, &SessionConfiguration::set_bwf_country_code)
			    ));

	add_option (_("Misc"), new EntryOption (
			    "bwf-organization-code",
			    _("Organization code"),
			    mem_fun (*_session_config, &SessionConfiguration::get_bwf_organization_code),
			    mem_fun (*_session_config, &SessionConfiguration::set_bwf_organization_code)
			    ));

	add_option (_("Connections"), new ConnectionOptions (s));
}
