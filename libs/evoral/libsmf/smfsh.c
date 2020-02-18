/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * ALTHOUGH THIS SOFTWARE IS MADE OF WIN AND SCIENCE, IT IS PROVIDED BY THE
 * AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * \file
 *
 * "SMF shell", command line utility.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "smf.h"
#include "config.h"

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

smf_track_t *selected_track = NULL;
smf_event_t *selected_event = NULL;
smf_t *smf = NULL;
char *last_file_name = NULL;

#define COMMAND_LENGTH 10

static void
log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer notused)
{
	if (strcmp(log_domain, "smfsh") == 0)
		fprintf(stderr, "%s\n", message);
	else
		fprintf(stderr, "%s: %s\n", log_domain, message);
}

static int cmd_track(char *arg);

static int
cmd_load(char *file_name)
{
	char *decoded;

	if (file_name == NULL) {
		if (last_file_name == NULL) {
			g_critical("Please specify file name.");
			return (-1);
		}

		file_name = strdup(last_file_name);
	} else {
		file_name = strdup(file_name);
	}

	selected_track = NULL;
	selected_event = NULL;

	if (smf != NULL) {
		smf_delete(smf);
		smf = NULL;
	}

	if (last_file_name != NULL)
		free(last_file_name);
	last_file_name = strdup(file_name);

	smf = smf_load(file_name);
	if (smf == NULL) {
		g_critical("Couldn't load '%s'.", file_name);

		smf = smf_new();
		if (smf == NULL) {
			g_critical("Cannot initialize smf_t.");
			return (-1);
		}

		return (-2);
	}

	g_message("File '%s' loaded.", file_name);
	decoded = smf_decode(smf);
	g_message("%s.", decoded);
	free(decoded);

	cmd_track("1");

	free(file_name);

	return (0);
}

static int
cmd_save(char *file_name)
{
	int ret;

	if (file_name == NULL) {
		if (last_file_name == NULL) {
			g_critical("Please specify file name.");
			return (-1);
		}

		file_name = strdup(last_file_name);
	} else {
		file_name = strdup(file_name);
	}

	if (last_file_name != NULL)
		free(last_file_name);
	last_file_name = strdup(file_name);

	ret = smf_save(smf, file_name);
	if (ret) {
		g_critical("Couldn't save '%s'", file_name);
		return (-1);
	}

	g_message("File '%s' saved.", file_name);

	free(file_name);

	return (0);
}

static int
cmd_ppqn(char *new_ppqn)
{
	int tmp;
	char *end;

	if (new_ppqn == NULL) {
		g_message("Pulses Per Quarter Note (aka Division) is %d.", smf->ppqn);
	} else {
		tmp = strtol(new_ppqn, &end, 10);
		if (end - new_ppqn != strlen(new_ppqn)) {
			g_critical("Invalid PPQN, garbage characters after the number.");
			return (-1);
		}

		if (tmp <= 0) {
			g_critical("Invalid PPQN, valid values are greater than zero.");
			return (-2);
		}

		if (smf_set_ppqn(smf, tmp)) {
			g_message("smf_set_ppqn failed.");
			return (-3);
		}

		g_message("Pulses Per Quarter Note changed to %d.", smf->ppqn);
	}

	return (0);
}

static int
cmd_format(char *new_format)
{
	int tmp;
	char *end;

	if (new_format == NULL) {
		g_message("Format is %d.", smf->format);
	} else {
		tmp = strtol(new_format, &end, 10);
		if (end - new_format != strlen(new_format)) {
			g_critical("Invalid format value, garbage characters after the number.");
			return (-1);
		}

		if (tmp < 0 || tmp > 2) {
			g_critical("Invalid format value, valid values are in range 0 - 2, inclusive.");
			return (-2);
		}

		if (smf_set_format(smf, tmp)) {
			g_critical("smf_set_format failed.");
			return (-3);
		}

		g_message("Forma changed to %d.", smf->format);
	}

	return (0);
}

