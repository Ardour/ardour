/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ardour/export_format_base.h"

namespace ARDOUR
{

void
ExportFormatBase::SelectableCompatible::set_selected (bool value)
{
	if (_selected != value) {
		_selected = value;
		SelectChanged (value);
	}
}

void
ExportFormatBase::SelectableCompatible::set_compatible (bool value)
{
	if (_compatible != value) {
		_compatible = value;
		CompatibleChanged (value);
	}
	if (!value) {
		set_selected (false);
	}
}

ExportFormatBase::ExportFormatBase ()
{

}

ExportFormatBase::ExportFormatBase (ExportFormatBase const & other) :
  sample_formats (other.sample_formats),
  endiannesses (other.endiannesses),
  sample_rates (other.sample_rates),
  format_ids (other.format_ids),
  qualities (other.qualities)
{

}

ExportFormatBase::~ExportFormatBase ()
{

}

boost::shared_ptr<ExportFormatBase>
ExportFormatBase::get_intersection (ExportFormatBase const & other) const
{
	return do_set_operation (other, SetIntersection);
}

boost::shared_ptr<ExportFormatBase>
ExportFormatBase::get_union (ExportFormatBase const & other) const
{
	return do_set_operation (other, SetUnion);
}

boost::shared_ptr<ExportFormatBase>
ExportFormatBase::do_set_operation (ExportFormatBase const & other, SetOperation operation) const
{
	boost::shared_ptr<ExportFormatBase> result (new ExportFormatBase ());

	/* Sets */

	// Endiannesses
	{
		EndianSet::const_iterator start1 = endiannesses.begin();
		EndianSet::const_iterator end1 = endiannesses.end();
		EndianSet::const_iterator start2 = other.endiannesses.begin();
		EndianSet::const_iterator end2 = other.endiannesses.end();
		std::insert_iterator<EndianSet> insert (result->endiannesses, result->endiannesses.begin());

		switch (operation) {
		  case SetIntersection:
			std::set_intersection (start1, end1, start2, end2, insert);
			break;
		  case SetUnion:
			std::set_union (start1, end1, start2, end2, insert);
			break;
		}
	}

	// Sample formats
	{
		SampleFormatSet::const_iterator start1 = sample_formats.begin();
		SampleFormatSet::const_iterator end1 = sample_formats.end();
		SampleFormatSet::const_iterator start2 = other.sample_formats.begin();
		SampleFormatSet::const_iterator end2 = other.sample_formats.end();
		std::insert_iterator<SampleFormatSet> insert (result->sample_formats, result->sample_formats.begin());

		switch (operation) {
		  case SetIntersection:
			std::set_intersection (start1, end1, start2, end2, insert);
			break;
		  case SetUnion:
			std::set_union (start1, end1, start2, end2, insert);
			break;
		}
	}


	// Sample rates
	{
		SampleRateSet::const_iterator start1 = sample_rates.begin();
		SampleRateSet::const_iterator end1 = sample_rates.end();
		SampleRateSet::const_iterator start2 = other.sample_rates.begin();
		SampleRateSet::const_iterator end2 = other.sample_rates.end();
		std::insert_iterator<SampleRateSet> insert (result->sample_rates, result->sample_rates.begin());

		switch (operation) {
		  case SetIntersection:
			std::set_intersection (start1, end1, start2, end2, insert);
			break;
		  case SetUnion:
			std::set_union (start1, end1, start2, end2, insert);
			break;
		}
	}

	// Format ids
	{
		FormatSet::const_iterator start1 = format_ids.begin();
		FormatSet::const_iterator end1 = format_ids.end();
		FormatSet::const_iterator start2 = other.format_ids.begin();
		FormatSet::const_iterator end2 = other.format_ids.end();
		std::insert_iterator<FormatSet> insert (result->format_ids, result->format_ids.begin());

		switch (operation) {
		  case SetIntersection:
			std::set_intersection (start1, end1, start2, end2, insert);
			break;
		  case SetUnion:
			std::set_union (start1, end1, start2, end2, insert);
			break;
		}
	}

	// Qualities
	{
		QualitySet::const_iterator start1 = qualities.begin();
		QualitySet::const_iterator end1 = qualities.end();
		QualitySet::const_iterator start2 = other.qualities.begin();
		QualitySet::const_iterator end2 = other.qualities.end();
		std::insert_iterator<QualitySet> insert (result->qualities, result->qualities.begin());

		switch (operation) {
		  case SetIntersection:
			std::set_intersection (start1, end1, start2, end2, insert);
			break;
		  case SetUnion:
			std::set_union (start1, end1, start2, end2, insert);
			break;
		}
	}

	return result;
}

ExportFormatBase::SampleRate
ExportFormatBase::nearest_sample_rate (framecnt_t sample_rate)
{
	int diff = 0;
	int smallest_diff = INT_MAX;
	SampleRate best_match = SR_None;

	#define DO_SR_COMPARISON(rate) \
	diff = std::fabs((double)((rate) - sample_rate)); \
	if(diff < smallest_diff) { \
		smallest_diff = diff; \
		best_match = (rate); \
	}

	DO_SR_COMPARISON(SR_8);
	DO_SR_COMPARISON(SR_22_05);
	DO_SR_COMPARISON(SR_44_1);
	DO_SR_COMPARISON(SR_48);
	DO_SR_COMPARISON(SR_88_2);
	DO_SR_COMPARISON(SR_96);
	DO_SR_COMPARISON(SR_192);

	return best_match;
	#undef DO_SR_COMPARISON
}

}; // namespace ARDOUR
