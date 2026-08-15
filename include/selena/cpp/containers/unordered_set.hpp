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

#ifndef SELENA_UNORDERED_SET
#define SELENA_UNORDERED_SET

#include <array>
#include <bit>
#include <bitset>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cmath>
#include <cstdint>
#include <cstddef>

#include "selena/cpp/containers/internals.hpp"
#include "selena/cpp/containers/iterator.hpp"
#include "selena/cpp/hasher.hpp"

namespace selena {

enum class us_size_of_args {
  keys_array,
  boolean_array,
};

template<internals::ValidHashKey_ KeyT_, std::size_t max_elems_, long double load_factor_>
class unordered_set {
private:
  static constexpr inline long double l_factor_{ []() -> long double {
    if (load_factor_ <= 0 || load_factor_ > 0.85) {
      return 0.35L;
    }

    return load_factor_;
  }() };

  static constexpr inline std::size_t capacity_{ std::bit_ceil(static_cast<std::size_t>(max_elems_ / l_factor_)) };

  template <typename SetT_, typename KeyConstRefT_, typename KeyConstPtrT_>
  friend class set_iterator;

public:
  using iterator = set_iterator<unordered_set, const KeyT_&, const KeyT_*>;
  using const_iterator = set_iterator<const unordered_set, const KeyT_&, const KeyT_*>;

  constexpr unordered_set()
    noexcept(std::is_nothrow_default_constructible_v<std::array<KeyT_, capacity_>>)
  {
    if (std::is_constant_evaluated()) {
      keys_.fill(KeyT_{});
    }
  }

  constexpr unordered_set(std::initializer_list<KeyT_> init_list_) {
    if (max_elems_ < init_list_.size()) {
      throw std::length_error("too many initializer values");
    }

    if (std::is_constant_evaluated()) {
      keys_.fill(KeyT_{});
    }

    for (const KeyT_& key_ : init_list_) {
      const std::size_t target_idx_{ find_index_(key_) };

      if (!occupied_flags_[target_idx_]) {
        keys_[target_idx_] = key_;
        occupied_flags_.set(target_idx_);
        ++sz_;
      }
    }
  }

  ~unordered_set() noexcept = default;

  constexpr unordered_set(const unordered_set&)
    noexcept(std::is_nothrow_copy_constructible_v<std::array<KeyT_, capacity_>>) = default;

  constexpr unordered_set& operator=(const unordered_set&)
    noexcept(std::is_nothrow_copy_assignable_v<std::array<KeyT_, capacity_>>) = default;

  constexpr unordered_set(unordered_set&& other_) noexcept
    : sz_{ other_.sz_ }, keys_{ std::move(other_.keys_) }, occupied_flags_{ std::move(other_.occupied_flags_) }
  {
    other_.sz_ = 0;
    other_.occupied_flags_.reset();
  }

  constexpr unordered_set& operator=(unordered_set&& other_) noexcept {
    if (this == &other_) {
      return *this;
    }

    std::swap(sz_, other_.sz_);
    std::swap(keys_, other_.keys_);
    std::swap(occupied_flags_, other_.occupied_flags_);

    return *this;
  }

  // Converting constructor. Creates a hash set out of a predefined array of keys. "Predifined array of keys" can be a C-style array,
  // a std::array, a std::vector, etc.
  constexpr explicit unordered_set(std::span<const KeyT_> span_) {
    if (max_elems_ < span_.size()) {
      throw std::length_error("continuous container exceeds maximum size limits");
    }

    if (std::is_constant_evaluated()) {
      keys_.fill(KeyT_{});
    }

    for (const KeyT_& key_ : span_) {
      const std::size_t target_idx_{ find_index_(key_) };

      if (!occupied_flags_[target_idx_]) {
        keys_[target_idx_] = key_;
        occupied_flags_.set(target_idx_);
        ++sz_;
      }
    }
  }

  // Conversion assignment. Clears out the unordered set and creates a hash set in-place out of the given array of keys. "Given array of keys" can be a C-style array,
  // a std::array, a std::vector, etc.
  constexpr unordered_set& operator=(std::span<const KeyT_> span_) {
    if (max_elems_ < span_.size()) {
      throw std::length_error{ "continuous container size exceeds maximum limits" };
    }

    this->clear();

    for (const KeyT_& key_ : span_) {
      const std::size_t target_idx_{ find_index_(key_) };

      if (!occupied_flags_[target_idx_]) {
        keys_[target_idx_] = key_;
        occupied_flags_.set(target_idx_);
        ++sz_;
      }
    }

    return *this;
  }

