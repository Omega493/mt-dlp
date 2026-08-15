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

#ifndef SELENA_CONTAINERS_INTERNALS
#define SELENA_CONTAINERS_INTERNALS

#include <algorithm>
#include <type_traits>
#include <utility>

#include <cstdint>

namespace selena {

namespace internals {

template <typename KeyT_, typename ValRefT_>
class arrow_proxy_ {
public:
  constexpr arrow_proxy_(const KeyT_& k_, ValRefT_ v_) noexcept : pair_{ k_, v_ } {}

  constexpr std::pair<const KeyT_&, ValRefT_>* operator->() noexcept {
    return &pair_;
  }

private:
  std::pair<const KeyT_&, ValRefT_> pair_;
}; // class arrow_proxy_

template <typename KeyT_, typename ValConstRefT_>
class const_arrow_proxy_ {
public:
  constexpr const_arrow_proxy_(const KeyT_& k_, ValConstRefT_ v_) noexcept : pair_{ k_, v_ } {}

  constexpr const std::pair<const KeyT_&, ValConstRefT_>* operator->() const noexcept {
    return &pair_;
  }

private:
  std::pair<const KeyT_&, ValConstRefT_> pair_;
}; // class const_arrow_proxy_

template <typename T_>
concept ValidHashKey_ =
(std::is_integral_v<T_> && !std::is_same_v<std::remove_cv_t<T_>, bool>) ||
std::is_floating_point_v<T_> ||
(std::is_pointer_v<T_> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T_>>, char>) ||
std::is_same_v<std::remove_cv_t<T_>, std::string_view>;

} // namespace internals

} // namespace selena

#endif // SELENA_CONTAINERS_INTERNALS
