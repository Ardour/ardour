/*
 * Copyright (C) 2013-2018 Robin Gareus <robin@gareus.org>
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
#ifndef __ardour_transcode_ffmpeg_h__
#define __ardour_transcode_ffmpeg_h__

#include <string>
#include "ardour/system_exec.h"
#include "ardour/types.h"


/** @class TranscodeFfmpeg
 *  @brief wrapper around ffmpeg and ffprobe command-line utils
 *
 *  This class includes parsers for stdi/o communication with
 *  'ffmpeg' and 'ffprobe' and provide an abstraction to
 *  transcode video-files and extract aufio tracks and query
 *  file information.
 */
class TranscodeFfmpeg : public sigc::trackable
                      , public PBD::ScopedConnectionList
{
	public:

	struct FFAudioStream {
		std::string name;
		std::string stream_id;
		uint32_t channels;
	};
	typedef std::vector<FFAudioStream> FFAudioStreams;
	typedef std::map<std::string,std::string> FFSettings;


		/** instantiate a new transcoder. If a file-name is given, the file's
		 * attributes (fps, duration, geometry etc) are read.
		 *
		 * @param f path to the video-file to probe or use as input for \ref extract_audio and \ref transcode .
		 */
		TranscodeFfmpeg (std::string f);
		virtual ~TranscodeFfmpeg ();

		/** transcode to (import a video-file)
		 *
		 * @param outfile full-path (incl. file-extension) of the file to create
		 * @param width video-width, if \c <0 no scaling
		 * @param height video-height, with \c <0 preserve aspect (\p width \c / \c aspect-ratio)
		 * @param kbitps video bitrate, with \c 0 calculate to use 0.7 bits per pixel on average
		 * @return \c true if the transcoder process was successfully started.
		 */
		bool transcode (std::string outfile, const int width=0, const int height=0, const int kbitps =0);

		/** Extract an audio track from the given input file to a new 32bit float little-endian PCM WAV file.
		 * @param outfile full-path (incl. file-extension) to .wav file to write
		 * @param samplerate target samplerate
		 * @param stream Stream-ID of the audio-track to extract
		 * specified as element-number in \ref get_audio().
		 * @return \c true if the transcoder process was successfully started.
		 */
		bool extract_audio (std::string outfile, ARDOUR::samplecnt_t samplerate, unsigned int stream=0);

		/** transcode video and mux audio files into a new video-file.
		 * @param outfile full-path of output file to create (existing files are overwritten)
		 * @param inf_a filename of input audio-file
		 * @param inf_v filename of input video-file
		 * @param ffs additional command-line parameters for 'ffmpeg'. key/value pairs
		 * eg ffs["-vcodec"] = "mpeg4"
		 * @param meta additional meta-data results in -metadata "<key>"="<value>" command-line
		 * arguments
		 * @param map if set to \c true stream mapping from input streams to output streams is set to use
		 * only the first available stream from the audio & video file (-map 0.0 -map 1.0).
		 * @return \c true if the encoder process was successfully started.
		 */
		bool encode (std::string outfile, std::string inf_a, std::string inf_v, FFSettings ffs, FFSettings meta, bool map = true);

		/** @return array with default encoder settings */
		FFSettings default_encoder_settings ();
		/** @return array with default meta data */
		FFSettings default_meta_data ();
		/** abort any running transcoding process */
		void cancel();
		/**
		 * @return \c true if the input file was parsed correctly on class creation. */
		bool probe_ok () { return probeok; }
		/** @return \c true if the ffmpeg/ffparse executables are avail on this system */
		bool ffexec_ok () { return ffexecok; }

		/** signal emitted when ffmpeg reports progress updates
		 * during \ref encode \ref transcode and \ref extract_audio
		 * The parameters are current and last video-frame.
		 */
		PBD::Signal2<void, ARDOUR::samplecnt_t, ARDOUR::samplecnt_t> Progress;
		/** signal emitted when the transcoder process terminates. */
		PBD::Signal0<void> Finished;

		double get_fps () { return m_fps; }
		double get_aspect () { return m_aspect; }
		int    get_width() { return m_width; }
		int    get_height() { return m_height; }
		ARDOUR::samplecnt_t get_duration() { return m_duration; }
		std::string  get_codec() { return m_codec; }

		FFAudioStreams get_audio() { return m_audio; }

		/** override file duration used with the \ref Progress signal.
		 * @param d duration in video-frames = length_in_seconds * get_fps()
		 */
		void set_duration(ARDOUR::samplecnt_t d) { m_duration = d; }

		/* offset, lead-in/out are in seconds */
		void set_avoffset(double av_offset) { m_avoffset = av_offset; }
		void set_leadinout(double lead_in, double lead_out) { m_lead_in = lead_in; m_lead_out = lead_out; }

		void set_fps(double fps) { m_fps = fps; } // on export, used for rounding only.

#if 1 /* tentative debug mode */
		void   set_debug (bool onoff) { debug_enable = onoff; }
#endif
	protected:
		std::string infile;
		ARDOUR::SystemExec  *ffcmd;

		bool probe ();

		double m_fps;
		double m_aspect;
		std::string m_sar;
		ARDOUR::samplecnt_t m_duration;
		int m_width;
		int m_height;
		std::string m_codec;

		int m_videoidx;
		double m_avoffset;
		double m_lead_in;
		double m_lead_out;
		bool ffexecok;
		bool probeok;

		FFAudioStreams m_audio;

		void ffmpegparse_v (std::string d, size_t s);
		void ffmpegparse_a (std::string d, size_t s);
		void ffprobeparse (std::string d, size_t s);
		void ffexit ();
		std::string ffoutput;

		std::string ffmpeg_exe;
		std::string ffprobe_exe;
#if 1 /* tentative debug mode */
		bool debug_enable;
#endif
};

#endif /* __ardour_transcode_ffmpeg_h__ */
