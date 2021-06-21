/*
 * Copyright (C) 2005-2008 Lennart Poettering
 * Copyright (C) 2010-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <algorithm>

#include <assert.h>

#ifndef COMPILER_MSVC
#include <dirent.h>
#endif

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <glibmm/miscutils.h>

#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/pthread_utils.h"
#include "pbd/system_exec.h"

using namespace std;
using namespace PBD;

static void * interposer_thread (void *arg);

#ifndef PLATFORM_WINDOWS /* POSIX Process only */
static void close_fd (int& fd) { if (fd >= 0) ::close (fd); fd = -1; }
#endif

void
SystemExec::init ()
{
	pthread_mutex_init (&write_lock, NULL);
	thread_active = false;
	pid = 0;
	pin[1] = -1;
	nicelevel = 0;
	envp = NULL;
#ifdef PLATFORM_WINDOWS
	stdinP[0] = stdinP[1] = INVALID_HANDLE_VALUE;
	stdoutP[0] = stdoutP[1] = INVALID_HANDLE_VALUE;
	stderrP[0] = stderrP[1] = INVALID_HANDLE_VALUE;
	w_args = NULL;
#else
	argx = NULL;
#endif
}

SystemExec::SystemExec (std::string c, std::string a)
	: cmd(c)
{
	init ();

	argp = NULL;
	make_envp();
	make_argp(a);
}

SystemExec::SystemExec (std::string c, char **a)
	: cmd(c) , argp(a)
{
	init ();

#ifdef PLATFORM_WINDOWS
	make_wargs(a);
#endif
	make_envp();
}

SystemExec::SystemExec (std::string command, const std::map<char, std::string> subs)
{
	init ();
	make_argp_escaped(command, subs);

#ifdef PLATFORM_WINDOWS
	if (argp[0] && strlen (argp[0]) > 0) {
		std::string wa = argp[0];
		// only add quotes to command if required..
		if (argp[0][0] != '"'
				&& argp[0][strlen(argp[0])-1] != '"'
				&& strchr(argp[0], ' ')) {
			wa = "\"";
			wa += argp[0];
			wa += "\"";
		}
		// ...but always quote all args
		for (int i = 1; argp[i]; ++i) {
			std::string tmp (argp[i]);
			size_t start_pos = 0;
			while ((start_pos = tmp.find("\"", start_pos)) != std::string::npos) {
				tmp.replace (start_pos, 1, "\\\"");
				start_pos += 2;
			}
			wa += " \"";
			wa += tmp;
			wa += '"';
		}
		w_args = strdup(wa.c_str());
	}
#else
	if (find_file (Searchpath (Glib::getenv ("PATH")), argp[0], cmd)) {
		// argp[0] exists in $PATH` - set it to the actual path where it was found
		free (argp[0]);
		argp[0] = strdup(cmd.c_str ());
	}
	// else argp[0] not found in path - leave it as-is, it might be an absolute path

	// Glib::find_program_in_path () is only available in Glib >= 2.28
	// cmd = Glib::find_program_in_path (argp[0]);
#endif
	make_envp();
}

char*
SystemExec::format_key_value_parameter (std::string key, std::string value)
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

#ifdef PLATFORM_WINDOWS
	/* SystemExec::make_wargs() adds quotes around the complete argument
	 * windows uses CreateProcess() with a parameter string
	 * (and not an array list of separate arguments like Unix)
	 * so quotes need to be escaped.
	 */
	start_pos = 0;
	while((start_pos = v1.find("\"", start_pos)) != std::string::npos) {
		v1.replace(start_pos, 1, "\\\"");
		start_pos += 2;
	}
#endif

	size_t len = key.length() + v1.length() + 2;
	char *mds = (char*) calloc(len, sizeof(char));
	snprintf(mds, len, "%s=%s", key.c_str(), v1.c_str());
	return mds;
}

