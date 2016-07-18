/*
  Copyright (C) 2008-2012 Paul Davis
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

#include "ardour/export_graph_builder.h"

#include <vector>

#include <glibmm/miscutils.h>

#include "audiographer/process_context.h"
#include "audiographer/general/chunker.h"
#include "audiographer/general/interleaver.h"
#include "audiographer/general/normalizer.h"
#include "audiographer/general/analyser.h"
#include "audiographer/general/peak_reader.h"
#include "audiographer/general/loudness_reader.h"
#include "audiographer/general/sample_format_converter.h"
#include "audiographer/general/sr_converter.h"
#include "audiographer/general/silence_trimmer.h"
#include "audiographer/general/threader.h"
#include "audiographer/sndfile/tmp_file.h"
#include "audiographer/sndfile/tmp_file_rt.h"
#include "audiographer/sndfile/tmp_file_sync.h"
#include "audiographer/sndfile/sndfile_writer.h"

#include "ardour/audioengine.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_timespan.h"
#include "ardour/session_directory.h"
#include "ardour/sndfile_helpers.h"

#include "pbd/file_utils.h"
#include "pbd/cpus.h"

using namespace AudioGrapher;
using std::string;

namespace ARDOUR {

ExportGraphBuilder::ExportGraphBuilder (Session const & session)
	: session (session)
	, thread_pool (hardware_concurrency())
{
	process_buffer_frames = session.engine().samples_per_cycle();
}

ExportGraphBuilder::~ExportGraphBuilder ()
{
}

int
ExportGraphBuilder::process (framecnt_t frames, bool last_cycle)
{
	assert(frames <= process_buffer_frames);

	for (ChannelMap::iterator it = channels.begin(); it != channels.end(); ++it) {
		Sample const * process_buffer = 0;
		it->first->read (process_buffer, frames);
		ConstProcessContext<Sample> context(process_buffer, frames, 1);
		if (last_cycle) { context().set_flag (ProcessContext<Sample>::EndOfInput); }
		it->second->process (context);
	}

	return 0;
}

bool
ExportGraphBuilder::post_process ()
{
	for (std::list<Intermediate *>::iterator it = intermediates.begin(); it != intermediates.end(); /* ++ in loop */) {
		if ((*it)->process()) {
			it = intermediates.erase (it);
		} else {
			++it;
		}
	}

	return intermediates.empty();
}

unsigned
ExportGraphBuilder::get_postprocessing_cycle_count() const
{
	unsigned max = 0;
	for (std::list<Intermediate *>::const_iterator it = intermediates.begin(); it != intermediates.end(); ++it) {
		max = std::max(max, (*it)->get_postprocessing_cycle_count());
	}
	return max;
}

void
ExportGraphBuilder::reset ()
{
	timespan.reset();
	channel_configs.clear ();
	channels.clear ();
	intermediates.clear ();
	analysis_map.clear();
	_realtime = false;
}

void
ExportGraphBuilder::cleanup (bool remove_out_files/*=false*/)
{
	ChannelConfigList::iterator iter = channel_configs.begin();

	while (iter != channel_configs.end() ) {
		iter->remove_children(remove_out_files);
		iter = channel_configs.erase(iter);
	}
}

void
ExportGraphBuilder::set_current_timespan (boost::shared_ptr<ExportTimespan> span)
{
	timespan = span;
}

