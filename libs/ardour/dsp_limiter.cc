/*
 * Copyright (C) 2010-2018 Fons Adriaensen <fons@linuxaudio.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "pbd/controllable.h"
#include "pbd/error.h"

#include "ardour/audio_buffer.h"
#include "ardour/dsp_limiter.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

void
Limiter::Histmin::init (int hlen)
{
	assert (hlen <= SIZE);
	_hlen = hlen;
	_hold = hlen;
	_wind = 0;
	_vmin = 1;
	for (int i = 0; i < SIZE; i++)
		_hist[i] = _vmin;
}

float
Limiter::Histmin::write (float v)
{
	int i    = _wind;
	_hist[i] = v;

	if (v <= _vmin) {
		_vmin = v;
		_hold = _hlen;
	} else if (--_hold == 0) {
		_vmin = v;
		_hold = _hlen;
		for (int j = 1 - _hlen; j < 0; j++) {
			v = _hist[(i + j) & MASK];
			if (v < _vmin) {
				_vmin = v;
				_hold = _hlen + j;
			}
		}
	}
	_wind = ++i & MASK;
	return _vmin;
}

/* ****************************************************************************/

static boost::shared_ptr<AutomationControl>
forge_control (Session& s, uint32_t idx)
{
	Evoral::Parameter       param (PluginAutomation, 0, idx);
	ParameterDescriptor     desc;
	PBD::Controllable::Flag flag = PBD::Controllable::Flag (0);

	switch (idx) {
		default:
		case 0:
			desc.label   = "Enable";
			desc.type    = PluginAutomation;
			desc.lower   = 0.0;
			desc.upper   = 1.0;
			desc.normal  = 0.0;
			desc.toggled = true;
			flag         = PBD::Controllable::Toggle;
			break;
		case 1:
			desc.label     = "Threshold";
			desc.type      = PluginAutomation;
			desc.lower     = -10;
			desc.upper     = 0;
			desc.normal    = -1;
			desc.unit      = ParameterDescriptor::DB;
			desc.print_fmt = "%.1f dB";
			break;
		case 2:
			desc.label       = "Release Time";
			desc.type        = PluginAutomation;
			desc.lower       = 1;
			desc.upper       = 1000;
			desc.normal      = 10;
			desc.logarithmic = true;
			desc.print_fmt   = "%.0fms";
			break;
		case 3:
			desc.label   = "TruePeak";
			desc.type    = PluginAutomation;
			desc.lower   = 0.0;
			desc.upper   = 1.0;
			desc.normal  = 1.0;
			desc.toggled = true;
			flag         = PBD::Controllable::Toggle;
			break;
	}

	desc.update_steps ();

	boost::shared_ptr<AutomationList>    list (new AutomationList (param, desc));
	boost::shared_ptr<AutomationControl> c (new AutomationControl (s, param, desc, list, desc.label, flag));
	return c;
}

Limiter::Limiter (Session& s, const std::string& name)
    : Processor (s, name)
    , _dly_buf (0)
    , _z (0)
    , _zlf (0)
    , _nchan (0)
    , _processing (false)
    , _truepeak (false)
    , _threshold (0)
    , _release_time (0)
    , _peak (0)
    , _redux (-20)
{
	_enable_ctrl    = forge_control (_session, 0);
	_threshold_ctrl = forge_control (_session, 1);
	_release_ctrl   = forge_control (_session, 2);
	_truepeak_ctrl  = forge_control (_session, 3);

	add_control (_threshold_ctrl);
	add_control (_release_ctrl);
	add_control (_truepeak_ctrl);

	ParameterDescriptor desc;
	desc.label     = "Redux";
	desc.type      = PluginAutomation;
	desc.lower     = 0.0; // -10.0;
	desc.upper     = 20.0;
	desc.normal    = 0.0;
	desc.unit      = ParameterDescriptor::DB;
	desc.print_fmt = "%.1f dB";

	_redux_ctrl = boost::shared_ptr<ReadOnlyControl> (new ReadOnlyControl (this, desc, 4));
}

Limiter::~Limiter (void)
{
	fini ();
	if (_processing) {
		LatencyChanged ();
	}
}