  constexpr std::pair<iterator, bool> insert(const KeyT_& value_) {
    const std::size_t curr_idx_{ find_index_(value_) };

    if (occupied_flags_[curr_idx_]) {
      return std::pair<iterator, bool>{ iterator{ this, curr_idx_ }, false };
    }

    if (sz_ == max_elems_) {
      throw std::out_of_range("container is at its limits");
    }

    keys_[curr_idx_] = value_;
    occupied_flags_.set(curr_idx_);
    tombstones_.reset(curr_idx_);
    ++sz_;

    return std::pair<iterator, bool>{ iterator{ this, curr_idx_ }, true };
  }

  template <typename... Args_>
  constexpr std::pair<iterator, bool> emplace(Args_&&... args_) {
    KeyT_ temp_val_{ std::forward<Args_>(args_)... };
    const std::size_t curr_idx_{ find_index_(temp_val_) };

    if (occupied_flags_[curr_idx_]) {
      return std::pair<iterator, bool>{ iterator{ this, curr_idx_ }, false };
    }

    if (sz_ == max_elems_) {
      throw std::out_of_range("container is at its limits");
    }

    keys_[curr_idx_] = std::move(temp_val_);
    occupied_flags_.set(curr_idx_);
    tombstones_.reset(curr_idx_);
    ++sz_;

    return std::pair<iterator, bool>{ iterator{ this, curr_idx_ }, true };
  }

  constexpr std::size_t erase(const KeyT_& key_) noexcept(std::is_nothrow_move_assignable_v<KeyT_>) {
    const std::size_t hash_{ static_cast<std::size_t>(selena::hasher::hash(key_)) };
    std::size_t curr_idx_{ hash_ & mask_ };

    for (; occupied_flags_[curr_idx_] || tombstones_[curr_idx_];) {
      if (occupied_flags_[curr_idx_]) {
        if (keys_[curr_idx_] == key_) {
          this->erase_at_(curr_idx_);
          return 1;
        }
      }

      curr_idx_ = (curr_idx_ + 1) & mask_;
    }

    return 0;
  }

  constexpr iterator erase(iterator pos_) noexcept(std::is_nothrow_move_assignable_v<KeyT_>) {
    const std::size_t org_idx_{ pos_.get_index() };
    this->erase_at_(org_idx_);

    std::size_t next_idx_{ org_idx_ };

    for (; next_idx_ < capacity_;) {
      if (occupied_flags_[next_idx_]) {
        break;
      }

      ++next_idx_;
    }

    return iterator{ this, next_idx_ };
  }

  constexpr iterator erase(const_iterator pos_) noexcept(std::is_nothrow_move_assignable_v<KeyT_>) {
    const std::size_t original_idx_{ pos_.get_index() };
    this->erase_at_(original_idx_);

    std::size_t next_idx_{ original_idx_ };

    for (; next_idx_ < capacity_;) {
      if (occupied_flags_[next_idx_]) {
        break;
      }

      ++next_idx_;
    }

    return iterator{ this, next_idx_ };
  }

  constexpr iterator erase(const_iterator first_, const_iterator last_) noexcept(std::is_nothrow_move_assignable_v<KeyT_>) {
    if (first_ == last_) {
      return iterator{ this, last_.get_index() };
    }

    std::size_t curr_idx_{ last_.get_index() };

    do {
      --curr_idx_;

      if (occupied_flags_[curr_idx_]) {
        this->erase_at_(curr_idx_);
      }

    } while (curr_idx_ != first_.get_index());

    std::size_t next_idx_{ first_.get_index() };

    for (; next_idx_ < capacity_;) {
      if (occupied_flags_[next_idx_]) {
        break;
      }

      ++next_idx_;
    }

    return iterator{ this, next_idx_ };
  }

  // Performing a full clear (calling std::memset with 0) will take too much time.
  // So, performs a logical erasure by resetting the set's internal tracking data.
  // The user data (the data contained in the std::arrays) still exist within memory, and
  // they are overwritten in subsequent writes.
  constexpr void clear() noexcept {
    sz_ = 0;
    occupied_flags_.reset();
    tombstones_.reset();
  }

  [[nodiscard]] constexpr iterator begin() noexcept {
    for (std::size_t i_{}; i_ < capacity_; ++i_) {
      if (occupied_flags_[i_]) {
        return iterator{ this, i_ };
      }
    }

    return this->end();
  }
  
  [[nodiscard]] constexpr iterator cbegin() noexcept {
    return begin();
  }

  [[nodiscard]] constexpr const_iterator begin() const noexcept {
    for (std::size_t i_{}; i_ < capacity_; ++i_) {
      if (occupied_flags_[i_]) {
        return const_iterator{ this, i_ };
      }
    }

    return this->end();
  }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
    return begin();
  }

  [[nodiscard]] constexpr iterator end() noexcept {
    return iterator{ this, capacity_ };
  }

