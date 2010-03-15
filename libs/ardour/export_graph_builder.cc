#include "ardour/export_graph_builder.h"

#include "audiographer/process_context.h"
#include "audiographer/general/interleaver.h"
#include "audiographer/general/normalizer.h"
#include "audiographer/general/peak_reader.h"
#include "audiographer/general/sample_format_converter.h"
#include "audiographer/general/sr_converter.h"
#include "audiographer/general/silence_trimmer.h"
#include "audiographer/general/threader.h"
#include "audiographer/sndfile/tmp_file.h"
#include "audiographer/sndfile/sndfile_writer.h"

#include "ardour/audioengine.h"
#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_format_specification.h"
#include "ardour/sndfile_helpers.h"

#include "pbd/filesystem.h"

using namespace AudioGrapher;

namespace ARDOUR {

ExportGraphBuilder::ExportGraphBuilder (Session const & session)
  : session (session)
  , thread_pool (4) // FIXME thread amount to cores amount
{
	process_buffer_frames = session.engine().frames_per_cycle();
	process_buffer = new Sample[process_buffer_frames];
}

ExportGraphBuilder::~ExportGraphBuilder ()
{
	delete [] process_buffer;
}

int
ExportGraphBuilder::process (nframes_t /* frames */, bool last_cycle)
{
	for (ChannelMap::iterator it = channels.begin(); it != channels.end(); ++it) {
		it->first->read (process_buffer, process_buffer_frames);
		ProcessContext<Sample> context(process_buffer, process_buffer_frames, 1);
		if (last_cycle) { context.set_flag (ProcessContext<Sample>::EndOfInput); }
		it->second->process (context);
	}
	
	return 0;
}

bool
ExportGraphBuilder::process_normalize ()
{
	for (std::list<Normalizer *>::iterator it = normalizers.begin(); it != normalizers.end(); /* ++ in loop */) {
		if ((*it)->process()) {
			it = normalizers.erase (it);
		} else {
			++it;
		}
	}
	
	return normalizers.empty();
}

void
ExportGraphBuilder::reset ()
{
	channel_configs.clear ();
	channels.clear ();
	normalizers.clear ();
}

void
ExportGraphBuilder::add_config (FileSpec const & config)
{
	for (ChannelConfigList::iterator it = channel_configs.begin(); it != channel_configs.end(); ++it) {
		if (*it == config) {
			it->add_child (config);
			return;
		}
	}
	
	// No duplicate channel config found, create new one
	channel_configs.push_back (ChannelConfig (*this));
	ChannelConfig & c_config (channel_configs.back());
	c_config.init (config, channels);
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
	Glib::ustring filename = config.filename->get_path (config.format);
	
	writer.reset (new AudioGrapher::SndfileWriter<T> (filename, format, channels, config.format->sample_rate()));
	writer->FileWritten.connect (sigc::mem_fun (*this, &ExportGraphBuilder::Encoder::copy_files));
}

void
ExportGraphBuilder::Encoder::copy_files (std::string orig_path)
{
	while (filenames.size()) {
		FilenamePtr & filename = filenames.front();
		PBD::sys::copy_file (orig_path, filename->get_path (config.format).c_str());
		filenames.pop_front();
	}
}

/* SFC */

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SFC::init (FileSpec const & new_config, nframes_t max_frames)
{
	config = new_config;
	data_width = sndfile_data_width (Encoder::get_real_format (config));
	unsigned channels = new_config.channel_config->get_n_chans();
	
	if (data_width == 8 || data_width == 16) {
		short_converter = ShortConverterPtr (new SampleFormatConverter<short> (channels));
		short_converter->init (max_frames, config.format->dither_type(), data_width);
		add_child (config);
		return short_converter;
	} else if (data_width == 24 || data_width == 32) {
		int_converter = IntConverterPtr (new SampleFormatConverter<int> (channels));
		int_converter->init (max_frames, config.format->dither_type(), data_width);
		add_child (config);
		return int_converter;
	} else {
		float_converter = FloatConverterPtr (new SampleFormatConverter<Sample> (channels));
		float_converter->init (max_frames, config.format->dither_type(), data_width);
		add_child (config);
		return float_converter;
	}
}

void
ExportGraphBuilder::SFC::add_child (FileSpec const & new_config)
{
	for (std::list<Encoder>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}
	
	children.push_back (Encoder());
	Encoder & encoder = children.back();
	
	if (data_width == 8 || data_width == 16) {
		short_converter->add_output (encoder.init<short> (new_config));
	} else if (data_width == 24 || data_width == 32) {
		int_converter->add_output (encoder.init<int> (new_config));
	} else {
		float_converter->add_output (encoder.init<Sample> (new_config));
	}
}

bool
ExportGraphBuilder::SFC::operator== (FileSpec const & other_config) const
{
	return config.format->sample_format() == other_config.format->sample_format();
}

/* Normalizer */

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::Normalizer::init (FileSpec const & new_config, nframes_t /*max_frames*/)
{
	config = new_config;
	max_frames_out = 4086; // TODO good chunk size
	
	buffer.reset (new AllocatingProcessContext<Sample> (max_frames_out, config.channel_config->get_n_chans()));
	peak_reader.reset (new PeakReader ());
	normalizer.reset (new AudioGrapher::Normalizer (config.format->normalize_target()));
	threader.reset (new Threader<Sample> (parent.thread_pool));
	
	normalizer->alloc_buffer (max_frames_out);
	normalizer->add_output (threader);
	
	int format = ExportFormatBase::F_RAW | ExportFormatBase::SF_Float;
	tmp_file.reset (new TmpFile<float> (format, config.channel_config->get_n_chans(), 
	                                    config.format->sample_rate()));
	tmp_file->FileWritten.connect (sigc::hide (sigc::mem_fun (*this, &Normalizer::start_post_processing)));
	
	add_child (new_config);
	
	peak_reader->add_output (tmp_file);
	return peak_reader;
}

void
ExportGraphBuilder::Normalizer::add_child (FileSpec const & new_config)
{
	for (std::list<SFC>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}
	
	children.push_back (SFC (parent));
	threader->add_output (children.back().init (new_config, max_frames_out));
}

bool
ExportGraphBuilder::Normalizer::operator== (FileSpec const & other_config) const
{
	return config.format->normalize() == other_config.format->normalize() &&
	       config.format->normalize_target() == other_config.format->normalize_target();
}

bool
ExportGraphBuilder::Normalizer::process()
{
	nframes_t frames_read = tmp_file->read (*buffer);
	return frames_read != buffer->frames();
}

void
ExportGraphBuilder::Normalizer::start_post_processing()
{
	normalizer->set_peak (peak_reader->get_peak());
	tmp_file->seek (0, SEEK_SET);
	tmp_file->add_output (normalizer);
	parent.normalizers.push_back (this);
}

/* SRC */

ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SRC::init (FileSpec const & new_config, nframes_t max_frames)
{
	config = new_config;
	converter.reset (new SampleRateConverter (new_config.channel_config->get_n_chans()));
	ExportFormatSpecification & format = *new_config.format;
	converter->init (parent.session.nominal_frame_rate(), format.sample_rate(), format.src_quality());
	max_frames_out = converter->allocate_buffers (max_frames);
	
	add_child (new_config);
	
	return converter;
}

void
ExportGraphBuilder::SRC::add_child (FileSpec const & new_config)
{
	if (new_config.format->normalize()) {
		add_child_to_list (new_config, normalized_children);
	} else {
		add_child_to_list (new_config, children);
	}
}

template<typename T>
void
ExportGraphBuilder::SRC::add_child_to_list (FileSpec const & new_config, std::list<T> & list)
{
	for (typename std::list<T>::iterator it = list.begin(); it != list.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}
	
	list.push_back (T (parent));
	converter->add_output (list.back().init (new_config, max_frames_out));
}

bool
ExportGraphBuilder::SRC::operator== (FileSpec const & other_config) const
{
	return config.format->sample_rate() == other_config.format->sample_rate();
}

/* SilenceHandler */
ExportGraphBuilder::FloatSinkPtr
ExportGraphBuilder::SilenceHandler::init (FileSpec const & new_config, nframes_t max_frames)
{
	config = new_config;
	max_frames_in = max_frames;
	nframes_t sample_rate = parent.session.nominal_frame_rate();
	
	silence_trimmer.reset (new SilenceTrimmer<Sample>(max_frames_in));
	silence_trimmer->set_trim_beginning (config.format->trim_beginning());
	silence_trimmer->set_trim_end (config.format->trim_end());
	silence_trimmer->add_silence_to_beginning (config.format->silence_beginning(sample_rate));
	silence_trimmer->add_silence_to_end (config.format->silence_end(sample_rate));
	
	add_child (new_config);
	
	return silence_trimmer;
}

void
ExportGraphBuilder::SilenceHandler::add_child (FileSpec const & new_config)
{
	for (std::list<SRC>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}
	
	children.push_back (SRC (parent));
	silence_trimmer->add_output (children.back().init (new_config, max_frames_in));
}

bool
ExportGraphBuilder::SilenceHandler::operator== (FileSpec const & other_config) const
{
	ExportFormatSpecification & format = *config.format;
	ExportFormatSpecification & other_format = *other_config.format;
	return (format.trim_beginning() == other_format.trim_beginning()) &&
	       (format.trim_end() == other_format.trim_end()) &&
	       (format.silence_beginning() == other_format.silence_beginning()) &&
	       (format.silence_end() == other_format.silence_end());
}

/* ChannelConfig */

void
ExportGraphBuilder::ChannelConfig::init (FileSpec const & new_config, ChannelMap & channel_map)
{
	typedef ExportChannelConfiguration::ChannelList ChannelList;
	
	config = new_config;
	max_frames = parent.session.engine().frames_per_cycle();
	
	interleaver.reset (new Interleaver<Sample> ());
	interleaver->init (new_config.channel_config->get_n_chans(), max_frames);
	
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
	for (std::list<SilenceHandler>::iterator it = children.begin(); it != children.end(); ++it) {
		if (*it == new_config) {
			it->add_child (new_config);
			return;
		}
	}
	
	children.push_back (SilenceHandler (parent));
	nframes_t max_frames_out = new_config.channel_config->get_n_chans() * max_frames;
	interleaver->add_output (children.back().init (new_config, max_frames_out));
}

bool
ExportGraphBuilder::ChannelConfig::operator== (FileSpec const & other_config) const
{
	return config.channel_config == other_config.channel_config;
}

} // namespace ARDOUR