float
Limiter::get_parameter (uint32_t which) const
{
	switch (which) {
		case 0:
			return _enable_ctrl->get_value ();
		case 1:
			return _threshold_ctrl->get_value ();
		case 2:
			return _release_ctrl->get_value ();
		case 3:
			return _truepeak_ctrl->get_value ();
		case 4:
			return _redux;
		default:
			assert (0);
			return 0;
	}
}

std::string
Limiter::describe_parameter (Evoral::Parameter which)
{
	if (which.type () != PluginAutomation || which.id () > 4) {
		return "??";
	}
	switch (which.id ()) {
		case 0:
			return _enable_ctrl->desc ().label;
		case 1:
			return _threshold_ctrl->desc ().label;
		case 2:
			return _release_ctrl->desc ().label;
		case 3:
			return _truepeak_ctrl->desc ().label;
		case 4:
			return _redux_ctrl->desc ().label;
		default:
			assert (0);
			return "??";
	}
}

bool
Limiter::enabled () const
{
	return _enable_ctrl->get_value () > 0 && _pending_active;
}

void
Limiter::enable (bool yn)
{
	if (!_pending_active) {
		activate ();
	}
	if (enabled () == yn) {
		return;
	}
	const double val = yn ? 1.0 : 0.0;
	_enable_ctrl->set_value (val, PBD::Controllable::NoGroup);
	ActiveChanged ();
}

XMLNode&
Limiter::get_state ()
{
	XMLNode& node (Processor::state ());
	node.set_property ("type", X_("Limiter"));

	for (Controls::iterator c = controls ().begin (); c != controls ().end (); ++c) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> ((*c).second);
		if (!ac) {
			continue;
		}
		XMLNode& n (ac->get_state ());
		n.set_property (X_("parameter"), ac->parameter ().id ());
		node.add_child_nocopy (n);
	}

	return node;
}

int
Limiter::set_state (const XMLNode& node, int version)
{
	std::string str;
	if (!node.get_property ("type", str) || str != X_("Limiter")) {
		PBD::error << _("XML node describing the `Limiter' is missing the `type' field") << endmsg;
		return -1;
	}

	XMLNodeList nlist = node.children ();
	for (XMLNodeIterator iter = nlist.begin (); iter != nlist.end (); ++iter) {
		if ((*iter)->name () != PBD::Controllable::xml_node_name) {
			continue;
		}
		uint32_t p;
		if (!(*iter)->get_property (X_("parameter"), p)) {
			continue;
		}
		boost::shared_ptr<Evoral::Control> c = control (Evoral::Parameter (PluginAutomation, 0, p));
		if (!c) {
			continue;
		}
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c);
		if (!ac) {
			continue;
		}
		ac->set_state (**iter, version);
	}
	return Processor::set_state (node, version);
}

void
Limiter::set_threshold (float v)
{
	if (_threshold == v) {
		return;
	}
	_threshold = v;
	_gt        = powf (10.f, -0.05f * v);
}

void
Limiter::set_release (float v)
{
	if (v == _release_time) {
		return;
	}
	_release_time = v;
	if (v > 1.f) {
		v = 1.f;
	}
	if (v < 1e-3f) {
		v = 1e-3f;
	}
	_w3 = 1.f / (v * _session.nominal_sample_rate ());
}

void
Limiter::set_truepeak (bool v)
{
	if (_truepeak == v) {
		return;
	}
	for (uint32_t i = 0; i < _nchan; i++) {
		for (int j = 0; j < 48; ++j) {
			_z[i][j] = 0.f;
		}
	}
	_truepeak = v;
}

