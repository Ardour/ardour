/*
    Copyright (C) 2010-2013 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#ifdef WITH_VIDEOTIMELINE

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <sys/types.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/file_utils.h"
#include "gui_thread.h"

#include "transcode_ffmpeg.h"
#include "utils_videotl.h"

#include "i18n.h"

TranscodeFfmpeg::TranscodeFfmpeg (std::string f)
	: infile(f)
{
	probeok = false;
	ffexecok = false;
	ffmpeg_exe = "";
	ffprobe_exe = "";
	m_duration = 0;
	m_avoffset = m_lead_in = m_lead_out = 0;
	m_width = m_height = 0;
	m_aspect = m_fps = 0;
#if 1 /* tentative debug mode */
	debug_enable = false;
#endif

	std::string ff_file_path;
	if (find_file_in_search_path (PBD::SearchPath(Glib::getenv("PATH")), X_("ffmpeg_harvid"), ff_file_path)) { ffmpeg_exe = ff_file_path; }
	else if (Glib::file_test(X_("C:\\Program Files\\harvid\\ffmpeg.exe"), Glib::FILE_TEST_EXISTS)) {
		ffmpeg_exe = X_("C:\\Program Files\\ffmpeg\\ffmpeg.exe");
	}
	else if (Glib::file_test(X_("C:\\Program Files\\ffmpeg\\ffmpeg.exe"), Glib::FILE_TEST_EXISTS)) {
		ffmpeg_exe = X_("C:\\Program Files\\ffmpeg\\ffmpeg.exe");
	}

	if (find_file_in_search_path (PBD::SearchPath(Glib::getenv("PATH")), X_("ffprobe_harvid"), ff_file_path)) { ffprobe_exe = ff_file_path; }
	else if (Glib::file_test(X_("C:\\Program Files\\harvid\\ffprobe.exe"), Glib::FILE_TEST_EXISTS)) {
		ffprobe_exe = X_("C:\\Program Files\\ffmpeg\\ffprobe.exe");
	}
	else if (Glib::file_test(X_("C:\\Program Files\\ffmpeg\\ffprobe.exe"), Glib::FILE_TEST_EXISTS)) {
		ffprobe_exe = X_("C:\\Program Files\\ffmpeg\\ffprobe.exe");
	}

	if (ffmpeg_exe.empty() || ffprobe_exe.empty()) {
		PBD::warning << _(
				"No ffprobe or ffmpeg executables could be found on this system.\n"
				"Video import and export is not possible until you install those tools.\n"
				"Ardour requires ffmpeg and ffprobe from ffmpeg.org - version 1.1 or newer.\n"
				"\n"
				"The tools are included with the Ardour releases from ardour.org "
				"and also available with the video-server at http://x42.github.com/harvid/\n"
				"\n"
				"Important: the files need to be installed in $PATH and named ffmpeg_harvid and ffprobe_harvid.\n"
				"If you already have a suitable ffmpeg installation on your system, we recommend creating "
				"symbolic links from ffmpeg to ffmpeg_harvid and from ffprobe to ffprobe_harvid.\n"
				) << endmsg;
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
	ffcmd = new SystemExec(ffprobe_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffprobeparse, this, _1 ,_2));
	ffcmd->Terminated.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffexit, this));
	if (ffcmd->start(1)) {
		ffexit();
		return false;
	}
	ffcmd->wait();

	/* parse */

	std::vector<std::vector<std::string> > lines;
	ParseCSV(ffoutput, lines);
	double timebase = 0;
	m_width = m_height = 0;
	m_fps = m_aspect = 0;
	m_duration = 0;
	m_codec.clear();
	m_audio.clear();

