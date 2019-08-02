/*
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
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

int ardour_main (int argc, char *argv[]);

#if (defined(COMPILER_MSVC) && defined(NDEBUG) && !defined(RDC_BUILD))

#include <fcntl.h>
#include <shellapi.h>

bool IsAConsolePort (HANDLE handle)
{
DWORD mode;

	return (GetConsoleMode(handle, &mode) != 0);
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
int   ret  = (-1);
char  szPathToProgram[768];
char* argv[256];

	// Essential!!  Make sure that any files used by Ardour
	//              will be created or opened in BINARY mode!
	_fmode = O_BINARY;

	GetModuleFileName (NULL, (LPSTR)szPathToProgram, (DWORD)sizeof(szPathToProgram));
	argv[0] = new char[(strlen(szPathToProgram) + 1)];

	if (argv[0])
	{
		LPWSTR  lpwCmdLine         = 0;
		int     count, nArgs, argc = 1;
		size_t  argStringLen       = strlen(lpCmdLine);

		// Copy the program path to argv[0]
		strcpy (argv[0], szPathToProgram);

		// Parse the user's command line and add any parameters to argv
		if (argStringLen)
		{
			lpwCmdLine = new wchar_t[argStringLen+1];
			mbstowcs (lpwCmdLine, lpCmdLine, argStringLen+1);

			LPWSTR* pwArgv = CommandLineToArgvW ((LPCWSTR)lpwCmdLine, &nArgs);

			if (pwArgv && nArgs)
			{
				for (count = 1; count <= nArgs; count++)
				{
					int argChars = wcslen (pwArgv[count-1]);
					if (0 != (argv[count] = new char[(argChars+1)]))
					{
						argc++;
						wcstombs (argv[count], pwArgv[count-1], argChars+1);

						// Append a NULL to the argv vector
						if (argc < 255)
							argv[count+1] = 0;
					}
				}
			}

			if (pwArgv)
				LocalFree (pwArgv);
		}

		// If the user started Mixbus from a console, re-attach
		// to the console so we can see 'printf()' output etc.
		FILE  *pStdOut = 0, *pStdErr = 0;
		BOOL  bConsole = AttachConsole(ATTACH_PARENT_PROCESS);
		HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

		if ((bConsole) && (IsAConsolePort(hStdOut)))
		{
			pStdOut = freopen( "CONOUT$", "w", stdout );
			pStdErr = freopen( "CONOUT$", "w", stderr );
		}

		ret = ardour_main (argc, argv);

		if (pStdOut)
			fclose (pStdOut);
		if (pStdErr)
			fclose (pStdErr);

		if (bConsole)
		{
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

		for (count = 0; count < argc; count++)
			delete[] argv[count];

		if (lpwCmdLine)
			delete[] lpwCmdLine;
	}

	return (ret);
}

#endif
