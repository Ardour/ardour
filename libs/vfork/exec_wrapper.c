/*
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

extern char **environ;
static void close_fd (int *fd) { if ((*fd) >= 0) close (*fd); *fd = -1; }

int main(int argc, char *argv[]) {
	if (argc < 10) {
		// TODO: if argv > 3, assume pok[] is given, notifify parent.
		// usage() and a man-page (help2man) would not be bad, either :)
		return -1;
	}

	int pok[2];
	int pin[2];
	int pout[2];

	pok[0]  = atoi(argv[1]);
	pok[1]  = atoi(argv[2]);
	pin[0]  = atoi(argv[3]);
	pin[1]  = atoi(argv[4]);
	pout[0] = atoi(argv[5]);
	pout[1] = atoi(argv[6]);

	int stderr_mode = atoi(argv[7]);
	int nicelevel = atoi(argv[8]);

	/* vfork()ed child process - exec external process */
	close_fd(&pok[0]);
	fcntl(pok[1], F_SETFD, FD_CLOEXEC);

	close_fd(&pin[1]);
	if (pin[0] != STDIN_FILENO) {
		dup2(pin[0], STDIN_FILENO);
	}
	close_fd(&pin[0]);
	close_fd(&pout[0]);
	if (pout[1] != STDOUT_FILENO) {
		dup2(pout[1], STDOUT_FILENO);
	}

	if (stderr_mode == 2) {
		/* merge STDERR into output */
		if (pout[1] != STDERR_FILENO) {
			dup2(pout[1], STDERR_FILENO);
		}
	} else if (stderr_mode == 1) {
		/* ignore STDERR */
		close(STDERR_FILENO);
	} else {
		/* keep STDERR */
	}

	if (pout[1] != STDOUT_FILENO && pout[1] != STDERR_FILENO) {
		close_fd(&pout[1]);
	}

	if (nicelevel !=0) {
		nice(nicelevel);
	}

	/* copy current environment */
	char **envp = NULL;
	int i=0;
	envp = (char **) calloc(1, sizeof(char*));
	for (i=0;environ[i];++i) {
		envp[i] = strdup(environ[i]);
		envp = (char **) realloc(envp, (i+2) * sizeof(char*));
	}
	envp[i] = 0;

#ifdef HAVE_SIGSET
	sigset(SIGPIPE, SIG_DFL);
#else
	signal(SIGPIPE, SIG_DFL);
#endif

	/* all systems go */
	execve(argv[9], &argv[9], envp);

	/* if we reach here something went wrong.. */
	char buf = 0;
	(void) write(pok[1], &buf, 1 );
	close_fd(&pok[1]);

#ifdef __clang_analyzer__
	// the clang static analyzer warns about a memleak here,
	// but we don't care. The OS will clean up after us in a jiffy.
	for (i=0; envp && envp[i]; ++i) {
		free (envp[i]);
	}
	free (envp);
#endif
	return -1;
}
