/*
 * Copyright (C) 2013-2014 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <sys/types.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/file_utils.h"
#include "gui_thread.h"

#include "ardour/filesystem_paths.h"

#include "transcode_ffmpeg.h"
#include "utils_videotl.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace VideoUtils;

TranscodeFfmpeg::TranscodeFfmpeg (std::string f)
	: infile(f)
{
	probeok = false;
	ffexecok = false;
	m_duration = 0;
	m_avoffset = m_lead_in = m_lead_out = 0;
	m_width = m_height = 0;
	m_aspect = m_fps = 0;
	m_sar = "";
#if 1 /* tentative debug mode */
	debug_enable = false;
#endif

	if (!ARDOUR::ArdourVideoToolPaths::transcoder_exe(ffmpeg_exe, ffprobe_exe)) {
		warning << string_compose(
				_(
					"ffmpeg installation was not found on this system.\n"
					"%1 requires ffmpeg and ffprobe from ffmpeg.org - version 1.1 or newer.\n"
					"Video import and export is not possible until you install tools.\n"
					"\n"
					"The tools are included with the %1 releases from ardour.org "
					"and also available with the video-server at http://x42.github.com/harvid/\n"
					"\n"
					"Important: the files need to be installed in $PATH and named ffmpeg_harvid and ffprobe_harvid.\n"
					"If you already have a suitable ffmpeg installation on your system, we recommend creating "
					"symbolic links from ffmpeg to ffmpeg_harvid and from ffprobe to ffprobe_harvid.\n"
					"\n"
					"see also http://manual.ardour.org/video-timeline/setup/"
				 ), PROGRAM_NAME) << endmsg;
		return;
	}
	ffexecok = true;

	if (infile.empty() || !probe()) {
		return;
	}
	probeok = true;
}

TranscodeFfmpeg::~TranscodeFfmpeg ()
{
  ;
}

bool
TranscodeFfmpeg::probe ()
{
	ffoutput = "";
	char **argp;
	argp=(char**) calloc(7,sizeof(char*));
	argp[0] = strdup(ffprobe_exe.c_str());
	argp[1] = strdup("-print_format");
	argp[2] = strdup("csv=nk=0");
	argp[3] = strdup("-show_format");
	argp[4] = strdup("-show_streams");
	argp[5] = strdup(infile.c_str());
	argp[6] = 0;
	ffcmd = new ARDOUR::SystemExec(ffprobe_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffprobeparse, this, _1 ,_2));
	ffcmd->Terminated.connect (*this, invalidator (*this), boost::bind (&TranscodeFfmpeg::ffexit, this), gui_context());
	if (ffcmd->start (SystemExec::IgnoreAndClose)) {
		ffexit();
		return false;
	}

	/* wait for ffprobe process to exit */
	ffcmd->wait();

	/* wait for interposer thread to copy all data.
	 * SystemExec::Terminated is emitted and ffcmd set to NULL */
	int timeout = 300; // 1.5 sec
	while (ffcmd && --timeout > 0) {
		Glib::usleep(5000);
		ARDOUR::GUIIdle();
	}
	if (timeout == 0 || ffoutput.empty()) {
		return false;
	}

	/* parse */

	std::vector<std::vector<std::string> > lines;
	ParseCSV(ffoutput, lines);
	double timebase = 0;
	m_width = m_height = 0;
	m_fps = m_aspect = 0;
	m_duration = 0;
	m_sar.clear();
	m_codec.clear();
	m_audio.clear();

