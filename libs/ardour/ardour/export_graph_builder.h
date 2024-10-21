/*
 * Copyright (C) 2009-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2010-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#pragma once

#include "ardour/export_handler.h"
#include "ardour/export_analysis.h"
#include "ardour/export_smf_writer.h"

#include "audiographer/utils/identity_vertex.h"

#include <boost/ptr_container/ptr_list.hpp>
#include <glibmm/threadpool.h>

namespace AudioGrapher {
	class SampleRateConverter;
	class PeakReader;
	class LoudnessReader;
	class Normalizer;
	class Limiter;
	class Analyser;
	class DemoNoiseAdder;
	template <typename T> class Chunker;
	template <typename T> class SampleFormatConverter;
	template <typename T> class Interleaver;
	template <typename T> class SndfileWriter;
	template <typename T> class CmdPipeWriter;
	template <typename T> class SilenceTrimmer;
	template <typename T> class TmpFile;
	template <typename T> class Threader;
	template <typename T> class AllocatingProcessContext;
}

namespace ARDOUR
{

class ExportTimespan;
class MidiBuffer;
class Session;

class LIBARDOUR_API ExportGraphBuilder
{
  private:
	typedef ExportHandler::FileSpec FileSpec;

	typedef std::shared_ptr<AudioGrapher::Sink<Sample> > FloatSinkPtr;
	typedef std::shared_ptr<AudioGrapher::Analyser> AnalysisPtr;
	typedef std::map<std::string, AnalysisPtr> AnalysisMap;

	struct AnyExport {
		/* Audio export */
		AudioGrapher::IdentityVertex<Sample> audio;
		void add_output (AudioGrapher::Source<Sample>::SinkPtr output) {
			audio.add_output (output);
		}
		void process (AudioGrapher::ProcessContext<Sample> const& c) {
			audio.process (c);
		}
		/* MIDI Export */
		ExportSMFWriter midi;
		void process (MidiBuffer const& buf, sampleoffset_t off, samplecnt_t cnt, bool last_cycle) {
			midi.process (buf, off, cnt, last_cycle);
		}
	};

	typedef std::shared_ptr<AnyExport> AnyExportPtr;
	typedef std::map<ExportChannelPtr, AnyExportPtr> ChannelMap;

  public:

	ExportGraphBuilder (Session const & session);
	~ExportGraphBuilder ();

	samplecnt_t process (samplecnt_t samples, bool last_cycle);
	bool post_process (); // returns true when finished
	bool need_postprocessing () const { return !intermediates.empty(); }
	bool realtime() const { return _realtime; }
	unsigned get_postprocessing_cycle_count() const;

	void reset ();
	void cleanup (bool remove_out_files = false);
	void set_current_timespan (std::shared_ptr<ExportTimespan> span);
	void add_config (FileSpec const & config, bool rt);
	void get_analysis_results (AnalysisResults& results);

	std::vector<std::string> exported_files () const {
		return _exported_files;
	}

  private:

	void add_analyser (const std::string& fn, AnalysisPtr ap) {
		analysis_map.insert (std::make_pair (fn, ap));
	}

	void add_export_fn (std::string const& fn) {
		_exported_files.push_back (fn);
	}

	std::vector<std::string> _exported_files;

	void add_split_config (FileSpec const & config);

	class Encoder {
            public:
		template <typename T> std::shared_ptr<AudioGrapher::Sink<T> > init (FileSpec const & new_config);
		void add_child (FileSpec const & new_config);
		void remove_children ();
		void destroy_writer (bool delete_out_file);
		bool operator== (FileSpec const & other_config) const;

		static int get_real_format (FileSpec const & config);

	                                        private:
		typedef std::shared_ptr<AudioGrapher::SndfileWriter<Sample> > FloatWriterPtr;
		typedef std::shared_ptr<AudioGrapher::SndfileWriter<int> >    IntWriterPtr;
		typedef std::shared_ptr<AudioGrapher::SndfileWriter<short> >  ShortWriterPtr;

		typedef std::shared_ptr<AudioGrapher::CmdPipeWriter<Sample> > FloatPipePtr;

		template<typename T> void init_writer (std::shared_ptr<AudioGrapher::SndfileWriter<T> > & writer);
		template<typename T> void init_writer (std::shared_ptr<AudioGrapher::CmdPipeWriter<T> > & writer);

		void copy_files (std::string orig_path);

		FileSpec               config;
		std::list<ExportFilenamePtr> filenames;
		PBD::ScopedConnection  copy_files_connection;

		std::string writer_filename;

		// Only one of these should be available at a time
		FloatWriterPtr float_writer;
		IntWriterPtr   int_writer;
		ShortWriterPtr short_writer;
		FloatPipePtr   pipe_writer;
	};

	// sample format converter
	class SFC {
	public:
		// This constructor so that this can be constructed like a Normalizer
		SFC (ExportGraphBuilder &, FileSpec const & new_config, samplecnt_t max_samples);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		void remove_children (bool remove_out_files);
		bool operator== (FileSpec const & other_config) const;

		void set_duration (samplecnt_t);
		void set_peak_dbfs (float, bool force = false);
		void set_peak_lufs (AudioGrapher::LoudnessReader const&);

	private:
		typedef std::shared_ptr<AudioGrapher::Chunker<float> > ChunkerPtr;
		typedef std::shared_ptr<AudioGrapher::DemoNoiseAdder> DemoNoisePtr;
		typedef std::shared_ptr<AudioGrapher::Normalizer> NormalizerPtr;
		typedef std::shared_ptr<AudioGrapher::Limiter> LimiterPtr;
		typedef std::shared_ptr<AudioGrapher::SampleFormatConverter<Sample> > FloatConverterPtr;
		typedef std::shared_ptr<AudioGrapher::SampleFormatConverter<int> >   IntConverterPtr;
		typedef std::shared_ptr<AudioGrapher::SampleFormatConverter<short> > ShortConverterPtr;

		FileSpec           config;
		int                data_width;
		std::list<Encoder> children;

		NormalizerPtr   normalizer;
		LimiterPtr      limiter;
		DemoNoisePtr    demo_noise_adder;
		ChunkerPtr      chunker;
		AnalysisPtr     analyser;
		bool            _analyse;
		// Only one of these should be available at a time
		FloatConverterPtr float_converter;
		IntConverterPtr int_converter;
		ShortConverterPtr short_converter;
	};

	class Intermediate {
	public:
		Intermediate (ExportGraphBuilder & parent, FileSpec const & new_config, samplecnt_t max_samples);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		void remove_children (bool remove_out_files);
		bool operator== (FileSpec const & other_config) const;

		unsigned get_postprocessing_cycle_count() const;

		/// Returns true when finished
		bool process ();

	private:
		typedef std::shared_ptr<AudioGrapher::PeakReader> PeakReaderPtr;
		typedef std::shared_ptr<AudioGrapher::LoudnessReader> LoudnessReaderPtr;
		typedef std::shared_ptr<AudioGrapher::TmpFile<Sample> > TmpFilePtr;
		typedef std::shared_ptr<AudioGrapher::Threader<Sample> > ThreaderPtr;
		typedef std::shared_ptr<AudioGrapher::AllocatingProcessContext<Sample> > BufferPtr;

		void prepare_post_processing ();
		void start_post_processing ();

		ExportGraphBuilder & parent;

		FileSpec        config;
		samplecnt_t     max_samples_out;
		bool            use_loudness;
		bool            use_peak;
		BufferPtr       buffer;
		PeakReaderPtr   peak_reader;
		TmpFilePtr      tmp_file;
		ThreaderPtr     threader;

		LoudnessReaderPtr    loudness_reader;
		std::list<SFC> children;

		PBD::ScopedConnectionList post_processing_connection;
	};

	// sample rate converter
	class SRC {
            public:
		SRC (ExportGraphBuilder & parent, FileSpec const & new_config, samplecnt_t max_samples);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		void remove_children (bool remove_out_files);

		bool operator== (FileSpec const & other_config) const;

	                                        private:
		typedef std::shared_ptr<AudioGrapher::SampleRateConverter> SRConverterPtr;

		template<typename T>
		void add_child_to_list (FileSpec const & new_config, boost::ptr_list<T> & list);

		ExportGraphBuilder &  parent;
		FileSpec              config;
		boost::ptr_list<SFC>  children;
		boost::ptr_list<Intermediate> intermediate_children;
		SRConverterPtr        converter;
		samplecnt_t           max_samples_out;
	};

	// Silence trimmer + adder
	class SilenceHandler {
	    public:
		SilenceHandler (ExportGraphBuilder & parent, FileSpec const & new_config, samplecnt_t max_samples);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		void remove_children (bool remove_out_files);
		bool operator== (FileSpec const & other_config) const;

	                                        private:
		typedef std::shared_ptr<AudioGrapher::SilenceTrimmer<Sample> > SilenceTrimmerPtr;

		ExportGraphBuilder & parent;
		FileSpec             config;
		boost::ptr_list<SRC> children;
		SilenceTrimmerPtr    silence_trimmer;
		samplecnt_t          max_samples_in;
	};

	// channel configuration
	class ChannelConfig {
	    public:
		ChannelConfig (ExportGraphBuilder & parent, FileSpec const & new_config, ChannelMap & channel_map);
		void add_child (FileSpec const & new_config);
		void remove_children (bool remove_out_files);
		bool operator== (FileSpec const & other_config) const;

	                                        private:
		typedef std::shared_ptr<AudioGrapher::Interleaver<Sample> > InterleaverPtr;
		typedef std::shared_ptr<AudioGrapher::Chunker<Sample> > ChunkerPtr;

		ExportGraphBuilder &      parent;
		FileSpec                  config;
		boost::ptr_list<SilenceHandler> children;
		InterleaverPtr            interleaver;
		ChunkerPtr                chunker;
		samplecnt_t               max_samples_out;
	};

	Session const & session;
	std::shared_ptr<ExportTimespan> timespan;

	// Roots for export processor trees
	typedef boost::ptr_list<ChannelConfig> ChannelConfigList;
	ChannelConfigList channel_configs;

	// The sources of all data, each channel is read only once
	ChannelMap channels;

	samplecnt_t process_buffer_samples;

	std::list<Intermediate *> intermediates;

	AnalysisMap analysis_map;

	bool        _realtime;
	samplecnt_t _master_align;

	Glib::ThreadPool     thread_pool;
	Glib::Threads::Mutex engine_request_lock;
};

} // namespace ARDOUR

