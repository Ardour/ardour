/*
    Copyright (C) 2009 Paul Davis
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

#ifndef __ardour_export_graph_builder_h__
#define __ardour_export_graph_builder_h__

#include "ardour/export_handler.h"

#include "audiographer/utils/identity_vertex.h"

#include <boost/ptr_container/ptr_list.hpp>
#include <glibmm/threadpool.h>

namespace AudioGrapher {
	class SampleRateConverter;
	class PeakReader;
	class Normalizer;
	template <typename T> class Chunker;
	template <typename T> class SampleFormatConverter;
	template <typename T> class Interleaver;
	template <typename T> class SndfileWriter;
	template <typename T> class SilenceTrimmer;
	template <typename T> class TmpFile;
	template <typename T> class Threader;
	template <typename T> class AllocatingProcessContext;
}

namespace ARDOUR
{

class ExportTimespan;
class Session;

class LIBARDOUR_API ExportGraphBuilder
{
  private:
	typedef ExportHandler::FileSpec FileSpec;

	typedef boost::shared_ptr<AudioGrapher::Sink<Sample> > FloatSinkPtr;
	typedef boost::shared_ptr<AudioGrapher::IdentityVertex<Sample> > IdentityVertexPtr;
	typedef std::map<ExportChannelPtr,  IdentityVertexPtr> ChannelMap;

  public:

	ExportGraphBuilder (Session const & session);
	~ExportGraphBuilder ();

	int process (framecnt_t frames, bool last_cycle);
	bool process_normalize (); // returns true when finished
	bool will_normalize() { return !normalizers.empty(); }
	unsigned get_normalize_cycle_count() const;

	void reset ();
	void set_current_timespan (boost::shared_ptr<ExportTimespan> span);
	void add_config (FileSpec const & config);

  private:

	void add_split_config (FileSpec const & config);

	class Encoder {
	  public:
		template <typename T> boost::shared_ptr<AudioGrapher::Sink<T> > init (FileSpec const & new_config);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;

		static int get_real_format (FileSpec const & config);

	  private:
		typedef boost::shared_ptr<AudioGrapher::SndfileWriter<Sample> > FloatWriterPtr;
		typedef boost::shared_ptr<AudioGrapher::SndfileWriter<int> >    IntWriterPtr;
		typedef boost::shared_ptr<AudioGrapher::SndfileWriter<short> >  ShortWriterPtr;

		template<typename T> void init_writer (boost::shared_ptr<AudioGrapher::SndfileWriter<T> > & writer);
		void copy_files (std::string orig_path);

		FileSpec               config;
		std::list<ExportFilenamePtr> filenames;
		PBD::ScopedConnection  copy_files_connection;

		// Only one of these should be available at a time
		FloatWriterPtr float_writer;
		IntWriterPtr   int_writer;
		ShortWriterPtr short_writer;
	};

	// sample format converter
	class SFC {
	  public:
		// This constructor so that this can be constructed like a Normalizer
		SFC (ExportGraphBuilder &, FileSpec const & new_config, framecnt_t max_frames);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;

	  private:
		typedef boost::shared_ptr<AudioGrapher::SampleFormatConverter<Sample> > FloatConverterPtr;
		typedef boost::shared_ptr<AudioGrapher::SampleFormatConverter<int> >   IntConverterPtr;
		typedef boost::shared_ptr<AudioGrapher::SampleFormatConverter<short> > ShortConverterPtr;

		FileSpec           config;
		boost::ptr_list<Encoder> children;
		int                data_width;

		// Only one of these should be available at a time
		FloatConverterPtr float_converter;
		IntConverterPtr int_converter;
		ShortConverterPtr short_converter;
	};

	class Normalizer {
	  public:
		Normalizer (ExportGraphBuilder & parent, FileSpec const & new_config, framecnt_t max_frames);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;

		unsigned get_normalize_cycle_count() const;

		/// Returns true when finished
		bool process ();

	  private:
		typedef boost::shared_ptr<AudioGrapher::PeakReader> PeakReaderPtr;
		typedef boost::shared_ptr<AudioGrapher::Normalizer> NormalizerPtr;
		typedef boost::shared_ptr<AudioGrapher::TmpFile<Sample> > TmpFilePtr;
		typedef boost::shared_ptr<AudioGrapher::Threader<Sample> > ThreaderPtr;
		typedef boost::shared_ptr<AudioGrapher::AllocatingProcessContext<Sample> > BufferPtr;

		void start_post_processing();

		ExportGraphBuilder & parent;

		FileSpec        config;
		framecnt_t      max_frames_out;

		BufferPtr       buffer;
		PeakReaderPtr   peak_reader;
		TmpFilePtr      tmp_file;
		NormalizerPtr   normalizer;
		ThreaderPtr     threader;
		boost::ptr_list<SFC> children;

		PBD::ScopedConnection post_processing_connection;
	};

	// sample rate converter
	class SRC {
	  public:
		SRC (ExportGraphBuilder & parent, FileSpec const & new_config, framecnt_t max_frames);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;

	  private:
		typedef boost::shared_ptr<AudioGrapher::SampleRateConverter> SRConverterPtr;

		template<typename T>
		void add_child_to_list (FileSpec const & new_config, boost::ptr_list<T> & list);

		ExportGraphBuilder &  parent;
		FileSpec              config;
		boost::ptr_list<SFC>  children;
		boost::ptr_list<Normalizer> normalized_children;
		SRConverterPtr        converter;
		framecnt_t            max_frames_out;
	};

	// Silence trimmer + adder
	class SilenceHandler {
	  public:
		SilenceHandler (ExportGraphBuilder & parent, FileSpec const & new_config, framecnt_t max_frames);
		FloatSinkPtr sink ();
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;

	  private:
		typedef boost::shared_ptr<AudioGrapher::SilenceTrimmer<Sample> > SilenceTrimmerPtr;

		ExportGraphBuilder & parent;
		FileSpec             config;
		boost::ptr_list<SRC> children;
		SilenceTrimmerPtr    silence_trimmer;
		framecnt_t           max_frames_in;
	};

	// channel configuration
	class ChannelConfig {
	  public:
		ChannelConfig (ExportGraphBuilder & parent, FileSpec const & new_config, ChannelMap & channel_map);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;

	  private:
		typedef boost::shared_ptr<AudioGrapher::Interleaver<Sample> > InterleaverPtr;
		typedef boost::shared_ptr<AudioGrapher::Chunker<Sample> > ChunkerPtr;

		ExportGraphBuilder &      parent;
		FileSpec                  config;
		boost::ptr_list<SilenceHandler> children;
		InterleaverPtr            interleaver;
		ChunkerPtr                chunker;
		framecnt_t                max_frames_out;
	};

	Session const & session;
	boost::shared_ptr<ExportTimespan> timespan;

	// Roots for export processor trees
	typedef boost::ptr_list<ChannelConfig> ChannelConfigList;
	ChannelConfigList channel_configs;

	// The sources of all data, each channel is read only once
	ChannelMap channels;

	framecnt_t process_buffer_frames;

	std::list<Normalizer *> normalizers;

	Glib::ThreadPool thread_pool;
};

} // namespace ARDOUR

#endif /* __ardour_export_graph_builder_h__ */