void
ExportGraphBuilder::add_config (FileSpec const & config, bool rt)
{
	ExportChannelConfiguration::ChannelList const & channels =
		config.channel_config->get_channels();
	for(ExportChannelConfiguration::ChannelList::const_iterator it = channels.begin();
	    it != channels.end(); ++it) {
		(*it)->set_max_buffer_size(process_buffer_frames);
	}

	_realtime = rt;

	// If the sample rate is "session rate", change it to the real value.
	// However, we need to copy it to not change the config which is saved...
	FileSpec new_config (config);
	new_config.format.reset(new ExportFormatSpecification(*new_config.format, false));
	if(new_config.format->sample_rate() == ExportFormatBase::SR_Session) {
		framecnt_t session_rate = session.nominal_frame_rate();
		new_config.format->set_sample_rate(ExportFormatBase::nearest_sample_rate(session_rate));
	}


	if (!new_config.channel_config->get_split ()) {
		add_split_config (new_config);
		return;
	}

	// Split channel configurations are split into several channel configurations,
	// each corresponding to a file, at this stage
	typedef std::list<boost::shared_ptr<ExportChannelConfiguration> > ConfigList;
	ConfigList file_configs;
	new_config.channel_config->configurations_for_files (file_configs);

	unsigned chan = 1;
	for (ConfigList::iterator it = file_configs.begin(); it != file_configs.end(); ++it, ++chan) {
		FileSpec copy = new_config;
		copy.channel_config = *it;

		copy.filename.reset (new ExportFilename (*copy.filename));
		copy.filename->include_channel = true;
		copy.filename->set_channel (chan);

		add_split_config (copy);
	}
}

void
ExportGraphBuilder::get_analysis_results (AnalysisResults& results) {
	for (AnalysisMap::iterator i = analysis_map.begin(); i != analysis_map.end(); ++i) {
		ExportAnalysisPtr p = i->second->result ();
		if (p) {
			results.insert (std::make_pair (i->first, p));
		}
	}
}

void
ExportGraphBuilder::add_split_config (FileSpec const & config)
{
	for (ChannelConfigList::iterator it = channel_configs.begin(); it != channel_configs.end(); ++it) {
		if (*it == config) {
			it->add_child (config);
			return;
		}
	}

	// No duplicate channel config found, create new one
	channel_configs.push_back (new ChannelConfig (*this, config, channels));
}

/* Encoder */

template <>
boost::shared_ptr<AudioGrapher::Sink<Sample> >
ExportGraphBuilder::Encoder::init (FileSpec const & new_config)
{
	config = new_config;
	init_writer (float_writer);
	return float_writer;
}

template <>
boost::shared_ptr<AudioGrapher::Sink<int> >
ExportGraphBuilder::Encoder::init (FileSpec const & new_config)
{
	config = new_config;
	init_writer (int_writer);
	return int_writer;
}

template <>
boost::shared_ptr<AudioGrapher::Sink<short> >
ExportGraphBuilder::Encoder::init (FileSpec const & new_config)
{
	config = new_config;
	init_writer (short_writer);
	return short_writer;
}

void
ExportGraphBuilder::Encoder::add_child (FileSpec const & new_config)
{
	filenames.push_back (new_config.filename);
}

void
ExportGraphBuilder::Encoder::destroy_writer (bool delete_out_file)
{
	if (delete_out_file ) {

		if (float_writer) {
			float_writer->close ();
		}

		if (int_writer) {
			int_writer->close ();
		}

		if (short_writer) {
			short_writer->close ();
		}

		if (std::remove(writer_filename.c_str() ) != 0) {
			std::cout << "Encoder::destroy_writer () : Error removing file: " << strerror(errno) << std::endl;
		}
	}

	float_writer.reset ();
	int_writer.reset ();
	short_writer.reset ();
}

bool
ExportGraphBuilder::Encoder::operator== (FileSpec const & other_config) const
{
	return get_real_format (config) == get_real_format (other_config);
}

int
ExportGraphBuilder::Encoder::get_real_format (FileSpec const & config)
{
	ExportFormatSpecification & format = *config.format;
	return format.format_id() | format.sample_format() | format.endianness();
}

template<typename T>
void
ExportGraphBuilder::Encoder::init_writer (boost::shared_ptr<AudioGrapher::SndfileWriter<T> > & writer)
{
	unsigned channels = config.channel_config->get_n_chans();
	int format = get_real_format (config);
	config.filename->set_channel_config(config.channel_config);
	writer_filename = config.filename->get_path (config.format);

	writer.reset (new AudioGrapher::SndfileWriter<T> (writer_filename, format, channels, config.format->sample_rate(), config.broadcast_info));
	writer->FileWritten.connect_same_thread (copy_files_connection, boost::bind (&ExportGraphBuilder::Encoder::copy_files, this, _1));
}

