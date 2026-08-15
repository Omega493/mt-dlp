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

#ifndef SELENA_CONTAINERS_HASHER
#define SELENA_CONTAINERS_HASHER

#include <bit>
#include <string_view>

#include <cmath>
#include <cstdint>

namespace selena {

namespace hasher {

constexpr inline std::uint64_t FNV_OFFSET_BASIS{ 14695981039346656037ULL };
constexpr inline std::uint64_t FNV_PRIME{ 1099511628211ULL };

constexpr inline std::uint64_t SPLITMIX_ADDEND_64{ 11400714819323198485ULL };
constexpr inline std::uint64_t SPLITMIX_MULTIPLIER_64_1{ 13787848793156543929ULL };
constexpr inline std::uint64_t SPLITMIX_MULTIPLIER_64_2{ 10723151780598845931ULL };

constexpr inline std::uint32_t SPLITMIX_ADDEND_32{ 2654435769UL };
constexpr inline std::uint32_t SPLITMIX_MULTIPLIER_32_1{ 2246822507UL };
constexpr inline std::uint32_t SPLITMIX_MULTIPLIER_32_2{ 3266489909UL };

constexpr inline std::uint64_t KNUTH_MULTIPLIER_64{ 11400714785074694791ULL };
constexpr inline std::uint32_t KNUTH_MULTIPLIER_32{ 2654435769UL };

constexpr inline std::uint64_t CANONICAL_NAN_64{ 0x7FF8000000000000ULL };
constexpr inline std::uint32_t CANONICAL_NAN_32{ 0x7FC00000UL };

[[nodiscard]] constexpr std::uint64_t hash_splitmix64(const std::uint64_t value) {
  std::uint64_t x{ value + SPLITMIX_ADDEND_64 };
  x = (x ^ (x >> 30)) * SPLITMIX_MULTIPLIER_64_1;
  x = (x ^ (x >> 27)) * SPLITMIX_MULTIPLIER_64_2;

  return static_cast<std::uint64_t>(x ^ (x >> 31));
}

[[nodiscard]] constexpr std::uint64_t hash_splitmix32(const std::uint32_t value) {
  std::uint64_t x{ static_cast<std::uint64_t>(value) + SPLITMIX_ADDEND_32 };
  x = (x ^ (x >> 16)) * SPLITMIX_MULTIPLIER_32_1;
  x = (x ^ (x >> 13)) * SPLITMIX_MULTIPLIER_32_2;

  return static_cast<std::uint64_t>(x ^ (x >> 16));
}

[[nodiscard]] constexpr std::uint64_t hash_knuth64(const std::uint64_t value) {
  return static_cast<std::uint64_t>(value * KNUTH_MULTIPLIER_64);
}

[[nodiscard]] constexpr std::uint64_t hash_knuth32(const std::uint32_t value) {
  return static_cast<std::uint64_t>(value * KNUTH_MULTIPLIER_32);
}

// Internally calls hash_splitmix64() by casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_int64(const int64_t value) {
  return hash_splitmix64(static_cast<std::uint64_t>(value));
}

// Internally calls hash_splitmix32() by casting 'value' to std::uint32_t
[[nodiscard]] constexpr std::uint64_t hash_int32(const int32_t value) {
  return hash_splitmix32(static_cast<std::uint32_t>(value));
}

// Internally calls hash_splitmix64() by bit-casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_float64(const double value) {
  if (!value) {
    return hash_splitmix64(std::bit_cast<std::uint64_t>(0.0));
  }

  if (value != value) {
    return hash_splitmix64(CANONICAL_NAN_64);
  }

  return hash_splitmix64(std::bit_cast<std::uint64_t>(value));
}

// Internally calls hash_splitmix32() by bit-casting 'value' to std::uint32_t
[[nodiscard]] constexpr std::uint64_t hash_float32(const float value) {
  if (!value) {
    return hash_splitmix32(std::bit_cast<std::uint32_t>(0.0f));
  }

  if (value != value) {
    return hash_splitmix32(CANONICAL_NAN_32);
  }

  return hash_splitmix32(std::bit_cast<std::uint32_t>(value));
}