void
SystemExec::make_argp_escaped (std::string command, const std::map<char, std::string> subs)
{

	int inquotes = 0;
	int n = 0;
	size_t i = 0;
	std::string arg = "";

	argp = (char**) malloc (sizeof(char*));

	for (i = 0; i <= command.length(); i++) { // include terminating '\0'
		char c = command.c_str()[i];
		if (inquotes) {
			if (c == '"') {
				inquotes = 0;
			} else {
				// still in quotes - just copy
				arg += c;
			}
		} else switch (c) {
			case '%' :
				c = command.c_str()[++i];
				if (c == '%' || c == '\0') {
					// "%%", "%" at end-of-string => "%"
					arg += '%';
				} else {
					// search subs for string to substitute for char
					std::map<char, std::string>::const_iterator s = subs.find(c);
					if (s != subs.end()) {
						// found substitution
						arg += s->second;
					} else {
						// not a valid substitution, just copy
						arg += '%';
						arg += c;
					}
				}
				break;
			case '\\':
				c = command.c_str()[++i];
				switch (c) {
					case ' ' :
					case '"' : arg += c; break; // "\\", "\" at end-of-string => "\"
					case '\0':
					case '\\': arg += '\\'; break;
					default  : arg += '\\'; arg += c; break;
				}
				break;
			case '"' :
				inquotes = 1;
				break;
			case ' ' :
			case '\t':
			case '\0':
				if (arg.length() > 0) {
					// if there wasn't already a space or tab, start a new parameter
					argp = (char **) realloc(argp, (n + 2) * sizeof(char *));
					argp[n++] = strdup (arg.c_str());
					arg = "";
				}
				break;
			default :
				arg += c;
				break;
		}
	}
	argp[n] = NULL;
}

SystemExec::~SystemExec ()
{
	terminate ();
	if (envp) {
		for (int i = 0; envp[i]; ++i) {
			free (envp[i]);
		}
		free (envp);
	}
	if (argp) {
		for (int i = 0; argp[i]; ++i) {
			free (argp[i]);
		}
		free (argp);
	}
#ifdef PLATFORM_WINDOWS
	if (w_args) free(w_args);
#else
	if (argx) {
		/* argx[0 .. 8] are fixed parameters to vfork-exec-wrapper */
		for (int i = 0; i < 9; ++i) {
			free (argx[i]);
		}
		free (argx);
	}
#endif
	pthread_mutex_destroy(&write_lock);
}

static void*
interposer_thread (void *arg) {
	SystemExec *sex = static_cast<SystemExec *>(arg);
	pthread_set_name ("ExecStdOut");
	sex->output_interposer();
	pthread_exit(0);
	return 0;
}

string
SystemExec::to_s () const
{
#ifdef PLATFORM_WINDOWS
	return string (w_args ? w_args : "");
#else
	stringstream out;
	if (argp) {
		for (int i = 0; argp[i]; ++i) {
			out << argp[i] << " ";
		}
	}
	return out.str();
#endif
}

size_t
SystemExec::write_to_stdin (std::string const& d, size_t len)
{
	const char *data = d.c_str();
	if (len == 0) {
		len = d.length();
	}
	return write_to_stdin ((const void*)data, len);
}

size_t
SystemExec::write_to_stdin (const char* data, size_t len)
{
	if (len == 0) {
		len = strlen (data);
	}
	return write_to_stdin ((const void*)data, len);
}

#ifdef PLATFORM_WINDOWS /* Windows Process */

/* HELPER FUNCTIONS */

static void
create_pipe (HANDLE *pipe, bool in)
{
	SECURITY_ATTRIBUTES secAtt = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
	HANDLE tmpHandle;
	if (in) {
		if (!CreatePipe(&pipe[0], &tmpHandle, &secAtt, 1024 * 1024)) return;
		if (!DuplicateHandle(GetCurrentProcess(), tmpHandle, GetCurrentProcess(), &pipe[1], 0, FALSE, DUPLICATE_SAME_ACCESS)) return;
	} else {
		if (!CreatePipe(&tmpHandle, &pipe[1], &secAtt, 1024 * 1024)) return;
		if (!DuplicateHandle(GetCurrentProcess(), tmpHandle, GetCurrentProcess(), &pipe[0], 0, FALSE, DUPLICATE_SAME_ACCESS)) return;
	}
	CloseHandle(tmpHandle);
}

