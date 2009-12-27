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

#include "ardour/ardour.h"
#include "ardour/export_handler.h"
#include "ardour/export_channel.h"
#include "ardour/export_format_base.h"

#include "audiographer/identity_vertex.h"

#include <glibmm/threadpool.h>

namespace AudioGrapher {
	class SampleRateConverter;
	class PeakReader;
	class Normalizer;
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

class ExportGraphBuilder
{
  private:
	typedef ExportHandler::FileSpec FileSpec;
	typedef ExportElementFactory::FilenamePtr FilenamePtr;

	typedef boost::shared_ptr<AudioGrapher::Sink<Sample> > FloatSinkPtr;
	typedef boost::shared_ptr<AudioGrapher::IdentityVertex<Sample> > IdentityVertexPtr;
	typedef std::map<ExportChannelPtr,  IdentityVertexPtr> ChannelMap;

  public:
	
	ExportGraphBuilder (Session const & session);
	~ExportGraphBuilder ();
	
	int process (nframes_t frames, bool last_cycle);
	
	void reset ();
	void add_config (FileSpec const & config);
	
  private:
	
	class Encoder : public sigc::trackable {
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
		std::list<FilenamePtr> filenames;
		
		// Only one of these should be available at a time
		FloatWriterPtr float_writer;
		IntWriterPtr   int_writer;
		ShortWriterPtr short_writer;
	};
	
	// sample format converter
	class SFC {
	  public:
		// This constructor so that this can be constructed like a Normalizer
		SFC (ExportGraphBuilder &) {}
		FloatSinkPtr init (FileSpec const & new_config, nframes_t max_frames);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;
		
	  private:
		typedef boost::shared_ptr<AudioGrapher::SampleFormatConverter<Sample> > FloatConverterPtr;
		typedef boost::shared_ptr<AudioGrapher::SampleFormatConverter<int> >   IntConverterPtr;
		typedef boost::shared_ptr<AudioGrapher::SampleFormatConverter<short> > ShortConverterPtr;
		
		FileSpec           config;
		std::list<Encoder> children;
		int                data_width;
		
		// Only one of these should be available at a time
		FloatConverterPtr float_converter;
		IntConverterPtr   int_converter;
		ShortConverterPtr short_converter;
	};
	
	class Normalizer : public sigc::trackable {
	  public:
		Normalizer (ExportGraphBuilder & parent) : parent (parent) {}
		FloatSinkPtr init (FileSpec const & new_config, nframes_t max_frames);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;
		
	  private:
		typedef boost::shared_ptr<AudioGrapher::PeakReader> PeakReaderPtr;
		typedef boost::shared_ptr<AudioGrapher::Normalizer> NormalizerPtr;
		typedef boost::shared_ptr<AudioGrapher::TmpFile<Sample> > TmpFilePtr;
		typedef boost::shared_ptr<AudioGrapher::Threader<Sample> > ThreaderPtr;
		typedef boost::shared_ptr<AudioGrapher::AllocatingProcessContext<Sample> > BufferPtr;
		
		void start_post_processing();
		void do_post_processing();
		
		ExportGraphBuilder & parent;
		
		FileSpec        config;
		nframes_t       max_frames_out;
		
		BufferPtr       buffer;
		PeakReaderPtr   peak_reader;
		TmpFilePtr      tmp_file;
		NormalizerPtr   normalizer;
		ThreaderPtr     threader;
		std::list<SFC>  children;
	};
	
	// sample rate converter
	class SRC {
	  public:
		SRC (ExportGraphBuilder & parent) : parent (parent) {}
		FloatSinkPtr init (FileSpec const & new_config, nframes_t max_frames);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;
		
	  private:
		typedef boost::shared_ptr<AudioGrapher::SampleRateConverter> SRConverterPtr;
		
		template<typename T>
		void add_child_to_list (FileSpec const & new_config, std::list<T> & list);
  
		ExportGraphBuilder &  parent;
		FileSpec              config;
		std::list<SFC>        children;
		std::list<Normalizer> normalized_children;
		SRConverterPtr        converter;
		nframes_t             max_frames_out;
	};
	
	// Silence trimmer + adder
	class SilenceHandler {
	  public:
		SilenceHandler (ExportGraphBuilder & parent) : parent (parent) {}
		FloatSinkPtr init (FileSpec const & new_config, nframes_t max_frames);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;
		
	  private:
		typedef boost::shared_ptr<AudioGrapher::SilenceTrimmer<Sample> > SilenceTrimmerPtr;
		
		ExportGraphBuilder & parent;
		FileSpec             config;
		std::list<SRC>       children;
		SilenceTrimmerPtr    silence_trimmer;
		nframes_t            max_frames_in;
	};
	
	// channel configuration
	class ChannelConfig {
	  public:
		ChannelConfig (ExportGraphBuilder & parent) : parent (parent) {}
		void init (FileSpec const & new_config, ChannelMap & channel_map);
		void add_child (FileSpec const & new_config);
		bool operator== (FileSpec const & other_config) const;
		
	  private:
		typedef boost::shared_ptr<AudioGrapher::Interleaver<Sample> > InterleaverPtr;
		
		ExportGraphBuilder &      parent;
		FileSpec                  config;
		std::list<SilenceHandler> children;
		InterleaverPtr            interleaver;
		nframes_t                 max_frames;
	};

	Session const & session;
	
	// Roots for export processor trees
	typedef std::list<ChannelConfig> ChannelConfigList;
	ChannelConfigList channel_configs;
	
	// The sources of all data, each channel is read only once
	ChannelMap channels;
	
	Sample *  process_buffer;
	nframes_t process_buffer_frames;
	
	Glib::ThreadPool thread_pool;
};

} // namespace ARDOUR

#endif /* __ardour_export_graph_builder_h__ */
