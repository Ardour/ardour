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

#ifndef __ardour_export_processor_h__
#define __ardour_export_processor_h__

#include <vector>

#include <boost/smart_ptr.hpp>
#include <glibmm/ustring.h>

#include "ardour/graph.h"
#include "ardour/export_file_io.h"
#include "ardour/export_utilities.h"

namespace ARDOUR
{

class Session;
class ExportStatus;
class ExportFilename;
class ExportFormatSpecification;

/// Sets up components for export post processing
class ExportProcessor
{
  private:
	/* Typedefs for utility processors */

	typedef boost::shared_ptr<SampleRateConverter> SRConverterPtr;
	typedef boost::shared_ptr<PeakReader> PReaderPtr;
	typedef boost::shared_ptr<Normalizer> NormalizerPtr;
	typedef boost::shared_ptr<ExportTempFile> TempFilePtr;

	typedef GraphSink<float> FloatSink;
	typedef boost::shared_ptr<FloatSink> FloatSinkPtr;
	typedef std::vector<FloatSinkPtr> FloatSinkVect;

	typedef boost::shared_ptr<ExportFilename> FilenamePtr;
	typedef boost::shared_ptr<ExportFormatSpecification const> FormatPtr;

	typedef boost::shared_ptr<ExportFileWriter> FileWriterPtr;
	typedef std::list<FileWriterPtr> FileWriterList;

  public:

	ExportProcessor (Session & session);
	~ExportProcessor ();
	ExportProcessor * copy() { return new ExportProcessor (session); }

	/// Do preparations for exporting
	/** Should be called before process
	 * @return 0 on success
	 */
	int prepare (FormatPtr format, FilenamePtr fname, uint32_t chans, bool split = false, nframes_t start = 0);

	/// Process data
	/** @param frames frames to process @return frames written **/
	nframes_t process (float * data, nframes_t frames);

	/** should be called after all data is given to process **/
	void prepare_post_processors ();

	void write_files ();

	static sigc::signal<void, Glib::ustring> WritingFile;

  private:

	void reset ();

	Session &                       session;
	boost::shared_ptr<ExportStatus> status;

	/* these are initalized in prepare() */

	FilenamePtr      filename;
	NormalizerPtr    normalizer;
	SRConverterPtr   src;
	PReaderPtr       peak_reader;
	TempFilePtr      temp_file;
	FloatSinkVect    file_sinks;
	FileWriterList   writer_list;

	/* general info */

	uint32_t         channels;
	nframes_t        blocksize;
	nframes_t        frame_rate;

	/* Processing */

	bool             tag;
	bool             broadcast_info;
	bool             split_files;
	bool             normalize;
	bool             trim_beginning;
	bool             trim_end;
	nframes_t        silence_beginning;
	nframes_t        silence_end;

	/* Progress info */

	nframes_t        temp_file_position;
	nframes_t        temp_file_length;
};

} // namespace ARDOUR

#endif /* __ardour_export_processor_h__ */
