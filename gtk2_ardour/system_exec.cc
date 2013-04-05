/*
    Copyright (C) 2010 Paul Davis
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef __WIN32__
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#endif

#include "system_exec.h"

using namespace std;
void * interposer_thread (void *arg);

SystemExec::SystemExec (std::string c, std::string a)
	: cmd(c)
{
	pthread_mutex_init(&write_lock, NULL);
	thread_active=false;
	pid = 0;
	pin[1] = -1;
	nicelevel = 0;
	envp = NULL;
	argp = NULL;
#ifdef __WIN32__
	stdinP[0] = stdinP[1] = INVALID_HANDLE_VALUE;
	stdoutP[0] = stdoutP[1] = INVALID_HANDLE_VALUE;
	stderrP[0] = stderrP[1] = INVALID_HANDLE_VALUE;
#endif
	make_envp();
	make_argp(a);
}

SystemExec::SystemExec (std::string c, char **a)
	: cmd(c) , argp(a)
{
	pthread_mutex_init(&write_lock, NULL);
	thread_active=false;
	pid = 0;
	pin[1] = -1;
	nicelevel = 0;
	envp = NULL;
#ifdef __WIN32__
	stdinP[0] = stdinP[1] = INVALID_HANDLE_VALUE;
	stdoutP[0] = stdoutP[1] = INVALID_HANDLE_VALUE;
	stderrP[0] = stderrP[1] = INVALID_HANDLE_VALUE;
	make_wargs(a);
#endif
	make_envp();
}

SystemExec::~SystemExec ()
{
	terminate ();
	if (envp) {
		for (int i=0;envp[i];++i) {
		  free(envp[i]);
		}
		free (envp);
	}
	if (argp) {
		for (int i=0;argp[i];++i) {
		  free(argp[i]);
		}
		free (argp);
	}
#ifdef __WIN32__
	if (w_args) free(w_args);
#endif
	pthread_mutex_destroy(&write_lock);
}

void *
interposer_thread (void *arg) {
	SystemExec *sex = static_cast<SystemExec *>(arg);
	sex->output_interposer();
	pthread_exit(0);
	return 0;
}

#ifdef __WIN32__ /* Windows Process */

/* HELPER FUNCTIONS */

static void create_pipe (HANDLE *pipe, bool in) {
	SECURITY_ATTRIBUTES secAtt = { sizeof( SECURITY_ATTRIBUTES ), NULL, TRUE };
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

static void destroy_pipe (HANDLE pipe[2]) {
	if (pipe[0] != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe[0]);
		pipe[0] = INVALID_HANDLE_VALUE;
	}
	if (pipe[1] != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe[1]);
		pipe[1] = INVALID_HANDLE_VALUE;
	}
}

static BOOL CALLBACK my_terminateApp(HWND hwnd, LPARAM procId)
{
	DWORD currentProcId = 0;
	GetWindowThreadProcessId(hwnd, &currentProcId);
	if (currentProcId == (DWORD)procId)
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	return TRUE;
}

/* PROCESS API */

void
SystemExec::make_envp() {
	;/* environemt is copied over with CreateProcess(...,env=0 ,..) */
}

void
SystemExec::make_wargs(char **a) {
	std::string wa = cmd;
	if (cmd[0] != '"' && cmd[cmd.size()] != '"' && strchr(cmd.c_str(), ' ')) { wa = "\"" + cmd + "\""; }
	std::replace(cmd.begin(), cmd.end(), '/', '\\' );
	char **tmp = a;
	while (tmp && *tmp) {
		wa.append(" \"");
		wa.append(*tmp);
		wa.append("\"");
		tmp++;
	}
	w_args = strdup(wa.c_str());
}

void
SystemExec::make_argp(std::string args) {
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
		WaitForSingleObject(pid->hProcess, INFINITE);
		Sleep(20);
	}
	return 0;
}

bool
SystemExec::is_running ()
{
	return pid?true:false;
}