static int
cmd_tracks(char *notused)
{
	if (smf->number_of_tracks > 0)
		g_message("There are %d tracks, numbered from 1 to %d.", smf->number_of_tracks, smf->number_of_tracks);
	else
		g_message("There are no tracks.");

	return (0);
}

static int
parse_track_number(const char *arg)
{
	int num;
	char *end;

	if (arg == NULL) {
		if (selected_track == NULL) {
			g_message("No track currently selected and no track number given.");
			return (-1);
		} else {
			return (selected_track->track_number);
		}
	}

	num = strtol(arg, &end, 10);
	if (end - arg != strlen(arg)) {
		g_critical("Invalid track number, garbage characters after the number.");
		return (-1);
	}

	if (num < 1 || num > smf->number_of_tracks) {
		if (smf->number_of_tracks > 0) {
			g_critical("Invalid track number specified; valid choices are 1 - %d.", smf->number_of_tracks);
		} else {
			g_critical("There are no tracks.");
		}

		return (-1);
	}

	return (num);
}

static int
cmd_track(char *arg)
{
	int num;

	if (arg == NULL) {
		if (selected_track == NULL)
			g_message("No track currently selected.");
		else
			g_message("Currently selected is track number %d, containing %d events.",
				selected_track->track_number, selected_track->number_of_events);
	} else {
		if (smf->number_of_tracks == 0) {
			g_message("There are no tracks.");
			return (-1);
		}

		num = parse_track_number(arg);
		if (num < 0)
			return (-1);

		selected_track = smf_get_track_by_number(smf, num);
		if (selected_track == NULL) {
			g_critical("smf_get_track_by_number() failed, track not selected.");
			return (-3);
		}

		selected_event = NULL;

		g_message("Track number %d selected; it contains %d events.",
				selected_track->track_number, selected_track->number_of_events);
	}

	return (0);
}

static int
cmd_trackadd(char *notused)
{
	selected_track = smf_track_new();
	if (selected_track == NULL) {
		g_critical("smf_track_new() failed, track not created.");
		return (-1);
	}

	smf_add_track(smf, selected_track);

	selected_event = NULL;

	g_message("Created new track; track number %d selected.", selected_track->track_number);

	return (0);
}

static int
cmd_trackrm(char *arg)
{
	int num = parse_track_number(arg);

	if (num < 0)
		return (-1);

	if (selected_track != NULL && num == selected_track->track_number) {
		selected_track = NULL;
		selected_event = NULL;
	}

	smf_track_delete(smf_get_track_by_number(smf, num));

	g_message("Track %d removed.", num);

	return (0);
}

#define BUFFER_SIZE 1024

static int
show_event(smf_event_t *event)
{
	int off = 0, i;
	char *decoded, *type;

	if (smf_event_is_metadata(event))
		type = "Metadata";
	else
		type = "Event";

	decoded = smf_event_decode(event);

	if (decoded == NULL) {
		decoded = malloc(BUFFER_SIZE);
		if (decoded == NULL) {
			g_critical("show_event: malloc failed.");
			return (-1);
		}

		off += snprintf(decoded + off, BUFFER_SIZE - off, "Unknown event:");

		for (i = 0; i < event->midi_buffer_length && i < 5; i++)
			off += snprintf(decoded + off, BUFFER_SIZE - off, " 0x%x", event->midi_buffer[i]);
	}

	g_message("%d: %s: %s, %f seconds, %d pulses, %d delta pulses", event->event_number, type, decoded,
	    event->time_seconds, event->time_pulses, event->delta_time_pulses);

	free(decoded);

	return (0);
}