void
ExportGraphBuilder::Encoder::copy_files (std::string orig_path)
{
	while (filenames.size()) {
		ExportFilenamePtr & filename = filenames.front();
		PBD::copy_file (orig_path, filename->get_path (config.format).c_str());
		filenames.pop_front();
	}
}

/* SFC */

ExportGraphBuilder::SFC::SFC (ExportGraphBuilder &parent, FileSpec const & new_config, framecnt_t max_frames)
	: data_width(0)
{
	config = new_config;
	data_width = sndfile_data_width (Encoder::get_real_format (config));
	unsigned channels = new_config.channel_config->get_n_chans();
	_analyse = config.format->analyse();
	if (_analyse) {
		framecnt_t sample_rate = parent.session.nominal_frame_rate();
		framecnt_t sb = config.format->silence_beginning_at (parent.timespan->get_start(), sample_rate);
		framecnt_t se = config.format->silence_end_at (parent.timespan->get_end(), sample_rate);
		framecnt_t duration = parent.timespan->get_length () + sb + se;
		max_frames = min ((framecnt_t) 8192 * channels, max ((framecnt_t) 4096 * channels, max_frames));
		chunker.reset (new Chunker<Sample> (max_frames));
		analyser.reset (new Analyser (config.format->sample_rate(), channels, max_frames,
					(framecnt_t) ceil (duration * config.format->sample_rate () / (double) sample_rate)));
		chunker->add_output (analyser);

		config.filename->set_channel_config (config.channel_config);
		parent.add_analyser (config.filename->get_path (config.format), analyser);
	}

	if (data_width == 8 || data_width == 16) {
		short_converter = ShortConverterPtr (new SampleFormatConverter<short> (channels));
		short_converter->init (max_frames, config.format->dither_type(), data_width);
		add_child (config);
		if (_analyse) { analyser->add_output (short_converter); }

	} else if (data_width == 24 || data_width == 32) {
		int_converter = IntConverterPtr (new SampleFormatConverter<int> (channels));
		int_converter->init (max_frames, config.format->dither_type(), data_width);
		add_child (config);
		if (_analyse) { analyser->add_output (int_converter); }
	} else {
		int actual_data_width = 8 * sizeof(Sample);
		float_converter = FloatConverterPtr (new SampleFormatConverter<Sample> (channels));
		float_converter->init (max_frames, config.format->dither_type(), actual_data_width);
		add_child (config);
		if (_analyse) { analyser->add_output (float_converter); }
	}
}

void
ExportGraphBuilder::SFC::set_peak (float gain)
{
	if (_analyse) {
		analyser->set_normalization_gain (gain);
	}
}

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SFC::sink ()
{
	if (_analyse) {
		return chunker;
	} else if (data_width == 8 || data_width == 16) {
		return short_converter;
	} else if (data_width == 24 || data_width == 32) {
		return int_converter;
	} else {
		return float_converter;
	}
}

void
ExportGraphBuilder::SFC::add_child (FileSpec const & new_config)
{
	for (boost::ptr_list<Encoder>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}

	children.push_back (new Encoder());
	Encoder & encoder = children.back();

	if (data_width == 8 || data_width == 16) {
		short_converter->add_output (encoder.init<short> (new_config));
	} else if (data_width == 24 || data_width == 32) {
		int_converter->add_output (encoder.init<int> (new_config));
	} else {
		float_converter->add_output (encoder.init<Sample> (new_config));
	}
}

void
ExportGraphBuilder::SFC::remove_children (bool remove_out_files)
{
	boost::ptr_list<Encoder>::iterator iter = children.begin ();

	while (iter != children.end() ) {

		if (remove_out_files) {
			iter->destroy_writer(remove_out_files);
		}
		iter = children.erase (iter);
	}
}

bool
ExportGraphBuilder::SFC::operator== (FileSpec const & other_config) const
{
	return config.format->sample_format() == other_config.format->sample_format();
}