int
SystemExec::start (int stderr_mode)
{
	char* working_dir = 0;

	if (pid) { return 0; }

	pid = new PROCESS_INFORMATION;
	memset(pid, 0, sizeof(PROCESS_INFORMATION));

	create_pipe(stdinP, true);
	create_pipe(stdoutP, false);

	if (stderr_mode == 2) {
	/* merge stout & stderr */
		DuplicateHandle(GetCurrentProcess(), stdoutP[1], GetCurrentProcess(), &stderrP[1], 0, TRUE, DUPLICATE_SAME_ACCESS);
	} else if (stderr_mode == 1) {
		//TODO read/flush this pipe or close it...
		create_pipe(stderrP, false);
	} else {
		//TODO: keep stderr of this process mode.
	}

	bool success = false;
	STARTUPINFOA startupInfo = { sizeof( STARTUPINFO ), 0, 0, 0,
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

	int rv = pthread_create(&thread_id_tt, NULL, interposer_thread, this);
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
	while(1) {
#if 0 // for non-blocking pipes..
		DWORD bytesAvail = 0;
		PeekNamedPipe(stdoutP[0], 0, 0, 0, &bytesAvail, 0);
		if (bytesAvail < 1) {Sleep(500); printf("N/A\n"); continue;}
#endif
		if (stdoutP[0] == INVALID_HANDLE_VALUE) break;
		if (!ReadFile(stdoutP[0], data, BUFSIZ, &bytesRead, 0)) break;
		if (bytesRead < 1) continue; /* actually not needed; but this is safe. */
		data[bytesRead] = 0;
		ReadStdout(data, bytesRead);/* EMIT SIGNAL */
	}
	Terminated();/* EMIT SIGNAL */
}

void
SystemExec::close_stdin()
{
	if (stdinP[0]!= INVALID_HANDLE_VALUE)  FlushFileBuffers(stdinP[0]);
	if (stdinP[1]!= INVALID_HANDLE_VALUE)  FlushFileBuffers(stdinP[1]);
	Sleep(200);
	destroy_pipe(stdinP);
}

int
SystemExec::write_to_stdin(std::string d, size_t len)
{
	const char *data;
	DWORD r,c;

	::pthread_mutex_lock(&write_lock);

	data=d.c_str();
	if (len == 0) {
		len=(d.length());
	}
	c=0;
	while (c < len) {
		if (!WriteFile(stdinP[1], data+c, len-c, &r, NULL)) {
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
SystemExec::make_envp() {
	int i=0;
	envp = (char **) calloc(1, sizeof(char*));
	/* copy current environment */
	for (i=0;environ[i];++i) {
	  envp[i] = strdup(environ[i]);
	  envp = (char **) realloc(envp, (i+2) * sizeof(char*));
	}
	envp[i] = 0;
}

void
SystemExec::make_argp(std::string args) {
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
	close_stdin();
	if (pid) {
		::usleep(100000);
		wait(WNOHANG);
	}

	if (pid) {
		::fprintf(stderr, "Child process is running. trying SIGTERM\n");
		::kill(pid, SIGTERM);
		::usleep(50000);
		wait(WNOHANG);
	}
	if (pid) {
		::fprintf(stderr, "Process is still running! trying SIGKILL\n");
		::kill(pid, SIGKILL);
	}

	wait();
	if (thread_active) pthread_join(thread_id_tt, NULL);
	thread_active = false;
	::pthread_mutex_unlock(&write_lock);
}

int
SystemExec::wait (int options)
{
	int status=0;
	if (pid==0) return -1;
	if (pid==::waitpid(pid, &status, options)) {
		pid=0;
	}
	if (errno == ECHILD) {
		pid=0;
	}
	return status;
}

bool
SystemExec::is_running ()
{
	int status=0;
	if (pid==0) return false;
	if (::waitpid(pid, &status, WNOHANG)==0) return true;
	return false;
}

int
SystemExec::start (int stderr_mode)
{
	if (is_running()) {
		return 0; // mmh what to return here?
	}
	int r;

	if (::pipe(pin) < 0 || ::pipe(pout) < 0 || ::pipe(pok) < 0) {
		/* Something unexpected went wrong creating a pipe. */
		return -1;
	}

	r = ::fork();
	if (r < 0) {
		/* failed to fork */
		return -2;
	}

	if (r > 0) {
		/* main */
		pid=r;

		/* check if execve was successful. */
		::close(pok[1]);
		char buf;
		for ( ;; ) {
			int n = ::read(pok[0], &buf, 1 );
			if ( n==1 ) {
				/* child process returned from execve */
				pid=0;
				::close(pok[0]);
				::close(pin[1]);
				::close(pin[0]);
				::close(pout[1]);
				::close(pout[0]);
				pin[1] = -1;
				return -3;
			} else if ( n==-1 ) {
				 if ( errno==EAGAIN || errno==EINTR )
					 continue;
			}
			break;
		}
		::close(pok[0]);
		/* child started successfully */

#if 0
/* use fork for output-interposer
 * it will run in a separated process
 */
		/* catch stdout thread */
		r = ::fork();
		if (r < 0) {
			// failed to fork
			terminate();
			return -2;
		}
		if (r == 0) {
			/* 2nd child process - catch stdout */
			::close(pin[1]);
			::close(pout[1]);
			output_interposer();
			exit(0);
		}
		::close(pout[1]);
		::close(pin[0]);
		::close(pout[0]);
#else /* use pthread */
		::close(pout[1]);
		::close(pin[0]);
		int rv = pthread_create(&thread_id_tt, NULL, interposer_thread, this);

		thread_active=true;
		if (rv) {
			thread_active=false;
			terminate();
			return -2;
		}
#endif
		return 0; /* all systems go - return to main */
	}

	/* child process - exec external process */
	::close(pok[0]);
	::fcntl(pok[1], F_SETFD, FD_CLOEXEC);

	::close(pin[1]);
	if (pin[0] != STDIN_FILENO) {
	  ::dup2(pin[0], STDIN_FILENO);
	}
	::close(pin[0]);
	::close(pout[0]);
	if (pout[1] != STDOUT_FILENO) {
		::dup2(pout[1], STDOUT_FILENO);
	}

	if (stderr_mode == 2) {
		/* merge STDERR into output */
		if (pout[1] != STDERR_FILENO) {
			::dup2(pout[1], STDERR_FILENO);
		}
	} else if (stderr_mode == 1) {
		/* ignore STDERR */
		::close(STDERR_FILENO);
	} else {
		/* keep STDERR */
	}

	if (pout[1] != STDOUT_FILENO && pout[1] != STDERR_FILENO) {
		::close(pout[1]);
	}

	if (nicelevel !=0) {
		::nice(nicelevel);
	}

#if 0
	/* chdir to executable dir */
	char *directory;
	directory = strdup(cmd.c_str());
	if (strrchr(directory, '/') != (char *) 0) {
		::chdir(directory);
	}
	free(directory);
#endif

#ifdef HAVE_SIGSET
	sigset(SIGPIPE, SIG_DFL);
#else
	signal(SIGPIPE, SIG_DFL);
#endif

	::execve(argp[0], argp, envp);
	/* if we reach here something went wrong.. */
	char buf = 0;
	(void) ::write(pok[1], &buf, 1 );
	(void) ::close(pok[1]);
	exit(-1);
	return -1;
}

void
SystemExec::output_interposer()
{
	int rfd=pout[0];
	char buf[BUFSIZ];
	size_t r;
	for (;fcntl(rfd, F_GETFL)!=-1;) {
		r = read(rfd, buf, sizeof(buf));
		if (r < 0 && (errno == EINTR || errno == EAGAIN)) {
			::usleep(1000);
			continue;
		}
		if (r <= 0) {
			break;
		}
		buf[r]=0;
		std::string rv = std::string(buf,r); // TODO: check allocation strategy
		ReadStdout(rv, r);/* EMIT SIGNAL */
	}
	Terminated();/* EMIT SIGNAL */
}

void
SystemExec::close_stdin()
{
	if (pin[1]<0) return;
	::close(pin[0]);
	::close(pin[1]);
	::close(pout[0]);
	::close(pout[1]);
	pin[1] = - 1; // mark as closed
}

int
SystemExec::write_to_stdin(std::string d, size_t len)
{
	const char *data;
	size_t r,c;
	::pthread_mutex_lock(&write_lock);

	data=d.c_str();
	if (len == 0) {
		len=(d.length());
	}
	c=0;
	while (c < len) {
		for (;;) {
			r=::write(pin[1], data+c, len-c);
			if (r < 0 && (errno == EINTR || errno == EAGAIN)) {
				sleep(1);
				continue;
			}
			if (r != (len-c)) {
				::pthread_mutex_unlock(&write_lock);
				return c;
			}
			break;
		}
		c += r;
	}
	fsync(pin[1]);
	::pthread_mutex_unlock(&write_lock);
	return c;
}

#endif // end UNIX process

#endif /* WITH_VIDEOTIMELINE */