static int
cmd_events(char *notused)
{
	smf_event_t *event;

	if (selected_track == NULL) {
		g_critical("No track selected - please use 'track <number>' command first.");
		return (-1);
	}

	if (selected_track->number_of_events == 0) {
		g_message("Selected track is empty.");
		return (0);
	}

	g_message("List of events in track %d follows:", selected_track->track_number);

	smf_rewind(smf);

	while ((event = smf_track_get_next_event(selected_track)) != NULL)
		show_event(event);

	smf_rewind(smf);

	return (0);
}

static int
parse_event_number(const char *arg)
{
	int num;
	char *end;

	if (selected_track == NULL) {
		g_critical("You need to select track first (using 'track <number>').");
		return (-1);
	}

	if (arg == NULL) {
		if (selected_event == NULL) {
			g_message("No event currently selected and no event number given.");
			return (-1);
		} else {
			return (selected_event->event_number);
		}
	}

	num = strtol(arg, &end, 10);
	if (end - arg != strlen(arg)) {
		g_critical("Invalid event number, garbage characters after the number.");
		return (-1);
	}

	if (num < 1 || num > selected_track->number_of_events) {
		if (selected_track->number_of_events > 0)
			g_critical("Invalid event number specified; valid choices are 1 - %d.", selected_track->number_of_events);
		else
			g_critical("There are no events in currently selected track.");

		return (-1);
	}

	return (num);
}

static int
cmd_event(char *arg)
{
	int num;

	if (arg == NULL) {
		if (selected_event == NULL) {
			g_message("No event currently selected.");
		} else {
			g_message("Currently selected is event %d, track %d.", selected_event->event_number, selected_track->track_number);
			show_event(selected_event);
		}
	} else {
		num = parse_event_number(arg);
		if (num < 0)
			return (-1);

		selected_event = smf_track_get_event_by_number(selected_track, num);
		if (selected_event == NULL) {
			g_critical("smf_get_event_by_number() failed, event not selected.");
			return (-2);
		}

		g_message("Event number %d selected.", selected_event->event_number);
		show_event(selected_event);
	}

	return (0);
}

static int
decode_hex(char *str, unsigned char **buffer, int *length)
{
	int i, value, midi_buffer_length;
	char buf[3];
	unsigned char *midi_buffer = NULL;
	char *end = NULL;

	if ((strlen(str) % 2) != 0) {
		g_critical("Hex value should have even number of characters, you know.");
		goto error;
	}

	midi_buffer_length = strlen(str) / 2;
	midi_buffer = malloc(midi_buffer_length);
	if (midi_buffer == NULL) {
		g_critical("malloc() failed.");
		goto error;
	}

	for (i = 0; i < midi_buffer_length; i++) {
		buf[0] = str[i * 2];
		buf[1] = str[i * 2 + 1];
		buf[2] = '\0';
		value = strtoll(buf, &end, 16);

		if (end - buf != 2) {
			g_critical("Garbage characters detected after hex.");
			goto error;
		}

		midi_buffer[i] = value;
	}

	*buffer = midi_buffer;
	*length = midi_buffer_length;

	return (0);

error:
	if (midi_buffer != NULL)
		free(midi_buffer);

	return (-1);
}

static void
eventadd_usage(void)
{
	g_message("Usage: add <time-in-seconds> <midi-in-hex> - for example, 'add 1 903C7F' will add");
	g_message("Note On event, note C4, velocity 127, channel 1, one second from the start of song, channel 1.");
}

