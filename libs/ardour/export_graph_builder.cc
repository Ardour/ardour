/*
 * Copyright (C) 2009-2013 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <vector>

#include <glibmm/miscutils.h>
#include <glibmm/timer.h>

#include "pbd/uuid.h"
#include "pbd/file_utils.h"
#include "pbd/cpus.h"

#include "audiographer/process_context.h"
#include "audiographer/general/chunker.h"
#include "audiographer/general/cmdpipe_writer.h"
#include "audiographer/general/demo_noise.h"
#include "audiographer/general/interleaver.h"
#include "audiographer/general/limiter.h"
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
#include "ardour/export_failed.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_specification.h"
#include "ardour/export_graph_builder.h"
#include "ardour/export_timespan.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session_directory.h"
#include "ardour/session_metadata.h"
#include "ardour/sndfile_helpers.h"
#include "ardour/system_exec.h"

using namespace AudioGrapher;
using std::string;

namespace ARDOUR {

ExportGraphBuilder::ExportGraphBuilder (Session const & session)
	: session (session)
	, thread_pool (hardware_concurrency())
{
	process_buffer_samples = session.engine().samples_per_cycle();
}

ExportGraphBuilder::~ExportGraphBuilder ()
{
}

samplecnt_t
ExportGraphBuilder::process (samplecnt_t samples, bool last_cycle)
{
	assert(samples <= process_buffer_samples);

	sampleoffset_t off = 0;
	for (ChannelMap::iterator it = channels.begin(); it != channels.end(); ++it) {
		Sample const * process_buffer = 0;
		it->first->read (process_buffer, samples);

		if (session.remaining_latency_preroll () >= _master_align + samples) {
			/* Skip processing during pre-roll, only read/write export ringbuffers */
			return 0;
		}

		off = 0;
		if (session.remaining_latency_preroll () > _master_align) {
			off = session.remaining_latency_preroll () - _master_align;
			assert (off < samples);
		}

		ConstProcessContext<Sample> context(&process_buffer[off], samples - off, 1);
		if (last_cycle) { context().set_flag (ProcessContext<Sample>::EndOfInput); }
		it->second->process (context);
	}

	return samples - off;
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
	_master_align = 0;
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
	/* calculate common latency, shave off master-bus hardware playback latency (if any) */
	_master_align = session.master_out() ? session.master_out()->output()->connected_latency (true) : 0;

	ExportChannelConfiguration::ChannelList const & channels = config.channel_config->get_channels();

	for(ExportChannelConfiguration::ChannelList::const_iterator it = channels.begin(); it != channels.end(); ++it) {
		_master_align = std::min (_master_align, (*it)->common_port_playback_latency ());
	}

	/* now set-up port-data sniffing and delay-ringbuffers */
	for(ExportChannelConfiguration::ChannelList::const_iterator it = channels.begin(); it != channels.end(); ++it) {
		(*it)->prepare_export (process_buffer_samples, _master_align);
	}

	_realtime = rt;

	/* If the sample rate is "session rate", change it to the real value.
	 * However, we need to copy it to not change the config which is saved...
	 */
	FileSpec new_config (config);
	new_config.format.reset(new ExportFormatSpecification(*new_config.format, false));
	if(new_config.format->sample_rate() == ExportFormatBase::SR_Session) {
		samplecnt_t session_rate = session.nominal_sample_rate();
		new_config.format->set_sample_rate(ExportFormatBase::nearest_sample_rate(session_rate));
	}

	if (!new_config.channel_config->get_split ()) {
		add_split_config (new_config);
		return;
	}

	/* Split channel configurations are split into several channel configurations,
	 * each corresponding to a file, at this stage
	 */
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
	if (config.format->format_id() == ExportFormatBase::F_FFMPEG) {
		init_writer (pipe_writer);
		return pipe_writer;
	} else {
		init_writer (float_writer);
		return float_writer;
	}
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

		if (pipe_writer) {
			pipe_writer->close ();
		}

		if (std::remove(writer_filename.c_str() ) != 0) {
			std::cout << "Encoder::destroy_writer () : Error removing file: " << strerror(errno) << std::endl;
		}
	}

	float_writer.reset ();
	int_writer.reset ();
	short_writer.reset ();
	pipe_writer.reset ();
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
	if (format & ExportFormatBase::SF_Vorbis) {
		/* libsndfile uses range 0..1 (worst.. best) for
		 * SFC_SET_VBR_ENCODING_QUALITY and maps
		 * SFC_SET_COMPRESSION_LEVEL = 1.0 - VBR_ENCODING_QUALITY
		 */
		double vorbis_quality = config.format->codec_quality () / 100.f;
		if (vorbis_quality >= 0 && vorbis_quality <= 1.0) {
			writer->command (SFC_SET_VBR_ENCODING_QUALITY, &vorbis_quality, sizeof (double));
		}
	}
}