#define PARSE_FRACTIONAL_FPS(VAR) \
	{ \
		std::string::size_type pos; \
		VAR = atof(value); \
		pos = value.find_first_of('/'); \
		if (pos != std::string::npos) { \
			VAR = atof(value.substr(0, pos)) / atof(value.substr(pos+1)); \
		} \
	}

	std::string duration_from_format;

	for (std::vector<std::vector<std::string> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		if (i->at(0) == X_("format")) {
			/* format,filename,#streams,format-name,format-long-name,start-time,duration,size,bitrate */
			for (std::vector<std::string>::iterator kv = i->begin(); kv != i->end(); ++kv) {
				const size_t kvsep = kv->find('=');
				if(kvsep == std::string::npos) continue;
				std::string key = kv->substr(0, kvsep);
				std::string value = kv->substr(kvsep + 1);
				if (key == X_("duration")) {
					duration_from_format = value;
				}
			}
		} else
		if (i->at(0) == X_("stream")) {
			if (i->at(5) == X_("codec_type=video") && m_width == 0) {

				for (std::vector<std::string>::iterator kv = i->begin(); kv != i->end(); ++kv) {
					const size_t kvsep = kv->find('=');
					if(kvsep == std::string::npos) continue;
					std::string key = kv->substr(0, kvsep);
					std::string value = kv->substr(kvsep + 1);

					if (key == X_("index")) {
						m_videoidx = atoi(value);
					} else if (key == X_("width")) {
						m_width = atoi(value);
					} else if (key == X_("height")) {
						m_height = atoi(value);
					} else if (key == X_("codec_name")) {
						if (!m_codec.empty()) m_codec += " ";
						m_codec += value;
					} else if (key == X_("codec_long_name")) {
						if (!m_codec.empty()) m_codec += " ";
						m_codec += "[" + value + "]";
					} else if (key == X_("codec_tag_string")) {
						if (!m_codec.empty()) m_codec += " ";
						m_codec += "(" + value + ")";
					} else if (key == X_("r_frame_rate")) {
						PARSE_FRACTIONAL_FPS(m_fps)
					} else if (key == X_("avg_frame_rate") && m_fps == 0) {
						PARSE_FRACTIONAL_FPS(m_fps)
					} else if (key == X_("time_base")) {
						PARSE_FRACTIONAL_FPS(timebase)
					} else if (key == X_("timecode") && m_duration == 0 && m_fps > 0) {
						int h,m,s; char f[32];
						if (sscanf(i->at(16).c_str(), "%d:%d:%d:%32s",&h,&m,&s,f) == 4) {
							m_duration = (ARDOUR::samplecnt_t) floor(m_fps * (
									h * 3600.0
								+ m * 60.0
								+ s * 1.0
								+ atoi(f) / pow((double)10, (int)strlen(f))
							));
						}
					} else if (key == X_("duration_ts") && m_fps == 0 && timebase !=0 ) {
						m_duration = atof(value) * m_fps * timebase;
					} else if (key == X_("duration") && m_fps != 0 && m_duration == 0) {
						m_duration = atof(value) * m_fps;
					} else if (key == X_("sample_aspect_ratio")) {
						std::string::size_type pos;
						pos = value.find_first_of(':');
						if (pos != std::string::npos && atof(value.substr(pos+1)) != 0) {
							m_sar = value;
							m_sar.replace(pos, 1, "/");
						}
					} else if (key == X_("display_aspect_ratio")) {
						std::string::size_type pos;
						pos = value.find_first_of(':');
						if (pos != std::string::npos && atof(value.substr(pos+1)) != 0) {
							m_aspect = atof(value.substr(0, pos)) / atof(value.substr(pos+1));
						}
					}
				}

				if (m_aspect == 0) {
					m_aspect = (double)m_width / (double)m_height;
				}

			} else if (i->at(5) == X_("codec_type=audio")) { /* new ffprobe */
				FFAudioStream as;
				for (std::vector<std::string>::iterator kv = i->begin(); kv != i->end(); ++kv) {
					const size_t kvsep = kv->find('=');
					if(kvsep == std::string::npos) continue;
					std::string key = kv->substr(0, kvsep);
					std::string value = kv->substr(kvsep + 1);

					if (key == X_("channels")) {
						as.channels   = atoi(value);
					} else if (key == X_("index")) {
						as.stream_id  = value;
					} else if (key == X_("codec_long_name")) {
						if (!as.name.empty()) as.name += " ";
						as.name += value;
					} else if (key == X_("codec_name")) {
						if (!as.name.empty()) as.name += " ";
						as.name += value;
					} else if (key == X_("sample_fmt")) {
						if (!as.name.empty()) as.name += " ";
						as.name += "FMT:" + value;
					} else if (key == X_("sample_rate")) {
						if (!as.name.empty()) as.name += " ";
						as.name += "SR:" + value;
					}

				}
				m_audio.push_back(as);
			}
		}
	}
	/* end parse */

	if (m_duration == 0 && !duration_from_format.empty() && m_fps > 0) {
		warning << "using video-duration from format (container)." << endmsg;
		m_duration = atof(duration_from_format) * m_fps;
	}