static void
destroy_pipe (HANDLE pipe[2])
{
	if (pipe[0] != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe[0]);
		pipe[0] = INVALID_HANDLE_VALUE;
	}
	if (pipe[1] != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe[1]);
		pipe[1] = INVALID_HANDLE_VALUE;
	}
}

static BOOL
CALLBACK my_terminateApp(HWND hwnd, LPARAM procId)
{
	DWORD currentProcId = 0;
	GetWindowThreadProcessId(hwnd, &currentProcId);
	if (currentProcId == (DWORD)procId)
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	return TRUE;
}

/* PROCESS API */

void
SystemExec::make_envp()
{
	; /* environemt is copied over with CreateProcess(...,env=0 ,..) */
}

void
SystemExec::make_wargs (char** a)
{
	std::string wa = cmd;
	if (cmd[0] != '"' && cmd[cmd.size()] != '"' && strchr(cmd.c_str(), ' ')) { wa = "\"" + cmd + "\""; }
	std::replace(cmd.begin(), cmd.end(), '/', '\\' );
	char **tmp = ++a;
	while (tmp && *tmp) {
		wa.append(" \"");
		wa.append(*tmp);
		if (strlen(*tmp) > 0 && (*tmp)[strlen(*tmp) - 1] == '\\') {
			wa.append("\\");
		}
		wa.append("\"");
		tmp++;
	}
	w_args = strdup(wa.c_str());
}

void
SystemExec::make_argp (std::string args)
{
	std::string wa = cmd;
	if (cmd[0] != '"' && cmd[cmd.size()] != '"' && strchr(cmd.c_str(), ' ')) { wa = "\"" + cmd + "\""; }
	std::replace(cmd.begin(), cmd.end(), '/', '\\' );
	wa.append(" ");
	wa.append(args);
	w_args = strdup(wa.c_str());
}

void
SystemExec::terminate ()
{
	::pthread_mutex_lock(&write_lock);

	close_stdin();

	if (pid) {
		/* terminate */
		EnumWindows(my_terminateApp, (LPARAM)pid->dwProcessId);
		PostThreadMessage(pid->dwThreadId, WM_CLOSE, 0, 0);

		/* kill ! */
		TerminateProcess(pid->hProcess, 0xf291);

		CloseHandle(pid->hThread);
		CloseHandle(pid->hProcess);
		destroy_pipe(stdinP);
		destroy_pipe(stdoutP);
		destroy_pipe(stderrP);
		delete pid;
		pid=0;
	}
	::pthread_mutex_unlock(&write_lock);
}

int
SystemExec::wait (int options)
{
	while (is_running()) {
		WaitForSingleObject(pid->hProcess, 40);
	}
	return 0;
}

bool
SystemExec::is_running ()
{
	if (!pid) return false;
	DWORD exit_code;
	if (GetExitCodeProcess(pid->hProcess, &exit_code)) {
		if (exit_code == STILL_ACTIVE) return true;
	}
	return false;
}

