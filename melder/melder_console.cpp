/* melder_console.cpp
 *
 * Copyright (C) 1992-2018,2020,2022,2024 Paul Boersma
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

#include "melder.h"
#ifdef _WIN32
	#include <windows.h>
	#include <fcntl.h>
	#include <io.h>
	#include <assert.h>
#endif
#if defined (PRAAT_LIB)
	#include <R_ext/Print.h>   // pladdrr: Rprintf / REprintf (API)
	#include <thread>   // pladdrr: main-thread gating
#endif

/*
	MelderConsole is the interface through which Melder_casual writes to
	the console (on stderr), and through which Melder_info writes to the
	console (on stdout) if it does not write to the GUI.
*/

namespace MelderConsole {// reopen
	/*
		On Windows, the console ("command prompt") understands UTF-16,
		independently of the selected code page, so we use UTF-16 by default.
		We could use UTF-8 as well, but this will work only if the code page
		is 65001 (i.e. one would have to type "chcp 65001" into the console).
		In redirection cases (pipes or files), the encoding will often have
		to be different.

		On Unix-like platforms (MacOS and Linux), the console understands UTF-8,
		so we use UTF-8 by default. Other programs are usually fine handling UTF-8,
		so we are probably good even in the context of pipes or redirection.
	*/
	MelderConsole::Encoding encoding =
		#if defined (_WIN32)
			MelderConsole::Encoding::UTF16;
		#else
			MelderConsole::Encoding::UTF8;
		#endif
}

void MelderConsole::setEncoding (MelderConsole::Encoding newEncoding) {
	MelderConsole :: encoding = newEncoding;
}

/*
	stdout and stderr should be kept distinct. For instance, if you do
		praat test.praat > out.txt
	the output of Melder_info should go to the file `out.txt`,
	whereas the output of Melder_casual should go to the console or terminal.

	On MacOS and Linux this requirement is satisfied by default,
	but on Windows satisfying this requirement involves some work.
*/

FILE *Melder_stdout, *Melder_stderr;

void MelderConsole_init () {
	trace (U"init");
	#if defined (_WIN32) && ! defined (PRAAT_LIB)
		/*
			Stdout and stderr are initialized automatically if we are redirected to a pipe or file.
			Stdout and stderr are not initialized, however, if Praat is started from the console,
			neither in GUI mode nor in console mode; in these latter cases,
			we manually attach Melder_stdout and Melder_stderr to the calling console.
		*/
		const bool stdoutHasBeenInitialized = ( _fileno (stdout) >= 0 );   // usual with pipe or file
		if (! stdoutHasBeenInitialized) {
			/*
				Don't change the following four lines into
					freopen ("CONOUT$", "w", stream);
				because if you did that, the distinction between stdout and stderr would be lost.
			*/
			const HANDLE osfStdoutHandle = GetStdHandle (STD_OUTPUT_HANDLE);
			if (osfStdoutHandle) {
				const int fileDescriptor = _open_osfhandle ((intptr_t) osfStdoutHandle, _O_TEXT);
				Melder_assert (fileDescriptor != 0);
				FILE *f = _fdopen (fileDescriptor, "w");
				if (f) {   // usually false under Cygwin
					#if defined (__clang__)
						Melder_stdout = f;   // usual under MSYS with Clang
					#else
						*stdout = *f;   // usual under GCC (MSYS or Cygwin for Windows)
					#endif
				}
			}
		};
		const bool stderrHasBeenInitialized = ( _fileno (stderr) >= 0 );   // usual with pipe or file
		if (! stderrHasBeenInitialized) {
			/*
				Don't change the following four lines into
					freopen ("CONOUT$", "w", stream);
				because if you did that, the distinction between stdout and stderr would be lost.
			*/
			const HANDLE osfStderrHandle = GetStdHandle (STD_ERROR_HANDLE);
			if (osfStderrHandle) {
				const int fileDescriptor = _open_osfhandle ((intptr_t) osfStderrHandle, _O_TEXT);
				Melder_assert (fileDescriptor != 0);
				FILE *f = _fdopen (fileDescriptor, "w");
				if (f) {   // usually false under Cygwin
					#if defined (__clang__)
						Melder_stderr = f;   // usual under MSYS with Clang
					#else
						*stderr = *f;  // usual under GCC (MSYS or Cygwin for Windows)
					#endif
				}
			}
		};
	#endif
	#if ! defined (PRAAT_LIB)
	if (! Melder_stdout)
		Melder_stdout = stdout;
	if (! Melder_stderr)
		Melder_stderr = stderr;
	#endif
}