  [[nodiscard]] constexpr iterator cend() noexcept {
    return end();
  }

  [[nodiscard]] constexpr const_iterator end() const noexcept {
    return const_iterator{ this, capacity_ };
  }

  [[nodiscard]] constexpr const_iterator cend() const noexcept {
    return end();
  }

  [[nodiscard]] constexpr iterator find(const KeyT_& key_) noexcept {
    const std::size_t hash_{ static_cast<std::size_t>(selena::hasher::hash(key_)) };
    std::size_t curr_idx_{ hash_ & mask_ };

    for (; occupied_flags_[curr_idx_] || tombstones_[curr_idx_];) {
      if (occupied_flags_[curr_idx_]) {
        if (keys_[curr_idx_] == key_) {
          return iterator{ this, curr_idx_ };
        }
      }

      curr_idx_ = (curr_idx_ + 1) & mask_;
    }

    return this->end();
  }

  [[nodiscard]] constexpr const_iterator find(const KeyT_& key_) const noexcept {
    const std::size_t hash_{ static_cast<std::size_t>(selena::hasher::hash(key_)) };
    std::size_t curr_idx_{ hash_ & mask_ };

    for (; occupied_flags_[curr_idx_] || tombstones_[curr_idx_];) {
      if (occupied_flags_[curr_idx_]) {
        if (keys_[curr_idx_] == key_) {
          return const_iterator{ this, curr_idx_ };
        }
      }

      curr_idx_ = (curr_idx_ + 1) & mask_;
    }

    return this->end();
  }

  [[nodiscard]] constexpr std::size_t count(const KeyT_& key_) const noexcept {
    return find(key_) != end();
  }

  [[nodiscard]] constexpr bool contains(const KeyT_& key_) const noexcept {
    return find(key_) != end();
  }

  // Returns the size of the container at the moment of invocation.
  [[nodiscard]] constexpr std::size_t size() const noexcept {
    return sz_;
  }

  // Returns the 'max_elements' template parameter created when instantiating a data member using this class.
  [[nodiscard]] constexpr std::size_t max_size() const noexcept {
    return max_elems_;
  }

  // Returns the capacity of the underlying arrays. This is equivalent to max elements / load factor rounded up to the nearest power of 2.
  [[nodiscard]] constexpr std::size_t capacity() const noexcept {
    return capacity_;
  }

  [[nodiscard]] constexpr long double max_load_factor() const noexcept {
    return l_factor_;
  }

  [[nodiscard]] constexpr long double current_load_factor() const noexcept {
    return static_cast<long double>(sz_) / static_cast<long double>(capacity_);
  }

  [[nodiscard]] constexpr std::size_t size_of(const us_size_of_args arg_) const noexcept {
    switch (arg_) {
      case us_size_of_args::keys_array: {
        return sizeof(keys_);
      }
      case us_size_of_args::boolean_array: {
        return sizeof(occupied_flags_);
      }
      default: {
        // to satisfy compiler requirement :>
        // realistically, this serves no purpose: us_size_of_args only has 2 possible values,
        // all of which are alrd handled
        return size();
      }
    }
  }

  // Returns whether the container is empty. Does NOT empty the container.
  // If you want to empty the container, use clear() instead.
  [[nodiscard]] constexpr bool empty() const noexcept {
    return !sz_;
  }

private:
  static constexpr std::size_t mask_{ capacity_ - 1 };
  std::size_t sz_{};
  std::array<KeyT_, capacity_> keys_;
  std::bitset<capacity_> occupied_flags_{};
  std::bitset<capacity_> tombstones_{};
  
  constexpr std::size_t find_index_(const KeyT_& key_) const noexcept {
    const std::size_t hash_{ static_cast<std::size_t>(selena::hasher::hash(key_)) };
    std::size_t curr_idx_{ hash_ & mask_ };
    std::size_t first_tombstone_{ capacity_ };

    for (; occupied_flags_[curr_idx_] || tombstones_[curr_idx_];) {
      if (occupied_flags_[curr_idx_]) {
        if (keys_[curr_idx_] == key_) {
          return curr_idx_;
        }
      } else {
        if (first_tombstone_ == capacity_) {
          first_tombstone_ = curr_idx_;
        }
      }

      curr_idx_ = (curr_idx_ + 1) & mask_;
    }

    if (first_tombstone_ != capacity_) {
      return first_tombstone_;
    }

    return curr_idx_;
  }

  constexpr void erase_at_(const std::size_t target_idx_) noexcept {
    occupied_flags_.reset(target_idx_);
    tombstones_.set(target_idx_);
    --sz_;
  }
}; // class unordered_set

} // namespace selena

#endif // SELENA_UNORDERED_SET