int
SystemExec::start (StdErrMode stderr_mode, const char * /*vfork_exec_wrapper*/)
{
	char* working_dir = 0;

	if (pid) { return 0; }

	pid = new PROCESS_INFORMATION;
	memset(pid, 0, sizeof(PROCESS_INFORMATION));

	create_pipe(stdinP, true);
	create_pipe(stdoutP, false);

	if (stderr_mode == MergeWithStdin) {
	/* merge stout & stderr */
		DuplicateHandle(GetCurrentProcess(), stdoutP[1], GetCurrentProcess(), &stderrP[1], 0, TRUE, DUPLICATE_SAME_ACCESS);
	} else if (stderr_mode == IgnoreAndClose) {
		//TODO read/flush this pipe or close it...
		create_pipe(stderrP, false);
	} else {
		//TODO: keep stderr of this process mode.
	}

	bool success = false;
	STARTUPINFOA startupInfo = { sizeof(STARTUPINFO), 0, 0, 0,
		(unsigned long)CW_USEDEFAULT, (unsigned long)CW_USEDEFAULT,
		(unsigned long)CW_USEDEFAULT, (unsigned long)CW_USEDEFAULT,
		0, 0, 0,
		STARTF_USESTDHANDLES,
		0, 0, 0,
		stdinP[0], stdoutP[1], stderrP[1]
	};

	success = CreateProcess(0, w_args,
		0, 0, /* bInheritHandles = */ TRUE,
		(CREATE_NO_WINDOW&0) | CREATE_UNICODE_ENVIRONMENT | (0&CREATE_NEW_CONSOLE),
		/*env = */ 0,
		working_dir,
		&startupInfo, pid);

	if (stdinP[0] != INVALID_HANDLE_VALUE) {
		CloseHandle(stdinP[0]);
		stdinP[0] = INVALID_HANDLE_VALUE;
	}
	if (stdoutP[1] != INVALID_HANDLE_VALUE) {
		CloseHandle(stdoutP[1]);
		stdoutP[1] = INVALID_HANDLE_VALUE;
	}
	if (stderrP[1] != INVALID_HANDLE_VALUE) {
		CloseHandle(stderrP[1]);
		stderrP[1] = INVALID_HANDLE_VALUE;
	}

	if (!success) {
		CloseHandle(pid->hThread);
		CloseHandle(pid->hProcess);
		destroy_pipe(stdinP);
		destroy_pipe(stdoutP);
		destroy_pipe(stderrP);
		delete pid;
		pid=0;
		return -1;
	}

	int rv = pthread_create (&thread_id_tt, NULL, interposer_thread, this);
	thread_active=true;
	if (rv) {
		thread_active=false;
		terminate();
		return -2;
	}
	Sleep(20);
	return 0;
}

void
SystemExec::output_interposer()
{
	DWORD bytesRead = 0;
	char data[BUFSIZ];
#if 0 // untested code to set up nonblocking
	unsigned long l = 1;
	ioctlsocket(stdoutP[0], FIONBIO, &l);
#endif
	while(1) {
#if 0 // for non-blocking pipes..
		DWORD bytesAvail = 0;
		PeekNamedPipe(stdoutP[0], 0, 0, 0, &bytesAvail, 0);
		if (bytesAvail < 1) {Sleep(500); printf("N/A\n"); continue;}
#endif
		if (stdoutP[0] == INVALID_HANDLE_VALUE) break;
		if (!ReadFile(stdoutP[0], data, BUFSIZ - 1, &bytesRead, 0)) {
			DWORD err =  GetLastError();
			if (err == ERROR_IO_PENDING) continue;
			break;
		}
		if (bytesRead < 1) continue; /* actually not needed; but this is safe. */
		data[bytesRead] = 0;
		ReadStdout(data, bytesRead); /* EMIT SIGNAL */
	}
	Terminated(); /* EMIT SIGNAL */
	pthread_exit(0);
}

void
SystemExec::close_stdin()
{
	if (stdinP[0] != INVALID_HANDLE_VALUE) FlushFileBuffers (stdinP[0]);
	if (stdinP[1] != INVALID_HANDLE_VALUE) FlushFileBuffers (stdinP[1]);
	Sleep(200);
	destroy_pipe (stdinP);
}

size_t
SystemExec::write_to_stdin (const void* data, size_t bytes)
{
	DWORD r, c;

	::pthread_mutex_lock (&write_lock);

	c=0;
	while (c < bytes) {
		if (!WriteFile (stdinP[1], &((const char*)data)[c], bytes - c, &r, NULL)) {
			if (GetLastError() == 0xE8 /*NT_STATUS_INVALID_USER_BUFFER*/) {
				Sleep(100);
				continue;
			} else {
				fprintf(stderr, "SYSTEM-EXEC: stdin write error.\n");
				break;
			}
		}
		c += r;
	}
	::pthread_mutex_unlock(&write_lock);
	return c;
}


/* end windows process */
#else
/* UNIX/POSIX process */

extern char **environ;

void
SystemExec::make_envp()
{
	int i = 0;
	envp = (char **) calloc(1, sizeof(char*));
	/* copy current environment */
	for (i = 0; environ[i]; ++i) {
	  envp[i] = strdup(environ[i]);
	  envp = (char **) realloc(envp, (i+2) * sizeof(char*));
	}
	envp[i] = 0;
}