/* Intermediate (Normalizer, TmpFile) */

ExportGraphBuilder::Intermediate::Intermediate (ExportGraphBuilder & parent, FileSpec const & new_config, framecnt_t max_frames)
	: parent (parent)
	, use_loudness (false)
	, use_peak (false)
{
	std::string tmpfile_path = parent.session.session_directory().export_path();
	tmpfile_path = Glib::build_filename(tmpfile_path, "XXXXXX");
	std::vector<char> tmpfile_path_buf(tmpfile_path.size() + 1);
	std::copy(tmpfile_path.begin(), tmpfile_path.end(), tmpfile_path_buf.begin());
	tmpfile_path_buf[tmpfile_path.size()] = '\0';

	config = new_config;
	uint32_t const channels = config.channel_config->get_n_chans();
	max_frames_out = 4086 - (4086 % channels); // TODO good chunk size
	use_loudness = config.format->normalize_loudness ();
	use_peak = config.format->normalize ();

	buffer.reset (new AllocatingProcessContext<Sample> (max_frames_out, channels));

	if (use_peak) {
		peak_reader.reset (new PeakReader ());
	}
	if (use_loudness) {
		loudness_reader.reset (new LoudnessReader (config.format->sample_rate(), channels, max_frames));
	}

	normalizer.reset (new AudioGrapher::Normalizer (use_loudness ? 0.0 : config.format->normalize_dbfs()));
	threader.reset (new Threader<Sample> (parent.thread_pool));
	normalizer->alloc_buffer (max_frames_out);
	normalizer->add_output (threader);

	int format = ExportFormatBase::F_RAW | ExportFormatBase::SF_Float;

	if (parent._realtime) {
		tmp_file.reset (new TmpFileRt<float> (&tmpfile_path_buf[0], format, channels, config.format->sample_rate()));
	} else {
		tmp_file.reset (new TmpFileSync<float> (&tmpfile_path_buf[0], format, channels, config.format->sample_rate()));
	}

	tmp_file->FileWritten.connect_same_thread (post_processing_connection,
	                                           boost::bind (&Intermediate::prepare_post_processing, this));
	tmp_file->FileFlushed.connect_same_thread (post_processing_connection,
	                                           boost::bind (&Intermediate::start_post_processing, this));

	add_child (new_config);

	if (use_loudness) {
		loudness_reader->add_output (tmp_file);
	} else if (use_peak) {
		peak_reader->add_output (tmp_file);
	}
}

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::Intermediate::sink ()
{
	if (use_loudness) {
		return loudness_reader;
	} else if (use_peak) {
		return peak_reader;
	}
	return tmp_file;
}

void
ExportGraphBuilder::Intermediate::add_child (FileSpec const & new_config)
{
	for (boost::ptr_list<SFC>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}

	children.push_back (new SFC (parent, new_config, max_frames_out));
	threader->add_output (children.back().sink());
}

void
ExportGraphBuilder::Intermediate::remove_children (bool remove_out_files)
{
	boost::ptr_list<SFC>::iterator iter = children.begin ();

	while (iter != children.end() ) {
		iter->remove_children (remove_out_files);
		iter = children.erase (iter);
	}
}

bool
ExportGraphBuilder::Intermediate::operator== (FileSpec const & other_config) const
{
	return config.format->normalize() == other_config.format->normalize() &&
		config.format->normalize_loudness () == other_config.format->normalize_loudness() &&
		(
		 (!config.format->normalize_loudness () && config.format->normalize_dbfs() == other_config.format->normalize_dbfs())
		 ||
		 // FIXME: allow simultaneous export of two formats with different loundness normalization settings
		 (config.format->normalize_loudness () /* lufs/dbtp is a result option, not an instantaion option */)
		);
}

unsigned
ExportGraphBuilder::Intermediate::get_postprocessing_cycle_count() const
{
	return static_cast<unsigned>(std::ceil(static_cast<float>(tmp_file->get_frames_written()) /
	                                       max_frames_out));
}

