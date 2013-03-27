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
	argp[1] = strdup("-print_format"); // "-of"  ; new version and avprobe compat but avprobe does not yet support csv
	argp[2] = strdup("csv"); // TODO use "csv=nk=0" and parse key/value pairs -> ffprobe version agnostic or parse XML or JSON key/value
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
	m_width = m_height = 0;
	m_fps = m_aspect = 0;
	m_duration = 0;
	m_codec.clear();
	m_audio.clear();

	for (std::vector<std::vector<std::string> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		if (i->at(0) == X_("format")) {
			/* format,filename,#streams,format-name,format-long-name,start-time,duration,size,bitrate */
		} else
		if (i->at(0) == X_("stream")) {
			/*--------- Stream format
			 * stream,index,codec-name,codec-name-long,PROFILE,
			 *   codec_time_base,codec_tag_string,codec_tag[hex],
			 * VIDEO:
			 *   width,height,has_b_frames,sample_aspect_ratio,display_aspect_ratio
			 *   pix_fmt,level,
			 *   timecode
			 * AUDIO:
			 *   sample_fmt,sample_rate,channels,bits_per_sample
			 *
			 * all cont'd;
			 *   r_frame_rate,avg_frame_rate,time_base,start_time,duration,
			 *   bit_rate,nb_frames,nb_read_frames,nb_read_packets
			 *
			 *---------- Example
			 * stream,0,mpeg2video,MPEG-2 video,video,1/50,[0][0][0][0],0x0000,720,576,1,16:15,4:3,yuv420p,8,00:02:30:00,0x1e0,25/1,25/1,1/90000,0.360000,N/A,7000000,N/A,N/A,N/A
			 * stream,1,ac3,ATSC A/52A (AC-3),audio,1/48000,[0][0][0][0],0x0000,s16,48000,6,0,-1,-1.000000,-1.000000,-1.000000,-1.000000,0x80,0/0,0/0,1/90000,0.280000,312.992000,448000,N/A,N/A,N/A
			 * stream,2,ac3,ATSC A/52A (AC-3),audio,1/48000,[0][0][0][0],0x0000,s16,48000,2,0,-1,-1.000000,-1.000000,-1.000000,-1.000000,0x82,0/0,0/0,1/90000,0.280000,312.992000,384000,N/A,N/A,N/A
			 * stream,3,ac3,ATSC A/52A (AC-3),audio,1/48000,[0][0][0][0],0x0000,s16,48000,2,0,-1,-1.000000,-1.000000,-1.000000,-1.000000,0x81,0/0,0/0,1/90000,0.280000,312.992000,192000,N/A,N/A,N/A
			 */
			if (i->at(4) == X_("video") && m_width == 0) {
				std::string::size_type pos;

				m_width = atoi(i->at(8).c_str());
				m_height = atoi(i->at(9).c_str());
				m_codec = i->at(3) + " -- " + i->at(2);
				m_fps = atof(i->at(17).c_str());

				pos = i->at(17).find_first_of('/');
				if (pos != std::string::npos) {
					m_fps = atof(i->at(17).substr(0, pos).c_str()) / atof(i->at(17).substr(pos+1).c_str());
				}

				pos = i->at(12).find_first_of(':');
				m_aspect = 0;
				if (pos != std::string::npos && atof(i->at(12).substr(pos+1).c_str()) != 0) {
					m_aspect = atof(i->at(12).substr(0, pos).c_str()) / atof(i->at(12).substr(pos+1).c_str());
				}
				if (m_aspect == 0) {
					m_aspect = (double)m_width / (double)m_height;
				}

				int h,m,s; char f[7];
				if (sscanf(i->at(15).c_str(), "%d:%d:%d:%s",&h,&m,&s,f) == 4) {
					m_duration = (ARDOUR::framecnt_t) floor(m_fps * (
							h * 3600.0
						+ m * 60.0
						+ s * 1.0
						+ atoi(f) / pow(10, strlen(f))
					));
				} else {
					m_duration = atof(i->at(21).c_str()) * m_fps;
				}

			} else if (i->at(4) == X_("audio")) {
				AudioStream as;
				as.name = i->at(3) + " " + i->at(2) + " " + i->at(8) + " " + i->at(9);
				as.stream_id  = i->at(1);
				as.channels   = atoi(i->at(10).c_str());
				m_audio.push_back(as);

			} else if (i->at(5) == X_("video") && m_width == 0) { /* new ffprobe */
				std::string::size_type pos;

				m_width = atoi(i->at(9).c_str());
				m_height = atoi(i->at(10).c_str());
				m_codec = i->at(3) + " -- " + i->at(2);
				m_fps = atof(i->at(18).c_str());

				pos = i->at(18).find_first_of('/');
				if (pos != std::string::npos) {
					m_fps = atof(i->at(18).substr(0, pos).c_str()) / atof(i->at(18).substr(pos+1).c_str());
				}

				pos = i->at(13).find_first_of(':');
				m_aspect = 0;
				if (pos != std::string::npos && atof(i->at(13).substr(pos+1).c_str()) != 0) {
					m_aspect = atof(i->at(13).substr(0, pos).c_str()) / atof(i->at(13).substr(pos+1).c_str());
				}
				if (m_aspect == 0) {
					m_aspect = (double)m_width / (double)m_height;
				}

				int h,m,s; char f[7];
				if (sscanf(i->at(17).c_str(), "%d:%d:%d:%s",&h,&m,&s,f) == 4) {
					m_duration = (ARDOUR::framecnt_t) floor(m_fps * (
							h * 3600.0
						+ m * 60.0
						+ s * 1.0
						+ atoi(f) / pow(10, strlen(f))
					));
				} else if (atof(i->at(23).c_str()) != 0) {
					m_duration = atof(i->at(23).c_str());
				} else {
					m_duration = atof(i->at(24).c_str()) * m_fps;
				}

			} else if (i->at(5) == X_("audio")) { /* new ffprobe */
				AudioStream as;
				as.name = i->at(3) + " " + i->at(2) + " " + i->at(9) + " " + i->at(10);
				as.stream_id  = i->at(1);
				as.channels   = atoi(i->at(11).c_str());
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
	ffs["-b"] = "5000k";
	ffs["-ab"] = "160k";
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
	if (map) {
		argp[a++] = strdup("-map");
		argp[a++] = strdup("0:0");
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

	argp=(char**) calloc(15,sizeof(char*));
	argp[0] = strdup(ffmpeg_exe.c_str());
	argp[1] = strdup("-i");
	argp[2] = strdup(infile.c_str());
	argp[3] = strdup("-ar");
	argp[4] = (char*) calloc(7,sizeof(char)); snprintf(argp[4], 7, "%"PRId64, samplerate);
	argp[5] = strdup("-ac");
	argp[6] = (char*) calloc(3,sizeof(char)); snprintf(argp[6], 3, "%i", m_audio.at(stream).channels);
	argp[7] = strdup("-map");
	argp[8] = (char*) calloc(8,sizeof(char)); snprintf(argp[8], 8, "0:%s", m_audio.at(stream).stream_id.c_str());
	argp[9] = strdup("-vn");
	argp[10] = strdup("-acodec");
	argp[11] = strdup("pcm_f32le");
	argp[12] = strdup("-y");
	argp[13] = strdup(outfile.c_str());
	argp[14] = (char *)0;
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
	argp[3] = strdup("-b");
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
	if (!(t=strstr(d.c_str(), "time="))) { return; }
	ARDOUR::framecnt_t f = (ARDOUR::framecnt_t) floorf (atof(t+5) * m_fps);
	if (f > m_duration ) { f = m_duration; }
	Progress(f, m_duration); /* EMIT SIGNAL */
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
		return;
	}
	ARDOUR::framecnt_t f = atol(d.substr(6).c_str());
	Progress(f, m_duration); /* EMIT SIGNAL */
}

#endif /* WITH_VIDEOTIMELINE */
