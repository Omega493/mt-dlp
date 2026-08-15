/*
 * Copyright (C) 2026 Omega493

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PANIC_HANDLER
#define PANIC_HANDLER

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <io.h>
# include <process.h>
# include <Windows.h>
// dbghelp must be included after everything else.
// ordering in ascending order be damned.
# include <dbghelp.h>
# pragma comment(lib, "dbghelp")
#else // ^^^ _WIN32 / !_WIN32 vvv
# include <dlfcn.h>
# include <execinfo.h>
# include <unistd.h>
#endif // ^^^ _WIN32 ^^^

#include <csignal>

template <size_t N_>
inline void stdout_write(const char(&buf_)[N_]) {
#ifdef _WIN32
# define write _write
#endif // ^^^ _WIN32 ^^^

  const auto x{ write(1, buf_, N_ - 1) };
}

template <size_t N_>
inline void stderr_write(const char(&buf_)[N_]) {
#ifdef _WIN32
# define write _write
#endif // ^^^ _WIN32 ^^^

  const auto x{ write(2, buf_, N_ - 1) };
}

#ifdef _WIN32
LONG WINAPI panic_handler(EXCEPTION_POINTERS*);
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
void panic_handler(const int, siginfo_t*, void*);
#endif // ^^^ _WIN32 ^^^

#endif // PANIC_HANDLER
