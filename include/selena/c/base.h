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

#ifndef SELENA_C_BASE

// This header defines a bunch of macros.
// It also handles conversion to C++ when this header is included in a C++ source.
#define SELENA_C_BASE

#ifdef __cplusplus
# include <cstddef>
#else // ^^^ C++ / C vvv
# include <stddef.h>
#endif // ^^^ __cplusplus ^^^

// Define thread_local.
#ifdef __cplusplus // If C++, directly use thread_local specifier.
# define thread_local thread_local
#else // ^^^ C++ | C vvv
# ifndef thread_local
// If C11+, directly include the thread_local specifier.
#   if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#     include <threads.h>
#   elifdef _MSC_VER // vvv MSVC fallback vvv
#     define thread_local __declspec(thread)
#   elif defined(__GNUC__) || defined(__clang__) // vvv GCC / Clang fallback vvv
#     define thread_local __thread
#   else
#     error "Thread-local storage is not supported by this compiler."
#   endif // C11+ check / fallbacks
# endif // thread_local
#endif // ^^^ __cplusplus | End thread_local definition ^^^

// constexpr specifier for C++.
#ifdef __cplusplus
# define SELENA_CONSTEXPR constexpr
#else
# define SELENA_CONSTEXPR
#endif // ^^^ enable constexpr in C++ ^^^

// noexcept specifier for C++.
#ifdef __cplusplus
# define SELENA_NOEXCEPT noexcept
#else
# define SELENA_NOEXCEPT
#endif // ^^^ enable noexcept in C++ ^^^

// Define nullptr.
#ifdef __cplusplus // If header is being used in C++, directly use nullptr.
using selena_nullptr_t = std::nullptr_t;
# define selena_nullptr nullptr
// If stdc is C23 and compiler is not MSVC, use nullptr from there.
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && !defined(_MSC_VER)
# define selena_nullptr_t nullptr_t
# define selena_nullptr nullptr
#else // Else dafault to handmade fallbacks.
  typedef void* selena_nullptr_t;
# define selena_nullptr ((selena_nullptr_t)0)
// In MSVC, doing something like "int x = selena_nullptr;" will result in the warning C4047.
// It is defined as follows: "'initializing' : 'int' differs in levels of indirection from 'selena_nullptr_t'".
// The following three lines turns this warning into a compile failure.
# ifdef _MSC_VER
#  pragma warning(error : 4047)
# endif // ^^^ _MSC_VER ^^^
#endif // ^^^ definition of nullptr ^^^

// Define the meaning of a "byte". There exists difference between std::byte and unsigned char in C++.
// See: https://en.cppreference.com/cpp/types/byte. In C++, directly using std::byte is better.
#ifdef __cplusplus
  using selena_byte = std::byte;
#else // ^^^ C++ | C vvv
  typedef unsigned char selena_byte;
// ^^^ definition of byte ^^^
#endif

// Used for pointer- or reference-based function parameters only. A function parameter marked this implies that
// the parameter serves only as an input to the function. In no shape or form shall the function modify that which
// is marked with this macro.
// Example: static inline int find_max(SELENA_FUNCTION_IN const std::vector<int>& pVec_)
// Here, we are to just find the maximum element of the array. The array itself doesn't require modification.
#define SELENA_FUNCTION_IN

// Used for pointer- or reference-based function parameters only. A function parameter marked this implies that
// the parameter serves as an input to the function and will be modified. Common use-cases of this macro is in cases
// where an array is passed as an input, which will later be sorted down the line.
// Example: static inline void sort_vec(SELENA_FUNCTION_IN_MODIFIED std::vector<int>& pVec_)
// Here, we are to sort the vector. Obviously, it will get modified. 
#define SELENA_FUNCTION_IN_MODIFIED

// Used for pointer- or reference-based function parameters only. A function parameter marked this implies that
// the parameter serves not as a input, but rather as an output. It shall be overwritten by the function, which shall
// serve as the "returned" value of the function. Common use cases are when a C-style array is returned from a function.
// Example:
// static inline char** fizzBuzz(const size_t pN_ /* number of array elements */, SELENA_FUNCTION_OUT int* const pReturnSize_ /* size of returned array */)
// Here, the "pReturnSize_" parameter will hold the size of the returned array of C-style strings.
#define SELENA_FUNCTION_OUT

// Used for pointer- or reference-based function parameters only, that themselves are function pointers. A function parameter marked this implies that
// the parameter is a function pointer. It may or may not modify other parameters. If they do, those parameters themselves are to be marked
// "SELENA_FUNCTION_IN_MODIFIED" - the function pointer should strictly be marked with this macro. Common use-cases are for defining a C-style sort function
// which requires a "comparator" function as one of its parameters. Example:
// static inline void selena_max(
//   SELENA_FUNCTION_IN const void* const pArr_, /* ptr to array */
//   const size_t pArrSize_, /* array size */
//   const size_t pElemSize_, /* size of each element */
//   SELENA_FUNCTION_OUT const void** pReturnPtr_, /* will store the max element */
//   SELENA_FUNCTION_PTR int (*pCompare_)(const void* const, const void* const) /* ptr to comparator function */
//  )
// Finding a maximum element requires 1. read-only access to the array, and 2. a comparator function.
// Since the function won't modify the array, we mark the array with "SELENA_FUNCTION_IN", and the function pointer is marked "SELENA_FUNCTION_PTR".
// Had the function modified the array as well, we would have marked the array "SELENA_FUNCTION_IN_MODIFIED".
#define SELENA_FUNCTION_PTR

#endif // SELENA_C_BASE