bool
ExportGraphBuilder::Intermediate::process()
{
	framecnt_t frames_read = tmp_file->read (*buffer);
	return frames_read != buffer->frames();
}

void
ExportGraphBuilder::Intermediate::prepare_post_processing()
{
	// called in sync rt-context
	float gain;
	if (use_loudness) {
		gain = normalizer->set_peak (loudness_reader->get_peak (config.format->normalize_lufs (), config.format->normalize_dbtp ()));
	} else if (use_peak) {
		gain = normalizer->set_peak (peak_reader->get_peak());
	} else {
		gain = normalizer->set_peak (0.0);
	}
	if (use_loudness || use_peak) {
		// push info to analyzers
		for (boost::ptr_list<SFC>::iterator i = children.begin(); i != children.end(); ++i) {
			(*i).set_peak (gain);
		}
	}
	tmp_file->add_output (normalizer);
	parent.intermediates.push_back (this);
}

void
ExportGraphBuilder::Intermediate::start_post_processing()
{
	// called in disk-thread (when exporting in realtime)
	tmp_file->seek (0, SEEK_SET);
	if (!AudioEngine::instance()->freewheeling ()) {
		AudioEngine::instance()->freewheel (true);
	}
}

/* SRC */

ExportGraphBuilder::SRC::SRC (ExportGraphBuilder & parent, FileSpec const & new_config, framecnt_t max_frames)
	: parent (parent)
{
	config = new_config;
	converter.reset (new SampleRateConverter (new_config.channel_config->get_n_chans()));
	ExportFormatSpecification & format = *new_config.format;
	converter->init (parent.session.nominal_frame_rate(), format.sample_rate(), format.src_quality());
	max_frames_out = converter->allocate_buffers (max_frames);

	add_child (new_config);
}

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SRC::sink ()
{
	return converter;
}

void
ExportGraphBuilder::SRC::add_child (FileSpec const & new_config)
{
	if (new_config.format->normalize() || parent._realtime) {
		add_child_to_list (new_config, intermediate_children);
	} else {
		add_child_to_list (new_config, children);
	}
}

void
ExportGraphBuilder::SRC::remove_children (bool remove_out_files)
{
	boost::ptr_list<SFC>::iterator sfc_iter = children.begin();

	while (sfc_iter != children.end() ) {
		converter->remove_output (sfc_iter->sink() );
		sfc_iter->remove_children (remove_out_files);
		sfc_iter = children.erase (sfc_iter);
	}

	boost::ptr_list<Intermediate>::iterator norm_iter = intermediate_children.begin();

	while (norm_iter != intermediate_children.end() ) {
		converter->remove_output (norm_iter->sink() );
		norm_iter->remove_children (remove_out_files);
		norm_iter = intermediate_children.erase (norm_iter);
	}

}

template<typename T>
void
ExportGraphBuilder::SRC::add_child_to_list (FileSpec const & new_config, boost::ptr_list<T> & list)
{
	for (typename boost::ptr_list<T>::iterator it = list.begin(); it != list.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}

	list.push_back (new T (parent, new_config, max_frames_out));
	converter->add_output (list.back().sink ());
}

bool
ExportGraphBuilder::SRC::operator== (FileSpec const & other_config) const
{
	return config.format->sample_rate() == other_config.format->sample_rate();
}

/* SilenceHandler */
ExportGraphBuilder::SilenceHandler::SilenceHandler (ExportGraphBuilder & parent, FileSpec const & new_config, framecnt_t max_frames)
	: parent (parent)
{
	config = new_config;
	max_frames_in = max_frames;
	framecnt_t sample_rate = parent.session.nominal_frame_rate();

#ifdef MIXBUS
	silence_trimmer.reset (new SilenceTrimmer<Sample>(max_frames_in, -90));
#else
	// TODO silence-threshold should be per export-preset, with Config->get_silence_threshold being the default
	silence_trimmer.reset (new SilenceTrimmer<Sample>(max_frames_in, Config->get_export_silence_threshold ()));
#endif
	silence_trimmer->set_trim_beginning (config.format->trim_beginning());
	silence_trimmer->set_trim_end (config.format->trim_end());

	framecnt_t sb = config.format->silence_beginning_at (parent.timespan->get_start(), sample_rate);
	framecnt_t se = config.format->silence_end_at (parent.timespan->get_end(), sample_rate);

	silence_trimmer->add_silence_to_beginning (sb);
	silence_trimmer->add_silence_to_end (se);

	add_child (new_config);
}

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SilenceHandler::sink ()
{
	return silence_trimmer;
}