void
Limiter::init (uint32_t nchan)
{
	if (nchan == _nchan) {
		return;
	}

	fini ();
	_processing = false;

	if (nchan == 0) {
		return;
	}

	samplecnt_t fsamp = _session.nominal_sample_rate ();

	_div3 = fsamp * 0.05; // 50 ms

	if (fsamp > 130000) {
		_div1 = 32;
	} else if (fsamp > 65000) {
		_div1 = 16;
	} else {
		_div1 = 8;
	}

	_nchan = nchan;
	_div2  = 8;
	int k1 = (int)(ceilf (1.2e-3f * fsamp / _div1));
	int k2 = 12;
	_delay = k1 * _div1;

	pframes_t dly_size;
	for (dly_size = 64; dly_size < _delay + _div1; dly_size *= 2) ;
	_dly_mask = dly_size - 1;
	_dly_ridx = 0;

	_dly_buf = new float*[_nchan];
	_zlf     = new float[_nchan];
	_z       = new float*[_nchan];

	for (uint32_t i = 0; i < _nchan; i++) {
		_dly_buf[i] = new float[dly_size];
		_z[i]       = new float[48];
		memset (_dly_buf[i], 0, dly_size * sizeof (float));
		memset (_z[i], 0, 48 * sizeof (float));
		_zlf[i] = 0.f;
	}

	_hist1.init (k1 + 1);
	_hist2.init (k2);

	_c1  = _div1;
	_c2  = _div2;
	_c3  = _div3;
	_c4  = _div3;
	_m1  = 0.f;
	_m2  = 0.f;
	_wlf = 6.28f * 500.f / fsamp;
	_w1  = 10.f / _delay;
	_w2  = _w1 / _div2;
	_w3  = 1.f / (0.01f * fsamp);
	_z1  = 1.f;
	_z2  = 1.f;
	_z3  = 1.f;
	_gt  = 1.f;

	_peak         = 0;
	_redux        = -20;
	_threshold    = 0;
	_release_time = 0;
}

void
Limiter::fini (void)
{
	for (uint32_t i = 0; i < _nchan; i++) {
		delete[] _dly_buf[i];
		delete[] _z[i];
		_dly_buf[i] = 0;
		_z[i]       = 0;
	}
	delete[] _dly_buf;
	delete[] _z;
	delete[] _zlf;
	_dly_buf = 0;
	_zlf     = 0;
	_z       = 0;
	_nchan   = 0;
}

bool
Limiter::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
Limiter::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}
	init (in.n_audio ());
	return Processor::configure_io (in, out);
}

samplecnt_t
Limiter::signal_latency () const
{
	return _processing ? _delay : 0;
}

void
Limiter::run (BufferSet& bufs, samplepos_t, samplepos_t, double, pframes_t nframes, bool)
{
	if (_nchan == 0) {
		_redux = -20;
		assert (_processing == false);
		return;
	}

	bool en = enabled ();

	set_truepeak (_truepeak_ctrl->get_value () > 0);
	set_release (_release_ctrl->get_value () / 1000.f);

	if (!en) {
		set_threshold (40.0); /* ask to bypass */
		if (_processing && (_z3 >= 0.9 || _c4 > _div3 + _div3)) {
			/* gain reduction is < 1dB or 100ms timeout.
			 * Due to latency change, bypass isn't click-free to begin with
			 */
			_processing = false;
			LatencyChanged ();
		} else if (_processing) {
			_c4 += nframes;
		}
	} else {
		_c4 = 0;
		set_threshold (_threshold_ctrl->get_value ());
		if (!_processing) {
			_processing = true;
			LatencyChanged ();
		}
	}

	if (!_processing) {
		_redux = -20;
		return;
	}

	process (bufs, nframes);

	_c3 += nframes;
	if (_c3 > _div3) {
		_c3 -= _div3;
		float pk = _peak < 0.1 ? -20 : (20. * log10f (_peak));
		_peak    = 0;

		if (_redux > -20) {
			_redux -= .3; // 6dB/sec
		}
		if (pk > _redux) {
			_redux = pk;
		}
	}

	_active = _pending_active;
}