static int
cmd_eventadd(char *str)
{
	int midi_buffer_length;
	double seconds;
	unsigned char *midi_buffer;
	char *time, *endtime;

	if (selected_track == NULL) {
		g_critical("Please select a track first, using 'track <number>' command.");
		return (-1);
	}

	if (str == NULL) {
		eventadd_usage();
		return (-2);
	}

	/* Extract the time.  Don't use strsep(3), it doesn't work on SunOS. */
	time = str;
	str = strchr(str, ' ');
	if (str != NULL) {
		*str = '\0';
		str++;
	}

	seconds = strtod(time, &endtime);
	if (endtime - time != strlen(time)) {
		g_critical("Time is supposed to be a number, without trailing characters.");
		return (-3);
	}

	/* Called with one parameter? */
	if (str == NULL) {
		eventadd_usage();
		return (-4);
	}

	if (decode_hex(str, &midi_buffer, &midi_buffer_length)) {
		eventadd_usage();
		return (-5);
	}

	selected_event = smf_event_new();
	if (selected_event == NULL) {
		g_critical("smf_event_new() failed, event not created.");
		return (-6);
	}

	selected_event->midi_buffer = midi_buffer;
	selected_event->midi_buffer_length = midi_buffer_length;

	if (smf_event_is_valid(selected_event) == 0) {
		g_critical("Event is invalid from the MIDI specification point of view, not created.");
		smf_event_delete(selected_event);
		selected_event = NULL;
		return (-7);
	}

	smf_track_add_event_seconds(selected_track, selected_event, seconds);

	g_message("Event created.");

	return (0);
}

static int
cmd_text(char *str)
{
	double seconds, type;
	char *time, *typestr, *end;

	if (selected_track == NULL) {
		g_critical("Please select a track first, using 'track <number>' command.");
		return (-1);
	}

	if (str == NULL) {
		g_critical("Usage: text <time-in-seconds> <event-type> <text-itself>");
		return (-2);
	}

	/* Extract the time.  Don't use strsep(3), it doesn't work on SunOS. */
	time = str;
	str = strchr(str, ' ');
	if (str != NULL) {
		*str = '\0';
		str++;
	}

	seconds = strtod(time, &end);
	if (end - time != strlen(time)) {
		g_critical("Time is supposed to be a number, without trailing characters.");
		return (-3);
	}

	/* Called with one parameter? */
	if (str == NULL) {
		g_critical("Usage: text <time-in-seconds> <event-type> <text-itself>");
		return (-4);
	}

	/* Extract the event type. */
	typestr = str;
	str = strchr(str, ' ');
	if (str != NULL) {
		*str = '\0';
		str++;
	}

	type = strtod(typestr, &end);
	if (end - typestr != strlen(typestr)) {
		g_critical("Type is supposed to be a number, without trailing characters.");
		return (-4);
	}

	if (type < 1 || type > 9) {
		g_critical("Valid values for type are 1 - 9, inclusive.");
		return (-5);
	}

	/* Called with one parameter? */
	if (str == NULL) {
		g_critical("Usage: text <time-in-seconds> <event-type> <text-itself>");
		return (-4);
	}

	selected_event = smf_event_new_textual(type, str);
	if (selected_event == NULL) {
		g_critical("smf_event_new_textual() failed, event not created.");
		return (-6);
	}

	assert(smf_event_is_valid(selected_event));

	smf_track_add_event_seconds(selected_track, selected_event, seconds);

	g_message("Event created.");

	return (0);
}


static int
cmd_eventaddeot(char *time)
{
	double seconds;
	char *end;

	if (selected_track == NULL) {
		g_critical("Please select a track first, using 'track <number>' command.");
		return (-1);
	}

	if (time == NULL) {
		g_critical("Please specify the time, in seconds.");
		return (-2);
	}

	seconds = strtod(time, &end);
	if (end - time != strlen(time)) {
		g_critical("Time is supposed to be a number, without trailing characters.");
		return (-3);
	}

	if (smf_track_add_eot_seconds(selected_track, seconds)) {
		g_critical("smf_track_add_eot() failed.");
		return (-4);
	}

	g_message("Event created.");

	return (0);
}

static int
cmd_eventrm(char *number)
{
	int num = parse_event_number(number);

	if (num < 0)
		return (-1);

	if (selected_event != NULL && num == selected_event->event_number)
		selected_event = NULL;

	smf_event_delete(smf_track_get_event_by_number(selected_track, num));

	g_message("Event #%d removed.", num);

	return (0);
}