#if 0 /* DEBUG */
	printf("FPS: %f\n", m_fps);
	printf("Duration: %lu frames\n",(unsigned long)m_duration);
	printf("W/H: %ix%i\n",m_width, m_height);
	printf("aspect: %f\n",m_aspect);
	printf("codec: %s\n",m_codec.c_str());
	if (m_audio.size() > 0) {
		for (AudioStreams::iterator it = m_audio.begin(); it < m_audio.end(); ++it) {
			printf("audio: %s - %i channels\n",(*it).stream_id.c_str(), (*it).channels);
		}
	} else {
	  printf("audio: no audio streams in file.\n");
	}
#endif

	return true;
}

TranscodeFfmpeg::FFSettings
TranscodeFfmpeg::default_encoder_settings ()
{
	TranscodeFfmpeg::FFSettings ffs;
	ffs.clear();
	ffs["-vcodec"] = "mpeg4";
	ffs["-acodec"] = "ac3";
	ffs["-b:v"] = "5000k";
	ffs["-b:a"] = "160k";
	return ffs;
}

TranscodeFfmpeg::FFSettings
TranscodeFfmpeg::default_meta_data ()
{
	TranscodeFfmpeg::FFSettings ffm;
	ffm.clear();
	ffm["comment"] = "Created with " PROGRAM_NAME;
	return ffm;
}


bool
TranscodeFfmpeg::encode (std::string outfile, std::string inf_a, std::string inf_v, TranscodeFfmpeg::FFSettings ffs, TranscodeFfmpeg::FFSettings meta, bool map)
{
#define MAX_FFMPEG_ENCODER_ARGS (100)
	char **argp;
	int a=0;

	argp=(char**) calloc(MAX_FFMPEG_ENCODER_ARGS,sizeof(char*));
	argp[a++] = strdup(ffmpeg_exe.c_str());
	if (m_avoffset < 0 || m_avoffset > 0) {
		std::ostringstream osstream; osstream << m_avoffset;
		argp[a++] = strdup("-itsoffset");
		argp[a++] = strdup(osstream.str().c_str());
	}
	argp[a++] = strdup("-i");
	argp[a++] = strdup(inf_v.c_str());

	argp[a++] = strdup("-i");
	argp[a++] = strdup(inf_a.c_str());

	for(TranscodeFfmpeg::FFSettings::const_iterator it = ffs.begin(); it != ffs.end(); ++it) {
		argp[a++] = strdup(it->first.c_str());
		argp[a++] = strdup(it->second.c_str());
	}
	for(TranscodeFfmpeg::FFSettings::const_iterator it = meta.begin(); it != meta.end(); ++it) {
		argp[a++] = strdup("-metadata");
		argp[a++] = SystemExec::format_key_value_parameter (it->first.c_str(), it->second.c_str());
	}

	if (m_fps > 0) {
		m_lead_in  = rint (m_lead_in * m_fps) / m_fps;
		m_lead_out = rint (m_lead_out * m_fps) / m_fps;
	}

	if (m_lead_in != 0 && m_lead_out != 0) {
		std::ostringstream osstream;
		argp[a++] = strdup("-vf");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_in;
		if (!m_sar.empty()) osstream << X_(":sar=") << m_sar;
		osstream << X_(" [pre]; ");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_out;
		if (!m_sar.empty()) osstream << X_(":sar=") << m_sar;
		osstream << X_(" [post]; ");
		osstream << X_("[pre] [in] [post] concat=n=3");
		argp[a++] = strdup(osstream.str().c_str());
	} else if (m_lead_in != 0) {
		std::ostringstream osstream;
		argp[a++] = strdup("-vf");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_in;
		if (!m_sar.empty()) osstream << X_(":sar=") << m_sar;
		osstream << X_(" [pre]; ");
		osstream << X_("[pre] [in] concat=n=2");
		argp[a++] = strdup(osstream.str().c_str());
	} else if (m_lead_out != 0) {
		std::ostringstream osstream;
		argp[a++] = strdup("-vf");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_out;
		if (!m_sar.empty()) osstream << X_(":sar=") << m_sar;
		osstream << X_(" [post]; ");
		osstream << X_("[in] [post] concat=n=2");
		argp[a++] = strdup(osstream.str().c_str());
	}

	if (map) {
		std::ostringstream osstream;
		argp[a++] = strdup("-map");
		osstream << X_("0:") << m_videoidx;
		argp[a++] = strdup(osstream.str().c_str());
		argp[a++] = strdup("-map");
		argp[a++] = strdup("1:0");
	}

	argp[a++] = strdup("-y");
	argp[a++] = strdup(outfile.c_str());
	argp[a] = (char *)0;
	assert(a<MAX_FFMPEG_ENCODER_ARGS);
	/* Note: these are free()d in ~SystemExec */
#if 1 /* DEBUG */
	if (debug_enable) { /* tentative debug mode */
	printf("EXPORT ENCODE:\n");
	for (int i=0; i< a; ++i) {
	  printf("%s ", argp[i]);
	}
	printf("\n");
	}
#endif

	ffcmd = new ARDOUR::SystemExec(ffmpeg_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffmpegparse_v, this, _1 ,_2));
	ffcmd->Terminated.connect (*this, invalidator (*this), boost::bind (&TranscodeFfmpeg::ffexit, this), gui_context());
	if (ffcmd->start (SystemExec::MergeWithStdin)) {
		ffexit();
		return false;
	}
	return true;
}