void
Limiter::process (BufferSet& bufs, pframes_t nframes)
{
	pframes_t ri, wi;
	float     h1, h2, m1, m2, z1, z2, z3, pk;
	Sample*   p;

	assert (bufs.count ().n_audio () == _nchan);

	pk = _peak;
	ri = _dly_ridx;
	wi = (ri + _delay) & _dly_mask;
	h1 = _hist1.vmin ();
	h2 = _hist2.vmin ();
	m1 = _m1;
	m2 = _m2;
	z1 = _z1;
	z2 = _z2;
	z3 = _z3;

	pframes_t k = 0;
	while (nframes) {
		pframes_t n = (_c1 < nframes) ? _c1 : nframes;

		for (uint32_t j = 0; j < _nchan; j++) {
			p = bufs.get_audio (j).data (k);
			float z = _zlf[j];
			for (pframes_t i = 0; i < n; i++) {
				float x = *p++;
				_dly_buf[j][wi + i] = x;
				z += _wlf * (x - z) + 1e-20f;

				if (_truepeak) {
					float* r = _z[j];
					float  u[4];
					r[47] = x;
					/* 4x upsample for true-peak analysis, cosine windowed sinc */
					/* clang-format off */
					u[0] = r[47];
					u[1] = r[ 0] * -2.330790e-05f + r[ 1] * +1.321291e-04f + r[ 2] * -3.394408e-04f + r[ 3] * +6.562235e-04f
					     + r[ 4] * -1.094138e-03f + r[ 5] * +1.665807e-03f + r[ 6] * -2.385230e-03f + r[ 7] * +3.268371e-03f
					     + r[ 8] * -4.334012e-03f + r[ 9] * +5.604985e-03f + r[10] * -7.109989e-03f + r[11] * +8.886314e-03f
					     + r[12] * -1.098403e-02f + r[13] * +1.347264e-02f + r[14] * -1.645206e-02f + r[15] * +2.007155e-02f
					     + r[16] * -2.456432e-02f + r[17] * +3.031531e-02f + r[18] * -3.800644e-02f + r[19] * +4.896667e-02f
					     + r[20] * -6.616853e-02f + r[21] * +9.788141e-02f + r[22] * -1.788607e-01f + r[23] * +9.000753e-01f
					     + r[24] * +2.993829e-01f + r[25] * -1.269367e-01f + r[26] * +7.922398e-02f + r[27] * -5.647748e-02f
					     + r[28] * +4.295093e-02f + r[29] * -3.385706e-02f + r[30] * +2.724946e-02f + r[31] * -2.218943e-02f
					     + r[32] * +1.816976e-02f + r[33] * -1.489313e-02f + r[34] * +1.217411e-02f + r[35] * -9.891211e-03f
					     + r[36] * +7.961470e-03f + r[37] * -6.326144e-03f + r[38] * +4.942202e-03f + r[39] * -3.777065e-03f
					     + r[40] * +2.805240e-03f + r[41] * -2.006106e-03f + r[42] * +1.362416e-03f + r[43] * -8.592768e-04f
					     + r[44] * +4.834383e-04f + r[45] * -2.228007e-04f + r[46] * +6.607267e-05f + r[47] * -2.537056e-06f;
					u[2] = r[ 0] * -1.450055e-05f + r[ 1] * +1.359163e-04f + r[ 2] * -3.928527e-04f + r[ 3] * +8.006445e-04f
					     + r[ 4] * -1.375510e-03f + r[ 5] * +2.134915e-03f + r[ 6] * -3.098103e-03f + r[ 7] * +4.286860e-03f
					     + r[ 8] * -5.726614e-03f + r[ 9] * +7.448018e-03f + r[10] * -9.489286e-03f + r[11] * +1.189966e-02f
					     + r[12] * -1.474471e-02f + r[13] * +1.811472e-02f + r[14] * -2.213828e-02f + r[15] * +2.700557e-02f
					     + r[16] * -3.301023e-02f + r[17] * +4.062971e-02f + r[18] * -5.069345e-02f + r[19] * +6.477499e-02f
					     + r[20] * -8.625619e-02f + r[21] * +1.239454e-01f + r[22] * -2.101678e-01f + r[23] * +6.359382e-01f
					     + r[24] * +6.359382e-01f + r[25] * -2.101678e-01f + r[26] * +1.239454e-01f + r[27] * -8.625619e-02f
					     + r[28] * +6.477499e-02f + r[29] * -5.069345e-02f + r[30] * +4.062971e-02f + r[31] * -3.301023e-02f
					     + r[32] * +2.700557e-02f + r[33] * -2.213828e-02f + r[34] * +1.811472e-02f + r[35] * -1.474471e-02f
					     + r[36] * +1.189966e-02f + r[37] * -9.489286e-03f + r[38] * +7.448018e-03f + r[39] * -5.726614e-03f
					     + r[40] * +4.286860e-03f + r[41] * -3.098103e-03f + r[42] * +2.134915e-03f + r[43] * -1.375510e-03f
					     + r[44] * +8.006445e-04f + r[45] * -3.928527e-04f + r[46] * +1.359163e-04f + r[47] * -1.450055e-05f;
					u[3] = r[ 0] * -2.537056e-06f + r[ 1] * +6.607267e-05f + r[ 2] * -2.228007e-04f + r[ 3] * +4.834383e-04f
					     + r[ 4] * -8.592768e-04f + r[ 5] * +1.362416e-03f + r[ 6] * -2.006106e-03f + r[ 7] * +2.805240e-03f
					     + r[ 8] * -3.777065e-03f + r[ 9] * +4.942202e-03f + r[10] * -6.326144e-03f + r[11] * +7.961470e-03f
					     + r[12] * -9.891211e-03f + r[13] * +1.217411e-02f + r[14] * -1.489313e-02f + r[15] * +1.816976e-02f
					     + r[16] * -2.218943e-02f + r[17] * +2.724946e-02f + r[18] * -3.385706e-02f + r[19] * +4.295093e-02f
					     + r[20] * -5.647748e-02f + r[21] * +7.922398e-02f + r[22] * -1.269367e-01f + r[23] * +2.993829e-01f
					     + r[24] * +9.000753e-01f + r[25] * -1.788607e-01f + r[26] * +9.788141e-02f + r[27] * -6.616853e-02f
					     + r[28] * +4.896667e-02f + r[29] * -3.800644e-02f + r[30] * +3.031531e-02f + r[31] * -2.456432e-02f
					     + r[32] * +2.007155e-02f + r[33] * -1.645206e-02f + r[34] * +1.347264e-02f + r[35] * -1.098403e-02f
					     + r[36] * +8.886314e-03f + r[37] * -7.109989e-03f + r[38] * +5.604985e-03f + r[39] * -4.334012e-03f
					     + r[40] * +3.268371e-03f + r[41] * -2.385230e-03f + r[42] * +1.665807e-03f + r[43] * -1.094138e-03f
					     + r[44] * +6.562235e-04f + r[45] * -3.394408e-04f + r[46] * +1.321291e-04f + r[47] * -2.330790e-05f;
					/* clang-format on */

					for (int i = 0; i < 47; ++i) {
						r[i] = r[i + 1];
					}

					float p1 = std::max (fabsf (u[0]), fabsf (u[1]));
					float p2 = std::max (fabsf (u[2]), fabsf (u[3]));
					x        = std::max (p1, p2);

				} else {
					x = fabsf (x);
				}

				if (x > m1) {
					m1 = x;
				}
				x = fabsf (z);
				if (x > m2) {
					m2 = x;
				}
			}
			_zlf[j] = z;
		}

		_c1 -= n;
		if (_c1 == 0) {
			m1 *= _gt;
			if (m1 > pk) {
				pk = m1;
			}
			h1  = (m1 > 1.f) ? 1.f / m1 : 1.f;
			h1  = _hist1.write (h1);
			m1  = 0;
			_c1 = _div1;
			if (--_c2 == 0) {
				m2 *= _gt;
				h2  = (m2 > 1.f) ? 1.f / m2 : 1.f;
				h2  = _hist2.write (h2);
				m2  = 0;
				_c2 = _div2;
			}
		}

		for (pframes_t i = 0; i < n; ++i) {
			z1 += _w1 * (h1 - z1);
			z2 += _w2 * (h2 - z2);
			float z = (z2 < z1) ? z2 : z1;
			if (z < z3) {
				z3 += _w1 * (z - z3);
			} else {
				z3 += _w3 * (z - z3);
			}
			for (uint32_t j = 0; j < _nchan; j++) {
				Sample* out = bufs.get_audio (j).data (k);
				out[i]      = z3 * _dly_buf[j][ri + i];
			}
		}

		wi = (wi + n) & _dly_mask;
		ri = (ri + n) & _dly_mask;
		k += n;
		nframes -= n;
	}

	_peak     = pk;
	_dly_ridx = ri;

	_m1 = m1;
	_m2 = m2;
	_z1 = z1;
	_z2 = z2;
	_z3 = z3;
}
