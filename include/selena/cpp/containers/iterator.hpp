// containers/iterator.hpp

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

#ifndef SELENA_CONTAINERS_ITERATOR
#define SELENA_CONTAINERS_ITERATOR

#include <utility>

#include <cstddef>

namespace selena {

template <typename MapT_, typename KeyRefT_, typename ValRefT_, typename ArrowProxyT_>
class map_iterator {
public:
  map_iterator(MapT_* map_ptr_, const size_t idx_) noexcept : map_ptr_{ map_ptr_ }, idx_{ idx_ } {}

  constexpr std::pair<KeyRefT_, ValRefT_> operator*() const noexcept {
    return std::pair<KeyRefT_, ValRefT_>{ map_ptr_->keys_[idx_], map_ptr_->vals_[idx_] };
  }

  constexpr ArrowProxyT_ operator->() const noexcept {
    return ArrowProxyT_{ map_ptr_->keys_[idx_], map_ptr_->vals_[idx_] };
  }

  map_iterator& operator++() noexcept {
    ++idx_;

    for (; idx_ < MapT_::capacity_;) {
      if (map_ptr_->occupied_flags_[idx_]) {
        break;
      }

      ++idx_;
    }

    return *this;
  }

  map_iterator operator++(int) noexcept {
    map_iterator old_{ *this };
    ++(*this);
    return old_;
  }

  constexpr bool operator==(const map_iterator& other_) const noexcept {
    return idx_ == other_.idx_;
  }

  constexpr bool operator!=(const map_iterator& other_) const noexcept {
    return idx_ != other_.idx_;
  }

  constexpr size_t get_index() const noexcept {
    return idx_;
  }

private:
  MapT_* map_ptr_;
  size_t idx_;
}; // class map_iterator

template <typename MapT_, typename KeyRefT_, typename ValConstRefT_, typename ConstArrowProxyT_>
class const_map_iterator {
public:
  const_map_iterator(const MapT_* const map_ptr_, const size_t idx_) noexcept : map_ptr_{ map_ptr_ }, idx_{ idx_ } {}

  constexpr std::pair<KeyRefT_, ValConstRefT_> operator*() const noexcept {
    return std::pair<KeyRefT_, ValConstRefT_>{ map_ptr_->keys_[idx_], map_ptr_->vals_[idx_] };
  }

  constexpr ConstArrowProxyT_ operator->() const noexcept {
    return ConstArrowProxyT_{ map_ptr_->keys_[idx_], map_ptr_->vals_[idx_] };
  }

  const_map_iterator& operator++() noexcept {
    ++idx_;

    for (; idx_ < MapT_::capacity_;) {
      if (map_ptr_->occupied_flags_[idx_]) {
        break;
      }

      ++idx_;
    }

    return *this;
  }

  const_map_iterator operator++(int) noexcept {
    const_map_iterator old_{ *this };
    ++(*this);
    return old_;
  }

  constexpr bool operator==(const const_map_iterator& other_) const noexcept {
    return idx_ == other_.idx_;
  }

  constexpr bool operator!=(const const_map_iterator& other_) const noexcept {
    return idx_ != other_.idx_;
  }

  constexpr size_t get_index() const noexcept {
    return idx_;
  }

private:
  const MapT_* map_ptr_;
  size_t idx_;
}; // class const_map_iterator

template <typename SetT_, typename KeyConstRefT_, typename KeyConstPtrT_>
class set_iterator {
public:
  set_iterator(SetT_* set_ptr_, const size_t idx_) noexcept : set_ptr_{ set_ptr_ }, idx_{ idx_ } {}

  constexpr KeyConstRefT_ operator*() const noexcept {
    return set_ptr_->keys_[idx_];
  }

  constexpr KeyConstPtrT_ operator->() const noexcept {
    return &set_ptr_->keys_[idx_];
  }

  set_iterator& operator++() noexcept {
    ++idx_;

    for (; idx_ < SetT_::capacity_;) {
      if (set_ptr_->occupied_flags_[idx_]) {
        break;
      }

      ++idx_;
    }

    return *this;
  }

  set_iterator operator++(int) noexcept {
    set_iterator old_{ *this };
    ++(*this);
    return old_;
  }

  constexpr bool operator==(const set_iterator& other_) const noexcept {
    return idx_ == other_.idx_;
  }

  constexpr bool operator!=(const set_iterator& other_) const noexcept {
    return idx_ != other_.idx_;
  }

  constexpr size_t get_index() const noexcept {
    return idx_;
  }

private:
  SetT_* set_ptr_;
  size_t idx_;
}; // class set_iterator

} // namespace selena

#endif // SELENA_CONTAINERS_ITERATOR
