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

#ifndef SELENA_STRING_UTILS
#define SELENA_STRING_UTILS

#include <string_view>

#include <cctype>

namespace selena {

#define SELENA_STR_COLORS_RED_DARK       "\033[31m"
#define SELENA_STR_COLORS_RED_LIGHT      "\033[91m"
#define SELENA_STR_COLORS_YELLOW_DARK    "\033[33m"
#define SELENA_STR_COLORS_YELLOW_LIGHT   "\033[93m"
#define SELENA_STR_COLORS_CYAN_DARK      "\033[36m"
#define SELENA_STR_COLORS_CYAN_LIGHT     "\033[96m"
#define SELENA_STR_COLORS_BLUE_DARK      "\033[34m"
#define SELENA_STR_COLORS_BLUE_LIGHT     "\033[94m"
#define SELENA_STR_COLORS_GREEN_DARK     "\033[32m"
#define SELENA_STR_COLORS_GREEN_LIGHT    "\033[92m"
#define SELENA_STR_COLORS_RESET          "\033[0m"

/*
 * Does a case-insensitive comparison of two characters.
 * Especially useful in algorithms like `std::search`.
 * @param c1 An unsigned char
 * @param c2 Another unsigned char
 * @returns true/false
 */
[[nodiscard]] inline bool iequal(const unsigned char c1, const unsigned char c2) {
  return std::tolower(c1) == std::tolower(c2);
}

/*
 * Checks if a target string is a part of a given text.
 * Makes use of `selena::iequal`.
 * @param text The base text
 * @param target The text to search for
 * @returns true/false
 */
[[nodiscard]] inline bool icontains(const std::string_view text, const std::string_view target) {
  const std::string_view::const_iterator it{ std::search(
    text.begin(), text.end(),
    target.begin(), target.end(),
    selena::iequal
  ) };

  return it != text.end();
}

} // namespace selena

#endif // SELENA_STRING_UTILS