void MelderConsole::write (conststring32 message, bool useStderr) {
	if (! message)
		return;
#if defined (PRAAT_LIB)
	// pladdrr: R's console API (Rprintf/REprintf) is only safe on the main
	// thread. Record the first writer — the R main thread at package load — and
	// drop messages from worker threads instead of calling R internals
	// off-main-thread (Melder_casual can fire inside Praat's threaded frame
	// loops, e.g. Sound_to_Pitch.cpp). R_Outputfile/R_Consolefile were
	// considered but R CMD check flags them as non-API entry points.
	static std::thread::id theConsoleThread;
	static std::once_flag theConsoleThreadInitialized;
	std::call_once (theConsoleThreadInitialized, [] () {
		theConsoleThread = std::this_thread::get_id ();
	});
	if (std::this_thread::get_id () != theConsoleThread)
		return;
	static std::mutex theConsoleMutex;
	std::lock_guard<std::mutex> lock (theConsoleMutex);
	if (useStderr)
		REprintf ("%s", Melder_peek32to8 (message));
	else
		Rprintf ("%s", Melder_peek32to8 (message));
	return;
#endif
	FILE *f = useStderr ? Melder_stderr : Melder_stdout;
	// pladdrr: in the embedded (no-GUI) context the lazy init that assigns these
	// globals may never run, leaving f == nullptr; a casual message from a
	// (possibly worker-thread) DSP routine then hits fputc(NULL) -> flockfile(0x68)
	// segfault. Fall back to the real libc streams. See PRAAT_MODIFICATIONS.md.
	if (! f)
		f = useStderr ? stderr : stdout;
	if (! f)
		return;
	if (MelderConsole :: encoding == Encoding::UTF16) {
		#if defined (_WIN32)
			fflush (f);
			int savedMode = _setmode (_fileno (f), _O_U16TEXT);   // without line-break translation
		#endif
		fwprintf (f, L"%ls", Melder_peek32to16 (message));   // with line-break translation (Windows)
		fflush (f);
		#if defined (_WIN32)
			_setmode (_fileno (f), savedMode);
		#endif
	} else if (MelderConsole :: encoding == Encoding::UTF8) {
		for (const char32 *p = & message [0]; *p != U'\0'; p ++) {
			char32 kar = *p;
			if (kar <= 0x00'007F) {
				fputc ((int) kar, f);   // because fputc wants an int instead of a uint8 (guarded conversion)
			} else if (kar <= 0x00'07FF) {
				fputc (0xC0 | (kar >> 6), f);
				fputc (0x80 | (kar & 0x00'003F), f);
			} else if (kar <= 0x00'FFFF) {
				fputc (0xE0 | (kar >> 12), f);
				fputc (0x80 | ((kar >> 6) & 0x00'003F), f);
				fputc (0x80 | (kar & 0x00'003F), f);
			} else {
				fputc (0xF0 | (kar >> 18), f);
				fputc (0x80 | ((kar >> 12) & 0x00'003F), f);
				fputc (0x80 | ((kar >> 6) & 0x00'003F), f);
				fputc (0x80 | (kar & 0x00'003F), f);
			}
		}
		fflush (f);
	} else if (MelderConsole :: encoding == Encoding::ANSI) {
		const integer n = Melder_length (message);
		for (integer i = 0; i < n; i ++) {
			/*
				We convert Unicode to ISO 8859-1 by simple truncation. This loses information.
			*/
			unsigned int kar = message [i] & 0x00'00FF;
			fputc ((int) kar, f);
		}
		fflush (f);
	} else {
		// should not happen
	}
}

/* End of file melder_console.cpp */