bool
TranscodeFfmpeg::extract_audio (std::string outfile, ARDOUR::samplecnt_t /*samplerate*/, unsigned int stream)
{
	if (!probeok) return false;
  if (stream >= m_audio.size()) return false;

	char **argp;
	int i = 0;

	argp=(char**) calloc(15,sizeof(char*));
	argp[i++] = strdup(ffmpeg_exe.c_str());
	argp[i++] = strdup("-i");
	argp[i++] = strdup(infile.c_str());
#if 0 /* ffmpeg write original samplerate, use a3/SRC to resample */
	argp[i++] = strdup("-ar");
	argp[i] = (char*) calloc(7,sizeof(char)); snprintf(argp[i++], 7, "%"PRId64, samplerate);
#endif
	argp[i++] = strdup("-ac");
	argp[i] = (char*) calloc(3,sizeof(char)); snprintf(argp[i++], 3, "%i", m_audio.at(stream).channels);
	argp[i++] = strdup("-map");
	argp[i] = (char*) calloc(8,sizeof(char)); snprintf(argp[i++], 8, "0:%s", m_audio.at(stream).stream_id.c_str());
	argp[i++] = strdup("-vn");
	argp[i++] = strdup("-acodec");
	argp[i++] = strdup("pcm_f32le");
	argp[i++] = strdup("-y");
	argp[i++] = strdup(outfile.c_str());
	argp[i++] = (char *)0;
	/* Note: argp is free()d in ~SystemExec */
#if 1 /* DEBUG */
	if (debug_enable) { /* tentative debug mode */
	printf("EXTRACT AUDIO:\n");
	for (int i=0; i< 14; ++i) {
	  printf("%s ", argp[i]);
	}
	printf("\n");
	}
#endif

	ffcmd = new ARDOUR::SystemExec(ffmpeg_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffmpegparse_a, this, _1 ,_2));
	ffcmd->Terminated.connect (*this, invalidator (*this), boost::bind (&TranscodeFfmpeg::ffexit, this), gui_context());
	if (ffcmd->start (SystemExec::MergeWithStdin)) {
		ffexit();
		return false;
	}
	return true;
}


