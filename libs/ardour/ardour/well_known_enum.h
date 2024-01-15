/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef _libardour_well_known_enum_h_
#define _libardour_well_known_enum_h_

namespace ARDOUR {

enum WellKnownCtrl : int
{
	EQ_Enable,
	EQ_Mode,
	EQ_BandGain,
	EQ_BandFreq,
	EQ_BandQ,
	EQ_BandShape,

	HPF_Enable,
	HPF_Freq,
	HPF_Slope,

	LPF_Enable,
	LPF_Freq,
	LPF_Slope,

	TapeDrive_Drive,
	TapeDrive_Mode,

	Comp_Enable,
	Comp_Mode,
	Comp_Threshold,
	Comp_Makeup,
	Comp_Ratio,
	Comp_Attack,
	Comp_Release,
	Comp_KeyFilterFreq,
	Comp_Lookahead,

	Gate_Enable,
	Gate_Mode,
	Gate_Threshold,
	Gate_Ratio,
	Gate_Knee,
	Gate_Depth,
	Gate_Hysteresis,
	Gate_Hold,
	Gate_Attack,
	Gate_Release,
	Gate_KeyListen,
	Gate_KeyFilterEnable,
	Gate_KeyFilterFreq,
	Gate_Lookahead,

	Master_Limiter_Enable,
};

enum WellKnownData : int
{
	TapeDrive_Saturation,
	Master_PhaseCorrelationMin,
	Master_PhaseCorrelationMax,
	Master_KMeter,
	Master_LimiterRedux,
	Comp_Meter,
	Comp_Redux,
	Gate_Meter,
	Gate_Redux,
};

} /* namespace ARDOUR */
#endif