static int
cmd_tempo(char *notused)
{
	int i;
	smf_tempo_t *tempo;

	for (i = 0;; i++) {
		tempo = smf_get_tempo_by_number(smf, i);
		if (tempo == NULL)
			break;

		g_message("Tempo #%d: Starts at %d pulses, %f seconds, setting %d microseconds per quarter note, %.2f BPM.",
		    i, tempo->time_pulses, tempo->time_seconds, tempo->microseconds_per_quarter_note,
		    60000000.0 / (double)tempo->microseconds_per_quarter_note);
		g_message("Time signature: %d/%d, %d clocks per click, %d 32nd notes per quarter note.",
		    tempo->numerator, tempo->denominator, tempo->clocks_per_click, tempo->notes_per_note);
	}

	return (0);
}

static int
cmd_length(char *notused)
{
	g_message("Length: %d pulses, %f seconds.", smf_get_length_pulses(smf), smf_get_length_seconds(smf));

	return (0);
}

static int
cmd_version(char *notused)
{
	g_message("libsmf version %s.", smf_get_version());

	return (0);
}

static int
cmd_exit(char *notused)
{
	g_debug("Good bye.");
	exit(0);
}

static int cmd_help(char *notused);

static struct command_struct {
	char *name;
	int (*function)(char *command);
	char *help;
} commands[] = {{"help", cmd_help, "Show this help."},
		{"?", cmd_help, NULL},
		{"load", cmd_load, "Load named file."},
		{"open", cmd_load},
		{"save", cmd_save, "Save to named file."},
		{"ppqn", cmd_ppqn, "Show ppqn (aka division), or set ppqn if used with parameter."},
		{"format", cmd_format, "Show format, or set format if used with parameter."},
		{"tracks", cmd_tracks, "Show number of tracks."},
		{"track", cmd_track, "Show number of currently selected track, or select a track."},
		{"trackadd", cmd_trackadd, "Add a track and select it."},
		{"trackrm", cmd_trackrm, "Remove currently selected track."},
		{"events", cmd_events, "Show events in the currently selected track."},
		{"event", cmd_event, "Show number of currently selected event, or select an event."},
		{"add", cmd_eventadd, "Add an event and select it."},
		{"text", cmd_text, "Add textual event and select it."},
		{"eventadd", cmd_eventadd, NULL},
		{"eot", cmd_eventaddeot, "Add an End Of Track event."},
		{"eventaddeot", cmd_eventaddeot, NULL},
		{"eventrm", cmd_eventrm, NULL},
		{"rm", cmd_eventrm, "Remove currently selected event."},
		{"tempo", cmd_tempo, "Show tempo map."},
		{"length", cmd_length, "Show length of the song."},
		{"version", cmd_version, "Show libsmf version."},
		{"exit", cmd_exit, "Exit to shell."},
		{"quit", cmd_exit, NULL},
		{"bye", cmd_exit, NULL},
		{NULL, NULL, NULL}};

static int
cmd_help(char *notused)
{
	int i, padding_length;
	char padding[COMMAND_LENGTH + 1];
	struct command_struct *tmp;

	g_message("Available commands:");

	for (tmp = commands; tmp->name != NULL; tmp++) {
		/* Skip commands with no help string. */
		if (tmp->help == NULL)
			continue;

		padding_length = COMMAND_LENGTH - strlen(tmp->name);
		assert(padding_length >= 0);
		for (i = 0; i < padding_length; i++)
			padding[i] = ' ';
		padding[i] = '\0';

		g_message("%s:%s%s", tmp->name, padding, tmp->help);
	}

	return (0);
}

/**
 * Removes (in place) all whitespace characters before the first
 * non-whitespace and all trailing whitespace characters.  Replaces
 * more than one consecutive whitespace characters with one.
 */