bool
TranscodeFfmpeg::transcode (std::string outfile, const int outw, const int outh, const int kbitps)
{
	if (!probeok) return false;

	char **argp;
	int bitrate = kbitps;
	int width = outw;
	int height = outh;

	if (width < 1 || width > m_width) { width = m_width; } /* don't allow upscaling */
	if (height < 1 || height > m_height) { height = floor(width / m_aspect); }

	if (bitrate == 0) {
		const double bitperpixel = .7; /* avg quality */
		bitrate = floor(m_fps * width * height * bitperpixel / 10000.0);
	} else {
		bitrate = bitrate / 10;
	}
	if (bitrate < 10)  bitrate = 10;
	if (bitrate > 1000) bitrate = 1000;

	argp=(char**) calloc(16,sizeof(char*));
	argp[0] = strdup(ffmpeg_exe.c_str());
	argp[1] = strdup("-i");
	argp[2] = strdup(infile.c_str());
	argp[3] = strdup("-b:v");
	argp[4] = (char*) calloc(7,sizeof(char)); snprintf(argp[4], 7, "%i0k", bitrate);
	argp[5] = strdup("-s");
	argp[6] = (char*) calloc(10,sizeof(char)); snprintf(argp[6], 10, "%ix%i", width, height);
	argp[7] = strdup("-y");
	argp[8] = strdup("-vcodec");
	argp[9] = strdup("mjpeg");
	argp[10] = strdup("-an");
	argp[11] = strdup("-intra");
	argp[12] = strdup("-g");
	argp[13] = strdup("1");
	argp[14] = strdup(outfile.c_str());
	argp[15] = (char *)0;
	/* Note: these are free()d in ~SystemExec */
#if 1 /* DEBUG */
	if (debug_enable) { /* tentative debug mode */
	printf("TRANSCODE VIDEO:\n");
	for (int i=0; i< 15; ++i) {
	  printf("%s ", argp[i]);
	}
	printf("\n");
	}
#endif
	ffcmd = new ARDOUR::SystemExec(ffmpeg_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffmpegparse_v, this, _1 ,_2));
	ffcmd->Terminated.connect (*this, invalidator (*this), boost::bind (&TranscodeFfmpeg::ffexit, this), gui_context());
	if (ffcmd->start (SystemExec::MergeWithStdin)) {
		ffexit();
		return false;
	}
	return true;
}

void
TranscodeFfmpeg::cancel ()
{
	if (!ffcmd || !ffcmd->is_running()) { return;}
	ffcmd->write_to_stdin("q");
#ifdef PLATFORM_WINDOWS
	Sleep(1000);
#else
	sleep (1);
#endif
	if (ffcmd) {
	  ffcmd->terminate();
	}
}

void
TranscodeFfmpeg::ffexit ()
{
	delete ffcmd;
	ffcmd=0;
	Finished(); /* EMIT SIGNAL */
}

void
TranscodeFfmpeg::ffprobeparse (std::string d, size_t /* s */)
{
	ffoutput+=d;
}

void
TranscodeFfmpeg::ffmpegparse_a (std::string d, size_t /* s */)
{
	const char *t;
	int h,m,s; char f[7];
	ARDOUR::samplecnt_t p = -1;

	if (!(t=strstr(d.c_str(), "time="))) { return; }

	if (sscanf(t+5, "%d:%d:%d.%s",&h,&m,&s,f) == 4) {
		p = (ARDOUR::samplecnt_t) floor( 100.0 * (
		      h * 3600.0
		    + m * 60.0
		    + s * 1.0
		    + atoi(f) / pow((double)10, (int)strlen(f))
		));
		p = p * m_fps / 100.0;
		if (p > m_duration ) { p = m_duration; }
		Progress(p, m_duration); /* EMIT SIGNAL */
	} else {
		Progress(0, 0); /* EMIT SIGNAL */
	}
}

void
TranscodeFfmpeg::ffmpegparse_v (std::string d, size_t /* s */)
{
	if (strstr(d.c_str(), "ERROR") || strstr(d.c_str(), "Error") || strstr(d.c_str(), "error")) {
		warning << "ffmpeg-error: " << d << endmsg;
	}
	if (strncmp(d.c_str(), "frame=",6)) {
#if 1 /* DEBUG */
		if (debug_enable) {
			d.erase(d.find_last_not_of(" \t\r\n") + 1);
		  printf("ffmpeg: '%s'\n", d.c_str());
		}
#endif
		return;
	}
	ARDOUR::samplecnt_t f = atol(d.substr(6));
	if (f == 0) {
		Progress(0, 0); /* EMIT SIGNAL */
	} else {
		Progress(f, m_duration); /* EMIT SIGNAL */
	}
}
