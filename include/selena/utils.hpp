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

#ifndef SELENA_UTILS_HPP
#define SELENA_UTILS_HPP

#include <string>
#include <string_view>
#include <regex>
#include <algorithm>

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "selena/string_utils.hpp"

namespace selena {
/*
 * Uses <regex> to match a given input string to a given pattern.
 * Uses std::string strings for the regex pattern.
 * @param input A string object
 * @param re_pattern A string object
 * @return true/false
 */
[[nodiscard]] inline bool is_valid_format(const std::string& input, const std::string& re_pattern) {
  return std::regex_match(input, std::regex{ re_pattern });
}

/*
 * Uses <regex> to match a given input string to a given pattern.
 * @param input A string object
 * @param re_pattern A regex object
 * @return true/false
 */
[[nodiscard]] inline bool is_valid_format(const std::string& input, const std::regex& re_pattern) {
  return std::regex_match(input, re_pattern);
}

/*
 * Evaluates the validity of an URL w/o using <regex>
 * Allows only http/https schemes. Blocks characters such as ';', '|', '`' and '$'.
 * Also blocks characters which don't have a visible representation, this includes spaces.
 * As for the meaning of "validity", it only checks if the string "looks" like an URL.
 * It doesn't not verify the actual existence of the URL.
 * @param url A string object
 * @return true/false
 */
[[nodiscard]] inline bool is_valid_url(const std::string& url) {
  if (url.empty()) {
    return false;
  }

  if (!selena::iequal(static_cast<unsigned char>(url[0]), 'h')) { // http/https restriction
    return false;
  }

  const size_t sep_pos{ url.find("://") };
  if (sep_pos == std::string::npos) {
    return false;
  }

  if (sep_pos != 4 && sep_pos != 5) { // http/https restriction
    return false;
  }

  {
    // Artificial scoping to prevent pollution
    // Though this is not absolutely required
    const char* const BASE_SCHEME{ "http" };
    for (size_t i{ 1 }; i < 4; ++i) { // Start from '1' bcs 'h' is alrd checked
      if (!selena::iequal(static_cast<unsigned char>(url[i]), BASE_SCHEME[i])) {
        return false;
      }
  }
  }

  if (sep_pos == 5) {
    if (!selena::iequal(static_cast<unsigned char>(url[4]), 's')) {
      return false;
    }
  }

  if ((sep_pos + 3) >= url.length()) {
    return false; // + 3 because "://"
  }

  for (size_t i{ sep_pos + 3 }; i < url.length(); ++i) { // +3 because "://"
    const unsigned char c{ static_cast<unsigned char>(url[i]) };
    if (c == ';' || c == '|' || c == '`' || c == '$') {
      return false;
    }

    if (!std::isgraph(c)) {
      return false; // Blocks anything that doesn't have a graphical representation
    }
  }

  return true;
}

/*
 * Finds the value corresponding to a given environment variable.
 * @param var_name A const char* to a C-style string
 * @return std::string A string object containing the returned value
 */
inline std::string getenv(const char* const var_name) {
  if (!var_name) {
    return "";
  }

  const char* const var{ std::getenv(var_name) };
  return var ? var : "";
}

/*
 * Performs an unsuppressed system call. For suppressed system calls, use "system_suppressed()"
 * Note: if the command given is "clear", it switches to "cls" on Windows.
 * For every other command, it performs the command as is.
 * Note-2: The function's return type is declared as [[nodiscard]].
 * If you feel like discarding, cast it to void, or something else.
 * It is however a good practice to always check the return code of commands.
 * Note-3: It is blind - it does NOT check for "dangerous" commands.
 * @param cmd A C-style string which specifies the command
 * @returns int The value returned from the system call
 */
[[nodiscard]] inline int system(const char* const cmd) {
  if (!cmd) {
    return -1;
  }
#ifdef _WIN32
  if (!std::strcmp(cmd, "clear")) {
    return std::system("cls");
  }
#endif // _WIN32
  return std::system(cmd);
}

/*
 * Performs a suppressed system call. For unsuppressed system calls, use "system()"
 * either from "std" or here. Note: If the command given is "clear", it performs "cls"
 * on Windows and "clear" on Unix - it doesn't supress anything, simply because there is
 * nothing to suppress. Every other command given is suppressed. Note-2: The function's return
 * type is declared [[nodiscard]]. If you feel like discarding, cast it to void, or something else.
 * It is however a good practice to always check the return code of commands.
 * Note-3: It is blind - it does NOT check for "dangerous" commands.
 * @param cmd A C-style string which specifies the command
 * @returns int The value returned from the system call
 */
[[nodiscard]] inline int system_suppressed(const char* const cmd) {
  if (!cmd) {
    return 1;
  }
  if (!std::strcmp(cmd, "clear")) {
#ifdef _WIN32
    return std::system("cls");
#else // ^^^ _WIN32 || !_WIN32 vvv
    return std::system("clear");
#endif // _WIN32
  }

#ifdef _WIN32
  const std::string suppressed_cmd{ cmd + std::string{" > NUL 2>&1"} };
#else // ^^^ _WIN32 || !_WIN32 vvv
  const std::string suppressed_cmd{ cmd + std::string{" > /dev/null 2>&1"} };
#endif // _WIN32
  return std::system(suppressed_cmd.c_str());
}
} // namespace selena

#endif // SELENA_UTILS_HPP