static void
strip_unneeded_whitespace(char *str, int len)
{
	char *src, *dest;
	int skip_white = 1;

	for (src = str, dest = str; src < dest + len; src++) {
		if (*src == '\n' || *src == '\0') {
			*dest = '\0';
			break;
		}

		if (isspace(*src)) {
			if (skip_white)
				continue;

			skip_white = 1;
		} else {
			skip_white = 0;
		}

		*dest = *src;
		dest++;
	}

	/* Remove trailing whitespace. */
	len = strlen(dest);
	if (isspace(dest[len - 1]))
		dest[len - 1] = '\0';
}

static char *
read_command(void)
{
	char *buf;
	int len;

#ifdef HAVE_LIBREADLINE
	buf = readline("smfsh> ");
#else
	buf = malloc(1024);
	if (buf == NULL) {
		g_critical("Malloc failed.");
		return (NULL);
	}

	fprintf(stdout, "smfsh> ");
	fflush(stdout);

	buf = fgets(buf, 1024, stdin);
#endif

	if (buf == NULL) {
		fprintf(stdout, "exit\n");
		return (strdup("exit"));
	}

	strip_unneeded_whitespace(buf, 1024);

	len = strlen(buf);

	if (len == 0)
		return (read_command());

#ifdef HAVE_LIBREADLINE
	add_history(buf);
#endif

	return (buf);
}

static int
execute_command(char *line)
{
	char *command, *args;
	struct command_struct *tmp;

	command = line;
	args = strchr(line, ' ');
	if (args != NULL) {
		*args = '\0';
		args++;
	}

	for (tmp = commands; tmp->name != NULL; tmp++) {
		if (strcmp(tmp->name, command) == 0)
			return ((tmp->function)(args));
	}

	g_warning("No such command: '%s'.  Type 'help' to see available commands.", command);

	return (-1);
}

static void
read_and_execute_command(void)
{
	int ret;
	char *command_line, *command, *next_command;

	command = command_line = read_command();

	do {
		next_command = strchr(command, ';');
		if (next_command != NULL) {
			*next_command = '\0';
			next_command++;
		}

		strip_unneeded_whitespace(command, 1024);
		if (strlen(command) > 0) {
			ret = execute_command(command);
			if (ret)
				g_warning("Command finished with error.");
		}

		command = next_command;

	} while (command);

	free(command_line);
}

#ifdef HAVE_LIBREADLINE

static char *
smfsh_command_generator(const char *text, int state)
{
	static struct command_struct *command = commands;
	char *tmp;

	if (state == 0)
		command = commands;

	while (command->name != NULL) {
		tmp = command->name;
		command++;

		if (strncmp(tmp, text, strlen(text)) == 0)
			return (strdup(tmp));
	}

	return (NULL);
}

static char **
smfsh_completion(const char *text, int start, int end)
{
	int i;

	/* Return NULL if "text" is not the first word in the input line. */
	if (start != 0) {
		for (i = 0; i < start; i++) {
			if (!isspace(rl_line_buffer[i]))
				return (NULL);
		}
	}

	return (rl_completion_matches(text, smfsh_command_generator));
}

#endif

static void
usage(void)
{
	fprintf(stderr, "usage: smfsh [-V | file]\n");

	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "V")) != -1) {
		switch (ch) {
		case 'V':
			cmd_version(NULL);
			exit(EX_OK);

		case '?':
		default:
			usage();
		}
	}

	if (argc > 2)
		usage();

	g_log_set_default_handler(log_handler, NULL);

	smf = smf_new();
	if (smf == NULL) {
		g_critical("Cannot initialize smf_t.");
		return (-1);
	}

	if (argc == 2)
		cmd_load(argv[1]);
	else
		cmd_trackadd(NULL);

#ifdef HAVE_LIBREADLINE
	rl_readline_name = "smfsh";
	rl_attempted_completion_function = smfsh_completion;
#endif

	for (;;)
		read_and_execute_command();

	return (0);
}