void
SystemExec::make_argp(std::string args)
{
	int argn = 1;
	char *cp1;
	char *cp2;

	char *carg = strdup(args.c_str());

	argp = (char **) malloc((argn + 1) * sizeof(char *));
	if (argp == (char **) 0) {
		free(carg);
		return; // FATAL
	}

	argp[0] = strdup(cmd.c_str());

	/* TODO: quotations and escapes
	 * http://stackoverflow.com/questions/1511797/convert-string-to-argv-in-c
	 *
	 * It's actually not needed. All relevant invocations specify 'argp' directly.
	 * Only 'xjadeo -L -R' uses this function and that uses neither quotations
	 * nor arguments with white-space.
	 */
	for (cp1 = cp2 = carg; *cp2 != '\0'; ++cp2) {
		if (*cp2 == ' ') {
			*cp2 = '\0';
			argp[argn++] = strdup(cp1);
			cp1 = cp2 + 1;
			argp = (char **) realloc(argp, (argn + 1) * sizeof(char *));
		}
	}
	if (cp2 != cp1) {
		argp[argn++] = strdup(cp1);
		argp = (char **) realloc(argp, (argn + 1) * sizeof(char *));
	}
	argp[argn] = (char *) 0;
	free(carg);
}

void
SystemExec::terminate ()
{
	::pthread_mutex_lock(&write_lock);

	/* close stdin in an attempt to get the child to exit cleanly.
	 */

	close_stdin();

	if (pid) {
		::usleep(200000);
		sched_yield();
		wait(WNOHANG);
	}

	/* if pid is non-zero, the child task is still executing (i.e. it did
	 * not exit in response to stdin being closed). try to kill it.
	 */

	if (pid) {
		::kill(pid, SIGTERM);
		::usleep(250000);
		sched_yield();
		wait(WNOHANG);
	}

	/* if pid is non-zero, the child task is STILL executing after being
	 * sent SIGTERM. Act tough ... send SIGKILL
	 */

	if (pid) {
		::fprintf(stderr, "Process is still running! trying SIGKILL\n");
		::kill(pid, SIGKILL);
	}

	wait();
	if (thread_active) pthread_join(thread_id_tt, NULL);
	thread_active = false;
	assert(pid == 0);
	::pthread_mutex_unlock(&write_lock);
}

int
SystemExec::wait (int options)
{
	int status = 0;
	int ret;

	if (pid == 0) return -1;

	ret = waitpid (pid, &status, options);

	if (ret == pid) {
		if (WEXITSTATUS(status) || WIFSIGNALED(status)) {
			pid=0;
		}
	} else {
		if (ret != 0) {
			if (errno == ECHILD) {
				/* no currently running children, reset pid */
				pid=0;
			}
		} /* else the process is still running */
	}
	return status;
}

bool
SystemExec::is_running ()
{
	int status = 0;
	if (pid == 0) {
		return false;
	}
	if (::waitpid (pid, &status, WNOHANG)==0) {
		return true;
	}
	return false;
}