void
ExportGraphBuilder::SilenceHandler::add_child (FileSpec const & new_config)
{
	for (boost::ptr_list<SRC>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}

	children.push_back (new SRC (parent, new_config, max_frames_in));
	silence_trimmer->add_output (children.back().sink());
}

void
ExportGraphBuilder::SilenceHandler::remove_children (bool remove_out_files)
{
	boost::ptr_list<SRC>::iterator iter = children.begin();

	while (iter != children.end() ) {
		silence_trimmer->remove_output (iter->sink() );
		iter->remove_children (remove_out_files);
		iter = children.erase (iter);
	}
}

bool
ExportGraphBuilder::SilenceHandler::operator== (FileSpec const & other_config) const
{
	ExportFormatSpecification & format = *config.format;
	ExportFormatSpecification & other_format = *other_config.format;
	return (format.trim_beginning() == other_format.trim_beginning()) &&
		(format.trim_end() == other_format.trim_end()) &&
		(format.silence_beginning_time() == other_format.silence_beginning_time()) &&
		(format.silence_end_time() == other_format.silence_end_time());
}

/* ChannelConfig */

ExportGraphBuilder::ChannelConfig::ChannelConfig (ExportGraphBuilder & parent, FileSpec const & new_config, ChannelMap & channel_map)
	: parent (parent)
{
	typedef ExportChannelConfiguration::ChannelList ChannelList;

	config = new_config;

	framecnt_t max_frames = parent.session.engine().samples_per_cycle();
	interleaver.reset (new Interleaver<Sample> ());
	interleaver->init (new_config.channel_config->get_n_chans(), max_frames);

	// Make the chunk size divisible by the channel count
	int chan_count = new_config.channel_config->get_n_chans();
	max_frames_out = 8192;
	if (chan_count > 0) {
		max_frames_out -= max_frames_out % chan_count;
	}
	chunker.reset (new Chunker<Sample> (max_frames_out));
	interleaver->add_output(chunker);

	ChannelList const & channel_list = config.channel_config->get_channels();
	unsigned chan = 0;
	for (ChannelList::const_iterator it = channel_list.begin(); it != channel_list.end(); ++it, ++chan) {
		ChannelMap::iterator map_it = channel_map.find (*it);
		if (map_it == channel_map.end()) {
			std::pair<ChannelMap::iterator, bool> result_pair =
				channel_map.insert (std::make_pair (*it, IdentityVertexPtr (new IdentityVertex<Sample> ())));
			assert (result_pair.second);
			map_it = result_pair.first;
		}
		map_it->second->add_output (interleaver->input (chan));
	}

	add_child (new_config);
}

void
ExportGraphBuilder::ChannelConfig::add_child (FileSpec const & new_config)
{
	assert (*this == new_config);

	for (boost::ptr_list<SilenceHandler>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}

	children.push_back (new SilenceHandler (parent, new_config, max_frames_out));
	chunker->add_output (children.back().sink ());
}

void
ExportGraphBuilder::ChannelConfig::remove_children (bool remove_out_files)
{
	boost::ptr_list<SilenceHandler>::iterator iter = children.begin();

	while(iter != children.end() ) {

		chunker->remove_output (iter->sink ());
		iter->remove_children (remove_out_files);
		iter = children.erase(iter);
	}
}

bool
ExportGraphBuilder::ChannelConfig::operator== (FileSpec const & other_config) const
{
	return config.channel_config == other_config.channel_config;
}

} // namespace ARDOUR