template<typename T>
void
ExportGraphBuilder::Encoder::init_writer (boost::shared_ptr<AudioGrapher::CmdPipeWriter<T> > & writer)
{
	unsigned channels = config.channel_config->get_n_chans();
	config.filename->set_channel_config(config.channel_config);
	writer_filename = config.filename->get_path (config.format);

	std::string ffmpeg_exe;
	std::string unused;

	if (!ArdourVideoToolPaths::transcoder_exe (ffmpeg_exe, unused)) {
		throw ExportFailed ("External encoder (ffmpeg) is not available.");
	}

	int quality = config.format->codec_quality ();

	int a=0;
	char **argp = (char**) calloc (100, sizeof(char*));
	char tmp[64];
	argp[a++] = strdup(ffmpeg_exe.c_str());
	argp[a++] = strdup ("-f");
	argp[a++] = strdup ("f32le");
	argp[a++] = strdup ("-acodec");
	argp[a++] = strdup ("pcm_f32le");
	argp[a++] = strdup ("-ac");
	snprintf (tmp, sizeof(tmp), "%d", channels);
	argp[a++] = strdup (tmp);
	argp[a++] = strdup ("-ar");
	snprintf (tmp, sizeof(tmp), "%d", config.format->sample_rate());
	argp[a++] = strdup (tmp);
	argp[a++] = strdup ("-i");
	argp[a++] = strdup ("pipe:0");

	argp[a++] = strdup ("-y");
	if (quality <= 0) {
		/* variable rate, lower is better */
		snprintf (tmp, sizeof(tmp), "%d", -quality);
		argp[a++] = strdup ("-q:a"); argp[a++] = strdup (tmp);
	} else {
		/* fixed bitrate, higher is better */
		snprintf (tmp, sizeof(tmp), "%dk", quality); // eg. "192k"
		argp[a++] = strdup ("-b:a"); argp[a++] = strdup (tmp);
	}

	SessionMetadata::MetaDataMap meta;
	meta["comment"] = "Created with " PROGRAM_NAME;

	if (config.format->tag()) {
		ARDOUR::SessionMetadata* session_data = ARDOUR::SessionMetadata::Metadata();
		session_data->av_export_tag (meta);
	}

	for(SessionMetadata::MetaDataMap::const_iterator it = meta.begin(); it != meta.end(); ++it) {
		argp[a++] = strdup("-metadata");
		argp[a++] = SystemExec::format_key_value_parameter (it->first.c_str(), it->second.c_str());
	}

	argp[a++] = strdup (writer_filename.c_str());
	argp[a] = (char *)0;

	/* argp is free()d in ~SystemExec,
	 * SystemExec is deleted when writer is destroyed */
	ARDOUR::SystemExec* exec = new ARDOUR::SystemExec (ffmpeg_exe, argp);
	PBD::info << "Encode command: { " << exec->to_s () << "}" << endmsg;
	if (exec->start (SystemExec::MergeWithStdin)) {
		throw ExportFailed ("External encoder (ffmpeg) cannot be started.");
	}
	writer.reset (new AudioGrapher::CmdPipeWriter<T> (exec, writer_filename));
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

ExportGraphBuilder::SFC::SFC (ExportGraphBuilder &parent, FileSpec const & new_config, samplecnt_t max_samples)
	: data_width(0)
{
	config = new_config;
	data_width = sndfile_data_width (Encoder::get_real_format (config));
	unsigned channels = new_config.channel_config->get_n_chans();
	_analyse = config.format->analyse();

	float ntarget = (config.format->normalize_loudness () || !config.format->normalize()) ? 0.0 : config.format->normalize_dbfs();
	normalizer.reset (new AudioGrapher::Normalizer (ntarget, max_samples));
	limiter.reset (new AudioGrapher::Limiter (config.format->sample_rate(), channels, max_samples));

	normalizer->add_output (limiter);

	boost::shared_ptr<AudioGrapher::ListedSource<float> > intermediate = limiter;

	if (_analyse) {
		samplecnt_t sample_rate = parent.session.nominal_sample_rate();
		samplecnt_t sb = config.format->silence_beginning_at (parent.timespan->get_start(), sample_rate);
		samplecnt_t se = config.format->silence_end_at (parent.timespan->get_end(), sample_rate);
		samplecnt_t duration = parent.timespan->get_length () + sb + se;

		max_samples = std::min ((samplecnt_t) 8192 * channels, std::max ((samplecnt_t) 4096 * channels, max_samples));
		chunker.reset (new Chunker<Sample> (max_samples));
		analyser.reset (new Analyser (config.format->sample_rate(), channels, max_samples,
		                              (samplecnt_t) ceil (duration * config.format->sample_rate () / (double) sample_rate),
		                              800 * ui_scale_factor, 200 * ui_scale_factor
		                             ));

		config.filename->set_channel_config (config.channel_config);
		parent.add_analyser (config.filename->get_path (config.format), analyser);
		limiter->set_result (analyser->result (true));

		chunker->add_output (analyser);
		intermediate->add_output (chunker);
		intermediate = analyser;
	}

	if (config.format->format_id() == ExportFormatBase::F_None) {
		/* do not encode result, stop after chunker/analyzer */
		assert (_analyse);
		return;
	}

	if (config.format->demo_noise_duration () > 0 && config.format->demo_noise_interval () > 0) {
		samplecnt_t sample_rate = parent.session.nominal_sample_rate();
		demo_noise_adder.reset (new DemoNoiseAdder (channels));
		demo_noise_adder->init (max_samples,
				sample_rate * config.format->demo_noise_interval () / 1000,
				sample_rate * config.format->demo_noise_duration () / 1000,
				config.format->demo_noise_level ());

		intermediate->add_output (demo_noise_adder);
		intermediate = demo_noise_adder;
	}

	if (data_width == 8 || data_width == 16) {
		short_converter = ShortConverterPtr (new SampleFormatConverter<short> (channels));
		short_converter->init (max_samples, config.format->dither_type(), data_width);
		add_child (config);
		intermediate->add_output (short_converter);
	} else if (data_width == 24 || data_width == 32) {
		int_converter = IntConverterPtr (new SampleFormatConverter<int> (channels));
		int_converter->init (max_samples, config.format->dither_type(), data_width);
		add_child (config);
		intermediate->add_output (int_converter);
	} else {
		int actual_data_width = 8 * sizeof(Sample);
		float_converter = FloatConverterPtr (new SampleFormatConverter<Sample> (channels));
		float_converter->init (max_samples, config.format->dither_type(), actual_data_width);
		add_child (config);
		intermediate->add_output (float_converter);
	}
}

void
ExportGraphBuilder::SFC::set_duration (samplecnt_t n_samples)
{
	/* update after silence trim */
	if (analyser) {
		analyser->set_duration (n_samples);
	}
	if (limiter) {
		limiter->set_duration (n_samples);
	}
}

void
ExportGraphBuilder::SFC::set_peak_dbfs (float peak, bool force)
{
	if (!config.format->normalize () && !force) {
		return;
	}
	float gain = normalizer->set_peak (peak);
	if (_analyse) {
		analyser->set_normalization_gain (gain);
	}
}

void
ExportGraphBuilder::SFC::set_peak_lufs (AudioGrapher::LoudnessReader const& lr)
{
	if (!config.format->normalize_loudness ()) {
		return;
	}
	float LUFSi, LUFSs;
	if (!config.format->use_tp_limiter ()) {
		float peak = lr.calc_peak (config.format->normalize_lufs (), config.format->normalize_dbtp ());
		set_peak_dbfs (peak, true);
	} else if (lr.get_loudness (&LUFSi, &LUFSs) && (LUFSi > -180 || LUFSs > -180)) {
		float lufs = LUFSi > -180 ? LUFSi : LUFSs;
		float peak = powf (10.f, .05 * (lufs - config.format->normalize_lufs () - 0.05));
		limiter->set_threshold (config.format->normalize_dbtp ());
		set_peak_dbfs (peak, true);
	}
}

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SFC::sink ()
{
	return normalizer;
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
ExportGraphBuilder::SFC::operator== (FileSpec const& other_config) const
{
	ExportFormatSpecification const& a = *config.format;
	ExportFormatSpecification const& b = *other_config.format;

	bool id = a.sample_format() == b.sample_format();

	if (a.normalize_loudness () == b.normalize_loudness ()) {
		id &= a.normalize_lufs () == b.normalize_lufs ();
		id &= a.normalize_dbtp () == b.normalize_dbtp ();
	} else {
		return false;
	}
	if (a.normalize () == b.normalize ()) {
		id &= a.normalize_dbfs () == b.normalize_dbfs ();
	} else {
		return false;
	}

	id &= a.demo_noise_duration () == b.demo_noise_duration ();
	id &= a.demo_noise_interval () == b.demo_noise_interval ();

	return id;
}

/* Intermediate (Normalizer, TmpFile) */

ExportGraphBuilder::Intermediate::Intermediate (ExportGraphBuilder & parent, FileSpec const & new_config, samplecnt_t max_samples)
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
	max_samples_out = 4086 - (4086 % channels); // TODO good chunk size

	buffer.reset (new AllocatingProcessContext<Sample> (max_samples_out, channels));

	peak_reader.reset (new PeakReader ());
	loudness_reader.reset (new LoudnessReader (config.format->sample_rate(), channels, max_samples));
	threader.reset (new Threader<Sample> (parent.thread_pool));

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

	peak_reader->add_output (loudness_reader);
	loudness_reader->add_output (tmp_file);
}

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::Intermediate::sink ()
{
	if (use_peak) {
		return peak_reader;
	} else if (use_loudness) {
		return loudness_reader;
	} else {
		return tmp_file;
	}
}

void
ExportGraphBuilder::Intermediate::add_child (FileSpec const & new_config)
{
	use_peak     |= new_config.format->normalize ();
	use_loudness |= new_config.format->normalize_loudness ();

	for (boost::ptr_list<SFC>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}

	children.push_back (new SFC (parent, new_config, max_samples_out));
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
	return true;
}

unsigned
ExportGraphBuilder::Intermediate::get_postprocessing_cycle_count() const
{
	return static_cast<unsigned>(std::ceil(static_cast<float>(tmp_file->get_samples_written()) /
	                                       max_samples_out));
}

bool
ExportGraphBuilder::Intermediate::process()
{
	samplecnt_t samples_read = tmp_file->read (*buffer);
	return samples_read != buffer->samples();
}

void
ExportGraphBuilder::Intermediate::prepare_post_processing()
{
	for (boost::ptr_list<SFC>::iterator i = children.begin(); i != children.end(); ++i) {
		if (use_peak) {
			(*i).set_peak_dbfs (peak_reader->get_peak());
		}
		if (use_loudness) {
			(*i).set_peak_lufs (*loudness_reader);
		}
	}

	tmp_file->add_output (threader);
	parent.intermediates.push_back (this);
}

void
ExportGraphBuilder::Intermediate::start_post_processing()
{
	for (boost::ptr_list<SFC>::iterator i = children.begin(); i != children.end(); ++i) {
		(*i).set_duration (tmp_file->get_samples_written() / config.channel_config->get_n_chans());
	}

	tmp_file->seek (0, SEEK_SET);

	/* called in disk-thread when exporting in realtime,
	 * to enable freewheeling for post-proc.
	 *
	 * It may also be called to normalize from the
	 * freewheeling rt-callback, in which case this
	 * will be a no-op.
	 *
	 * RT Stem export has multiple TmpFileRt threads,
	 * prevent concurrent calls to enable freewheel ()
	 */
	Glib::Threads::Mutex::Lock lm (parent.engine_request_lock);
	if (!AudioEngine::instance()->freewheeling ()) {
		AudioEngine::instance()->freewheel (true);
		while (!AudioEngine::instance()->freewheeling ()) {
			Glib::usleep (AudioEngine::instance()->usecs_per_cycle ());
		}
	}
}

/* SRC */

ExportGraphBuilder::SRC::SRC (ExportGraphBuilder & parent, FileSpec const & new_config, samplecnt_t max_samples)
	: parent (parent)
{
	config = new_config;
	converter.reset (new SampleRateConverter (new_config.channel_config->get_n_chans()));
	ExportFormatSpecification & format = *new_config.format;
	converter->init (parent.session.nominal_sample_rate(), format.sample_rate(), format.src_quality());
	max_samples_out = converter->allocate_buffers (max_samples);

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

	list.push_back (new T (parent, new_config, max_samples_out));
	converter->add_output (list.back().sink ());
}

bool
ExportGraphBuilder::SRC::operator== (FileSpec const & other_config) const
{
	return config.format->sample_rate() == other_config.format->sample_rate();
}

/* SilenceHandler */
ExportGraphBuilder::SilenceHandler::SilenceHandler (ExportGraphBuilder & parent, FileSpec const & new_config, samplecnt_t max_samples)
	: parent (parent)
{
	config = new_config;
	max_samples_in = max_samples;
	samplecnt_t sample_rate = parent.session.nominal_sample_rate();

	/* work around partsing "-inf" config to "0" -- 7b1f97b
	 * silence trim 0dBFS makes no sense, anyway.
	 */
	float est = Config->get_export_silence_threshold ();
	if (est >= 0.f) est = -INFINITY;
#ifdef MIXBUS
	// Mixbus channelstrip always dithers the signal, cut above dither level
	silence_trimmer.reset (new SilenceTrimmer<Sample>(max_samples_in, std::max (-90.f, est)));
#else
	// TODO silence-threshold should be per export-preset, with Config->get_silence_threshold being the default
	silence_trimmer.reset (new SilenceTrimmer<Sample>(max_samples_in, est));
#endif
	silence_trimmer->set_trim_beginning (config.format->trim_beginning());
	silence_trimmer->set_trim_end (config.format->trim_end());

	samplecnt_t sb = config.format->silence_beginning_at (parent.timespan->get_start(), sample_rate);
	samplecnt_t se = config.format->silence_end_at (parent.timespan->get_end(), sample_rate);

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

	children.push_back (new SRC (parent, new_config, max_samples_in));
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

	samplecnt_t max_samples = parent.session.engine().samples_per_cycle();
	interleaver.reset (new Interleaver<Sample> ());
	interleaver->init (new_config.channel_config->get_n_chans(), max_samples);

	// Make the chunk size divisible by the channel count
	int chan_count = new_config.channel_config->get_n_chans();
	max_samples_out = 8192;
	if (chan_count > 0) {
		max_samples_out -= max_samples_out % chan_count;
	}
	chunker.reset (new Chunker<Sample> (max_samples_out));
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

	children.push_back (new SilenceHandler (parent, new_config, max_samples_out));
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