int
SystemExec::start (StdErrMode stderr_mode, const char *vfork_exec_wrapper)
{
	if (is_running ()) {
		return 0;
	}

	int r;

	if (::pipe(pin) < 0 || ::pipe(pout) < 0 || ::pipe(pok) < 0) {
		/* Something unexpected went wrong creating a pipe. */
		return -1;
	}

	r = ::vfork();
	if (r < 0) {
		/* failed to fork */
		return -2;
	}

	if (r > 0) {
		/* main */
		pid = r;

		/* check if execve was successful. */
		close_fd (pok[1]);
		char buf;
		for (;;) {
			ssize_t n = ::read (pok[0], &buf, 1);
			if (n == 1) {
				/* child process returned from execve */
				pid=0;
				close_fd (pok[0]);
				close_fd (pok[1]);
				close_fd (pin[1]);
				close_fd (pin[0]);
				close_fd (pout[1]);
				close_fd (pout[0]);
				return -3;
			} else if (n == -1) {
				if (errno==EAGAIN || errno==EINTR) {
					continue;
				}
			}
			break;
		}

		close_fd (pok[0]);
		/* child started successfully */

		close_fd (pout[1]);
		close_fd (pin[0]);

		int rv = pthread_create (&thread_id_tt, NULL, interposer_thread, this);
		thread_active=true;

		if (rv) {
			thread_active=false;
			terminate();
			return -2;
		}
		return 0; /* all systems go - return to main */
	}

	/* XXX this should be done before vfork()
	 * calling malloc here only increases the time vfork() blocks
	 */
	int argn = 0;
	for (int i = 0; argp[i]; ++i) { argn++; }

	argx = (char **) malloc ((argn + 10) * sizeof(char*));
	argx[0] = strdup (vfork_exec_wrapper);

#define FDARG(NUM, FDN) \
	argx[NUM] = (char*) calloc(6, sizeof(char)); snprintf(argx[NUM], 6, "%d", FDN);

	FDARG (1, pok[0])
	FDARG (2, pok[1])
	FDARG (3, pin[0])
	FDARG (4, pin[1])
	FDARG (5, pout[0])
	FDARG (6, pout[1])
	FDARG (7, stderr_mode)
	FDARG (8, nicelevel)

	for (int i = 0; argp[i]; ++i) {
		argx[9+i] = argp[i];
	}
	argx[argn+9] = NULL;

	::execve (argx[0], argx, envp);

	/* if we reach here something went wrong.. */
	char buf = 0;
	(void) ::write (pok[1], &buf, 1);
	close_fd (pok[1]);
	_exit (EXIT_FAILURE);
	return -1;
}

void
SystemExec::output_interposer ()
{
	int rfd = pout[0];
	char buf[BUFSIZ];
	ssize_t r;
	unsigned long l = 1;

	ioctl (rfd, FIONBIO, &l); // set non-blocking I/O

	for (;fcntl (rfd, F_GETFL) != -1;) {
		r = read (rfd, buf, BUFSIZ - 1);
		if (r < 0 && (errno == EINTR || errno == EAGAIN)) {

#ifdef __APPLE__
again:
#endif

			/* wait till ready to read */
			struct pollfd pfd;

			pfd.fd = rfd;
			pfd.events = POLLIN|POLLERR|POLLHUP|POLLNVAL;

#ifdef __APPLE__
			/* on macOS poll() will not return when the pipe
			 * is closed in an EOF state.
			 * Work around with a timeout and fail next time
			 * when with POLLNVAL.
			 */
			int rv = poll (&pfd, 1, 1000);
#else
			int rv = poll (&pfd, 1, -1);
#endif

			if (rv == -1) {
				break;
			}

			if (pfd.revents & (POLLERR|POLLHUP|POLLNVAL)) {
				break;
			}

			if (rv == 1 && pfd.revents & POLLIN) {
				/* back to read(2) call */
				continue;
			}
#ifdef __APPLE__
			if (rv == 0) {
				/* Timeout, poll again */
				goto again;
			}
#endif
		}
		if (r <= 0) {
			break;
		}
		buf[r]=0;
		std::string rv = std::string (buf, r);
		ReadStdout (rv, r); /* EMIT SIGNAL */
	}
	Terminated (); /* EMIT SIGNAL */
	pthread_exit (0);
}

void
SystemExec::close_stdin()
{
	if (pin[1] < 0) {
		return;
	}
	close_fd (pin[0]);
	close_fd (pin[1]);
	close_fd (pout[0]);
	close_fd (pout[1]);
}

size_t
SystemExec::write_to_stdin (const void* data, size_t bytes)
{
	ssize_t r;
	size_t c;
	::pthread_mutex_lock (&write_lock);

	c = 0;
	while (c < bytes) {
		for (;;) {
			r = ::write (pin[1], &((const char*)data)[c], bytes - c);
			if (r < 0 && (errno == EINTR || errno == EAGAIN)) {
				sleep(1);
				continue;
			}
			if ((size_t) r != (bytes-c)) {
				::pthread_mutex_unlock(&write_lock);
				return c;
			}
			break;
		}
		c += r;
	}
	fsync (pin[1]);
	::pthread_mutex_unlock(&write_lock);
	return c;
}

#endif // end UNIX process
