/* STATIC FUNCTIONS -- INCLUDE DIRECTLY BEFORE  main () */

#if (!defined COMPILER_MSVC && defined PLATFORM_WINDOWS)
#include <windows.h>

static FILE* pStdOut = 0;
static FILE* pStdErr = 0;
static BOOL  bConsole;
static HANDLE hStdOut;

static bool
IsAConsolePort (HANDLE handle)
{
	DWORD mode;
	return (GetConsoleMode(handle, &mode) != 0);
}

static void
console_madness_begin ()
{
	bConsole = AttachConsole(ATTACH_PARENT_PROCESS);
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	/* re-attach to the console so we can see 'printf()' output etc.
	 * for MSVC see  gtk2_ardour/msvc/winmain.cc
	 */

	if ((bConsole) && (IsAConsolePort(hStdOut))) {
		pStdOut = freopen( "CONOUT$", "w", stdout );
		pStdErr = freopen( "CONOUT$", "w", stderr );
	}
}

static void
console_madness_end ()
{
	if (pStdOut) {
		fclose (pStdOut);
	}
	if (pStdErr) {
		fclose (pStdErr);
	}

	if (bConsole) {
		// Detach and free the console from our application
		INPUT_RECORD input_record;

		input_record.EventType = KEY_EVENT;
		input_record.Event.KeyEvent.bKeyDown = TRUE;
		input_record.Event.KeyEvent.dwControlKeyState = 0;
		input_record.Event.KeyEvent.uChar.UnicodeChar = VK_RETURN;
		input_record.Event.KeyEvent.wRepeatCount      = 1;
		input_record.Event.KeyEvent.wVirtualKeyCode   = VK_RETURN;
		input_record.Event.KeyEvent.wVirtualScanCode  = MapVirtualKey( VK_RETURN, 0 );

		DWORD written = 0;
		WriteConsoleInput( GetStdHandle( STD_INPUT_HANDLE ), &input_record, 1, &written );

		FreeConsole();
	}
}

#elif (defined(COMPILER_MSVC) && defined(NDEBUG) && !defined(RDC_BUILD))

// these are not used here. for MSVC see gtk2_ardour/msvc/winmain.cc
static void console_madness_begin () {}
static void console_madness_end () {}

#else

static void console_madness_begin () {}
static void console_madness_end () {}

#endif