// Internally calls hash_knuth64() by casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_char(const char value) {
  return hash_knuth64(static_cast<std::uint64_t>(value));
}

// Internally calls hash_knuth64() by casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_wchar_t(const wchar_t value) {
  return hash_knuth64(static_cast<std::uint64_t>(value));
}

#if __cpp_char8_t
// Internally calls hash_knuth64() by casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_char8_t(const char8_t value) {
  return hash_knuth64(static_cast<std::uint64_t>(value));
}
#endif // ^^^ __cpp_char8_t ^^^

// Internally calls hash_knuth64() by casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_char16_t(const char16_t value) {
  return hash_knuth64(static_cast<std::uint64_t>(value));
}

// Internally calls hash_knuth64() by casting 'value' to std::uint64_t
[[nodiscard]] constexpr std::uint64_t hash_char32_t(const char32_t value) {
  return hash_knuth64(static_cast<std::uint64_t>(value));
}

[[nodiscard]] constexpr std::uint64_t hash_fnv1a(const std::string_view view_) {
  std::uint64_t hash_value_{ 14695981039346656037ULL };

  for (const char c_ : view_) {
    hash_value_ ^= static_cast<std::uint64_t>(c_);
    hash_value_ *= 1099511628211ULL;
  }

  return hash_value_;
}

template <typename T>
[[nodiscard]] constexpr std::uint64_t hash(const T& value) {
  using DecayedType = std::decay_t<T>;

  if constexpr (std::is_same_v<DecayedType, const char*> || std::is_same_v<DecayedType, char*>) {
    return hash_fnv1a(std::string_view{ value });
  } else if constexpr (std::is_same_v<DecayedType, std::string_view>) {
    return hash_fnv1a(value);
  } else if constexpr (std::is_same_v<DecayedType, bool>) {
    static_assert(!std::is_same_v<DecayedType, bool>, "selena::hasher::hash(): booleans are ignored and unsupported.");
    return 0;
  } else if constexpr (
    std::is_same_v<DecayedType, char>               ||
    std::is_same_v<DecayedType, wchar_t>            ||
#ifdef __cpp_char8_t
    std::is_same_v<DecayedType, char8_t>            ||
#endif // ^^^ __cpp_char8_t ^^^
    std::is_same_v<DecayedType, char16_t>           ||
    std::is_same_v<DecayedType, char32_t>
    ) {
    return hash_splitmix64(static_cast<std::uint64_t>(value));
  } else if constexpr (std::is_floating_point_v<DecayedType>) {
    if constexpr (sizeof(DecayedType) == 4) {
      return hash_float32(static_cast<float>(value));
    } else {
      return hash_float64(static_cast<double>(value));
    }
  } else if constexpr (std::is_integral_v<DecayedType>) {
    if constexpr (sizeof(DecayedType) <= 4) {
      if constexpr (std::is_signed_v<DecayedType>) {
        return hash_int32(static_cast<std::int32_t>(value));
      } else {
        return hash_splitmix32(static_cast<std::uint32_t>(value));
      }
    } else {
      if constexpr (std::is_signed_v<DecayedType>) {
        return hash_int64(static_cast<std::int64_t>(value));
      } else {
        return hash_splitmix64(static_cast<std::uint64_t>(value));
      }
    }
  } else {
    static_assert(
      std::is_integral_v<DecayedType>                ||
      std::is_floating_point_v<DecayedType>          ||
      std::is_same_v<DecayedType, const char*>       ||
      std::is_same_v<DecayedType, char*>,
      "selena::hasher::hash(): Unsupported type provided."
    );
    return 0;
  }
}

} // namespace hasher

} // namespace selena

#endif // SELENA_CONTAINERS_HASHER
