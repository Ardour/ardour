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
#ifndef __ardour_system_exec_h__
#define __ardour_system_exec_h__

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#include <string>
#include <pthread.h>
#include <signal.h>
#ifdef NOPBD  /* outside ardour */
#include <sigc++/bind.h>
#include <sigc++/signal.h>
#else
#include <pbd/signals.h>
#endif

/** @class: SystemExec
 *  @brief execute an external command
 *
 * This class allows launche an external command-line application
 * opening a full-duplex connection to its standard I/O.
 *
 * In Ardour context it is used to launch xjadeo and ffmpeg.
 *
 * The \ref write_to_stdin function provides for injecting data into STDIN
 * of the child-application while output of the program to STDOUT/STDERR is
 * forwarded using the \ref ReadStdout signal.
 * \ref Terminated is sent if the child application exits.
 *
 */
class SystemExec
{
	public:
		/** prepare execution of a program with 'execve'
		 *
		 * This function takes over the existing environment variable and provides
		 * an easy way to speciy command-line arguments for the new process.
		 *
		 * Note: The argument parser does not interpret quotation-marks and splits
		 * arugments on whitespace. The argument string can be empty.
		 * The alternative constructor below allows to specify quoted parameters
		 * incl. whitespace.
		 *
		 * @param c program pathname that identifies the new process image file.
		 * @param a string of commandline-arguments to be passed to the new program.
		 */
		SystemExec (std::string c, std::string a = "");
		/** similar to \ref SystemExec but allows to specify custom arguments
		 *
		 * @param c program pathname that identifies the new process image file.
		 * @param a array of argument strings passed to the new program as 'argv'.
		 *          it must be terminated by a null pointer (see the 'evecve'
		 *          POSIX-C documentation for more information)
		 *          The array must be dynamically allocated using malloc or strdup.
		 *          Unless they're NULL, the array itself and each of its content
		 *          memory is freed() in the destructor.
		 *
		 */
		SystemExec (std::string c, char ** a);
		virtual ~SystemExec ();

		/** fork and execute the given program
		 *
		 * @param stderr_mode select what to do with program's standard error
		 * output:
		 * '0': keep STDERR; mix it with parent-process' STDERR
		 * '1': ignore STDERR of child-program
		 * '2': merge STDERR into STDOUT and send it with the
		 *      ReadStdout signal.
		 * @return If the process is already running or was launched successfully
		 * the function returns zero (0). A negative number indicates an error.
		 */
		int start (int stderr_mode = 1);
		/** kill running child-process
		 *
		 * if a child process exists trt to shut it down by closing its STDIN.
		 * if the program dies not react try SIGTERM and eventually SIGKILL
		 */
		void terminate ();
		/** check if the child programm is (still) running.
		 *
		 * This function calls waitpid(WNOHANG) to check the state of the
		 * child-process.
		 * @return true if the program is (still) running.
		 */
		bool is_running ();
		/** call the waitpid system-call with the pid of the child-program
		 *
		 * Basically what \ref terminate uses internally.
		 *
		 * This function is only useful if you want to control application
		 * termination yourself (eg timeouts or progress-dialog).
		 * @param option flags - see waitpid manual
		 * @return status info from waitpid call (not waitpid's return value)
		 * or -1 if the child-program is not running.
		 */
		int wait (int options=0);
		/** closes both STDIN and STDOUT connections to/from
		 * the child-program.
		 * With the output-interposer thread gone, the program
		 * should terminate.
		 * used by \ref terminate()
		 */
		void close_stdin ();
		/** write into child-program's STDIN
		 * @param d data to write
		 * @param len length of data to write, if it is 0 (zero), d.length() is
		 * used to determine the number of bytes to transmit.
		 * @return number of bytes written.
		 */
		int write_to_stdin (std::string d, size_t len=0);

		/** The ReadStdout signal is emitted when the application writes to STDOUT.
		 * it passes the written data and its length in bytes as arguments to the bound
		 * slot(s).
		 */
#ifdef NOPBD  /* outside ardour */
		sigc::signal<void, std::string,size_t> ReadStdout;
#else
		PBD::Signal2<void, std::string,size_t> ReadStdout;
#endif

		/** The Terminated signal is emitted when application terminates. */
#ifdef NOPBD  /* outside ardour */
		sigc::signal<void> Terminated;
#else
		PBD::Signal0<void> Terminated;
#endif

		/** interposer to emit signal for writes to STDOUT/ERR.
		 *
		 * Thread that reads the stdout of the forked
		 * process and signal-sends it to the main thread.
		 * It also emits the Terminated() signal once
		 * the the forked process closes it's stdout.
		 *
		 * Note: it's actually 'private' function but used
		 * by the internal pthread, which only has a pointer
		 * to this instance and thus can only access public fn.
		 */
		void output_interposer ();

	protected:
		std::string cmd; ///< path to command - set when creating the class
		int nicelevel; ///< process nice level - defaults to 0

		void make_argp(std::string);
		void make_envp();

		char **argp;
		char **envp;

	private:
#ifdef __WIN32__
		PROCESS_INFORMATION *pid;
		HANDLE stdinP[2];
		HANDLE stdoutP[2];
		HANDLE stderrP[2];
		char *w_args;
		void make_wargs(char **);
#else
		pid_t pid;
#endif
		pthread_mutex_t write_lock;

		int fdin; ///< file-descriptor for writing to child's STDIN. This variable is identical to pin[1] but also used as status check if the stdin pipe is open: <0 means closed.
		int pok[2];
		int pin[2];
		int pout[2];

		pthread_t      thread_id_tt;
		bool           thread_active;
};

#endif /* __ardour_system_exec_h__ */
