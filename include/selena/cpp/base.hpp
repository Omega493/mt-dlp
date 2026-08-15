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

#ifndef SELENA_BASE_HPP
#define SELENA_BASE_HPP

// Might be useful in classes dealing with raw ptrs, where smart ptrs either introduce unnecessary complexity.
// or is just not required / a viable option (ex. while working with C APIs).
// Also, don't panic if "Function definition for 'NO_COPY_MOVE' not found."  or smtg similar occurs.
// Copy/move is blocked when this is used.
#define NO_COPY(ClassName) \
  ClassName(const ClassName&) = delete; \
  ClassName& operator=(const ClassName&) = delete;

#define NO_MOVE(ClassName) \
  ClassName(ClassName&&) = delete; \
  ClassName& operator=(ClassName&&) = delete;

#define NO_COPY_MOVE(ClassName) \
  ClassName(const ClassName&) = delete; \
  ClassName& operator=(const ClassName&) = delete; \
  ClassName(ClassName&&) = delete; \
  ClassName& operator=(ClassName&&) = delete;

#ifdef _MSC_VER
#  define NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#  define NOINLINE __attribute__((noinline))
#else
#  define NOINLINE
#endif

#endif // SELENA_BASE_HPP