#define PARSE_FRACTIONAL_FPS(VAR) \
	{ \
		std::string::size_type pos; \
		VAR = atof(value.c_str()); \
		pos = value.find_first_of('/'); \
		if (pos != std::string::npos) { \
			VAR = atof(value.substr(0, pos).c_str()) / atof(value.substr(pos+1).c_str()); \
		} \
	}

	for (std::vector<std::vector<std::string> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		if (i->at(0) == X_("format")) {
			/* format,filename,#streams,format-name,format-long-name,start-time,duration,size,bitrate */
		} else
		if (i->at(0) == X_("stream")) {
			if (i->at(5) == X_("codec_type=video") && m_width == 0) {

				for (std::vector<std::string>::iterator kv = i->begin(); kv != i->end(); ++kv) {
					const size_t kvsep = kv->find('=');
					if(kvsep == std::string::npos) continue;
					std::string key = kv->substr(0, kvsep);
					std::string value = kv->substr(kvsep + 1);

					if (key == X_("index")) {
						m_videoidx = atoi(value.c_str());
					} else if (key == X_("width")) {
						m_width = atoi(value.c_str());
					} else if (key == X_("height")) {
						m_height = atoi(value.c_str());
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
					} else if (key == X_("timecode") && m_duration == 0) {
						int h,m,s; char f[7];
						if (sscanf(i->at(16).c_str(), "%d:%d:%d:%s",&h,&m,&s,f) == 4) {
							m_duration = (ARDOUR::framecnt_t) floor(m_fps * (
									h * 3600.0
								+ m * 60.0
								+ s * 1.0
								+ atoi(f) / pow(10, strlen(f))
							));
						}
					} else if (key == X_("duration_ts") && m_fps == 0 && timebase !=0 ) {
						m_duration = atof(value.c_str()) * m_fps * timebase;
					} else if (key == X_("duration") && m_fps != 0 && m_duration == 0) {
						m_duration = atof(value.c_str()) * m_fps;
					} else if (key == X_("display_aspect_ratio")) {
						std::string::size_type pos;
						pos = value.find_first_of(':');
						if (pos != std::string::npos && atof(value.substr(pos+1).c_str()) != 0) {
							m_aspect = atof(value.substr(0, pos).c_str()) / atof(value.substr(pos+1).c_str());
						}
					}
				}

				if (m_aspect == 0) {
					m_aspect = (double)m_width / (double)m_height;
				}

			} else if (i->at(5) == X_("codec_type=audio")) { /* new ffprobe */
				AudioStream as;
				for (std::vector<std::string>::iterator kv = i->begin(); kv != i->end(); ++kv) {
					const size_t kvsep = kv->find('=');
					if(kvsep == std::string::npos) continue;
					std::string key = kv->substr(0, kvsep);
					std::string value = kv->substr(kvsep + 1);

					if (key == X_("channels")) {
						as.channels   = atoi(value.c_str());
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


	int timeout = 500;
	while (ffcmd && --timeout) usleep (1000); // wait until 'ffprobe' terminated.
	if (timeout == 0) return false;

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

FFSettings
TranscodeFfmpeg::default_encoder_settings ()
{
	FFSettings ffs;
	ffs.clear();
	ffs["-vcodec"] = "mpeg4";
	ffs["-acodec"] = "ac3";
	ffs["-b:v"] = "5000k";
	ffs["-b:a"] = "160k";
	return ffs;
}

FFSettings
TranscodeFfmpeg::default_meta_data ()
{
	FFSettings ffm;
	ffm.clear();
	ffm["comment"] = "Created with ardour";
	return ffm;
}

char *
TranscodeFfmpeg::format_metadata (std::string key, std::string value)
{
	size_t start_pos = 0;
	std::string v1 = value;
	while((start_pos = v1.find_first_not_of(
			"abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789(),.\"'",
			start_pos)) != std::string::npos)
	{
		v1.replace(start_pos, 1, "_");
		start_pos += 1;
	}

	start_pos = 0;
	while((start_pos = v1.find("\"", start_pos)) != std::string::npos) {
		v1.replace(start_pos, 1, "\\\"");
		start_pos += 2;
	}

	size_t len = key.length() + v1.length() + 4;
	char *mds = (char*) calloc(len, sizeof(char));
	snprintf(mds, len, "%s=\"%s\"", key.c_str(), v1.c_str());
	return mds;
}

bool
TranscodeFfmpeg::encode (std::string outfile, std::string inf_a, std::string inf_v, FFSettings ffs, FFSettings meta, bool map)
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

	for(FFSettings::const_iterator it = ffs.begin(); it != ffs.end(); ++it) {
		argp[a++] = strdup(it->first.c_str());
		argp[a++] = strdup(it->second.c_str());
	}
	for(FFSettings::const_iterator it = meta.begin(); it != meta.end(); ++it) {
		argp[a++] = strdup("-metadata");
		argp[a++] = format_metadata(it->first.c_str(), it->second.c_str());
	}
	if (m_lead_in != 0 && m_lead_out != 0) {
		std::ostringstream osstream;
		argp[a++] = strdup("-vf");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_in << X_(" [pre]; ");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_out << X_(" [post]; ");
		osstream << X_("[pre] [in] [post] concat=n=3");
		argp[a++] = strdup(osstream.str().c_str());
	} else if (m_lead_in != 0) {
		std::ostringstream osstream;
		argp[a++] = strdup("-vf");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_in << X_(" [pre]; ");
		osstream << X_("[pre] [in] concat=n=2");
		argp[a++] = strdup(osstream.str().c_str());
	} else if (m_lead_out != 0) {
		std::ostringstream osstream;
		argp[a++] = strdup("-vf");
		osstream << X_("color=c=black:s=") << m_width << X_("x") << m_height << X_(":d=") << m_lead_out << X_(" [post]; ");
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

	ffcmd = new SystemExec(ffmpeg_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffmpegparse_v, this, _1 ,_2));
	ffcmd->Terminated.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffexit, this));
	if (ffcmd->start(2)) {
		ffexit();
		return false;
	}
	return true;
}

bool
TranscodeFfmpeg::extract_audio (std::string outfile, ARDOUR::framecnt_t samplerate, unsigned int stream)
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

	ffcmd = new SystemExec(ffmpeg_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffmpegparse_a, this, _1 ,_2));
	ffcmd->Terminated.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffexit, this));
	if (ffcmd->start(2)) {
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
	argp[9] = strdup("mpeg4");
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
	ffcmd = new SystemExec(ffmpeg_exe, argp);
	ffcmd->ReadStdout.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffmpegparse_v, this, _1 ,_2));
	ffcmd->Terminated.connect_same_thread (*this, boost::bind (&TranscodeFfmpeg::ffexit, this));
	if (ffcmd->start(2)) {
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
	sleep (1);
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
	ARDOUR::framecnt_t p = -1;

	if (!(t=strstr(d.c_str(), "time="))) { return; }

	if (sscanf(t+5, "%d:%d:%d.%s",&h,&m,&s,f) == 4) {
		p = (ARDOUR::framecnt_t) floor( 100.0 * (
		      h * 3600.0
		    + m * 60.0
		    + s * 1.0
		    + atoi(f) / pow(10, strlen(f))
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
		PBD::warning << "ffmpeg-error: " << d << endmsg;
	}
	if (strncmp(d.c_str(), "frame=",6)) {
#if 1 /* DEBUG */
		if (debug_enable) {
			d.erase(d.find_last_not_of(" \t\r\n") + 1);
		  printf("ffmpeg: '%s'\n", d.c_str());
		}
#endif
		Progress(0, 0); /* EMIT SIGNAL */
		return;
	}
	ARDOUR::framecnt_t f = atol(d.substr(6).c_str());
	if (f == 0) {
		Progress(0, 0); /* EMIT SIGNAL */
	} else {
		Progress(f, m_duration); /* EMIT SIGNAL */
	}
}

#endif /* WITH_VIDEOTIMELINE */
