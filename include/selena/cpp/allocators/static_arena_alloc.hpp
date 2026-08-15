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

#ifndef SELENA_STATIC_ARENA_ALLOC
#define SELENA_STATIC_ARENA_ALLOC

#ifdef SELENA_ARENA_ALLOC
# error "Please use either selena::arena_alloc or selena::static_arena_alloc
#endif // ^^^ SELENA_ARENA_ALLOC ^^^

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <process.h>
# include <Windows.h>
 // For VirtualAlloc2 and VirtualFreeEx
# pragma comment(lib, "mincore")
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
# include <sys/mman.h>
# include <unistd.h>
#endif // ^^^ _WIN32 ^^^

#include <atomic>
#include <bit>
#include <mutex>
#include <new>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "selena/c/base.h"

namespace selena {

class static_arena_alloc {
public:

  /*
   * @brief Allocates a region of memory that is at least as large as the requested size.
   * @returns A pointer to the start of the allocated memory region.
   */
  [[nodiscard]] static void* alloc(
    const std::size_t pSize_,
    const std::align_val_t pAlignment_ = static_cast<std::align_val_t>(alignof(std::max_align_t))
  ) noexcept {
    const std::size_t align_val{ static_cast<std::size_t>(pAlignment_) };

    // Required size
    const std::size_t req_size{ pSize_ + align_val + sizeof(allocator_header_t) };

    std::scoped_lock lock{ state_.mtx_ };

    // If required size is less than or equal to a page size, allocate it inside a page,
    // or create a new page.
    if (req_size <= get_page_size()) {
      const std::size_t class_idx{ get_class_idx(req_size) };

      if (state_.free_lists_[class_idx]) {
        free_node_t* node{ state_.free_lists_[class_idx] };
        state_.free_lists_[class_idx] = node->next_;

        /*
         * -------------------------------------------------------------------...
         * |                          |                      |
         * |    Alignment Padding     |   allocation header  |  actual data
         * | 0 to align_val - 1 bytes |                      |
         * |                          |                      |
         * -------------------------------------------------------------------...
         * ^                          ^                      ^
         * |                          |                      |
         * base pointer      header starts here       data starts here
         *
         * creating ascii art is :catDespair:
         */

         // Start of memory.
        std::byte* base_ptr{ reinterpret_cast<std::byte*>(node) };

        // Move past the header and align it to the next multiple of alignment.
        std::byte* data_ptr{
          reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(base_ptr) + sizeof(allocator_header_t) + align_val - 1) & ~(align_val - 1))
        };

        // Move back up and store header.
        allocator_header_t* header{ reinterpret_cast<allocator_header_t*>(data_ptr - sizeof(allocator_header_t)) };
        header->size_ = get_chunk_size(class_idx);
        header->padding_offs_ = static_cast<std::size_t>(data_ptr - base_ptr);

        return data_ptr;
      }

      // Triggered if free lists didn't have a page.

      void* page_ptr{ alloc_pages(1) };

      if (!page_ptr) {
        return nullptr;
      }

      const std::size_t chunk_size{ get_chunk_size(class_idx) };
      const std::size_t num_chunks{ get_page_size() / chunk_size };

      // The base pointer of the newly allocated page.
      std::byte* byte_ptr{ static_cast<std::byte*>(page_ptr) };

      for (std::size_t i{}; i < num_chunks; ++i) {
        // Add the address of the base pointer to the offset of current chunk. Then, convert it
        // into a node pointer.
        free_node_t* node{ reinterpret_cast<free_node_t*>(byte_ptr + (i * chunk_size)) };

        // A push front operation of a LIFO stack.
        node->next_ = state_.free_lists_[class_idx];
        state_.free_lists_[class_idx] = node;
      }

      free_node_t* node{ state_.free_lists_[class_idx] };

      // This is to silence the "Dereferencing NULL pointer. 'node' contains the same NULL value as 'state_.free_lists_[class_idx]' did."
      // warning from MSVC. Without this line, the compiler treats the return value of get_page_size as an arbitrary size_t.
      // For the num_chunks calc. MSVC assumes that this division could potentially evaluate to 0. If this were 0, the
      // for loop would skip entirely, leaving the free lists in nullptr. Immediately after this loop, we dereference the node
      // (via node->next_), which triggers a nullptr derefence warning.
      // In reality however, num_chunk will always be at least 1. The code block is guarded by if (req_size <= get_page_size()),
      // and the get_chunk_size function will round the requested size up to the nearest pre-defined bucket, which maxes out
      // exactly at the page size. Thus, num_chunks will always be >= 1.
      if (!node) {
        return nullptr;
      }

      state_.free_lists_[class_idx] = node->next_;

      std::byte* base_ptr{ reinterpret_cast<std::byte*>(node) };
      std::byte* data_ptr{
        reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(base_ptr) + sizeof(allocator_header_t) + align_val - 1) & ~(align_val - 1))
      };

      allocator_header_t* header{ reinterpret_cast<allocator_header_t*>(data_ptr - sizeof(allocator_header_t)) };
      header->size_ = chunk_size;
      header->padding_offs_ = static_cast<std::size_t>(data_ptr - base_ptr);

      return data_ptr;
    }

    // If required size is larger than 2 MB, allocate directly from OS.
    if (req_size > (kArenaSize - get_page_size())) {
      // Actual required size: size asked by user + allocation header + large allocs header.
      const std::size_t new_req_size{ req_size + sizeof(large_allocs_t) };
      void* os_ptr{ reserve_os(new_req_size) };

      if (!os_ptr) {
        return nullptr;
      }

      if (!commit_os(os_ptr, new_req_size)) {
        if (!release_os(os_ptr, new_req_size)) {
          // Memory leak!
        }

        return nullptr;
      }

      large_allocs_t* node{ new(os_ptr) large_allocs_t() };
      node->size_ = new_req_size;
      node->npx_ = xor_ptrs<large_allocs_t>(nullptr, state_.large_allocs_head_);

      if (state_.large_allocs_head_) {
        state_.large_allocs_head_->npx_ = xor_ptrs<large_allocs_t>(node, xor_ptrs<large_allocs_t>(nullptr, state_.large_allocs_head_->npx_));
      }

      state_.large_allocs_head_ = node;

      std::byte* base_ptr{ static_cast<std::byte*>(os_ptr) + sizeof(large_allocs_t) };
      std::byte* data_ptr{
        reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(base_ptr) + sizeof(allocator_header_t) + align_val - 1) & ~(align_val - 1))
      };

      allocator_header_t* header{ reinterpret_cast<allocator_header_t*>(data_ptr - sizeof(allocator_header_t)) };
      header->size_ = req_size;
      header->padding_offs_ = static_cast<std::size_t>(data_ptr - base_ptr);

      return data_ptr;
    }

    // Ceiling division. Adding page size, we guarantee any fractional divison would go to the
    // nearest integer towards infinity (ceiling).
    const std::size_t pages_req{ (req_size + get_page_size() - 1) >> std::countr_zero(get_page_size()) };
    void* multi_page_ptr{ alloc_pages(pages_req) };

    if (!multi_page_ptr) {
      return nullptr;
    }

    std::byte* base_ptr{ static_cast<std::byte*>(multi_page_ptr) };
    std::byte* data_ptr{
      reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(base_ptr) + sizeof(allocator_header_t) + align_val - 1) & ~(align_val - 1))
    };

    allocator_header_t* header{ reinterpret_cast<allocator_header_t*>(data_ptr - sizeof(allocator_header_t)) };
    header->size_ = pages_req << std::countr_zero(get_page_size());
    header->padding_offs_ = static_cast<std::size_t>(data_ptr - base_ptr);

    return data_ptr;
  }

  /*
   * @brief Frees a region of memory starting at given pointer.
   * @return Nothing.
   */
  static void free(
    SELENA_FUNCTION_IN_MODIFIED void* pPtr_
  ) noexcept {
    if (!pPtr_) {
      return;
    }

    std::scoped_lock lock{ state_.mtx_ };

    std::byte* data_ptr{ reinterpret_cast<std::byte*>(pPtr_) };
    allocator_header_t* header{ reinterpret_cast<allocator_header_t*>(data_ptr - sizeof(allocator_header_t)) };
    std::byte* base_ptr{ data_ptr - header->padding_offs_ };

    if (header->size_ <= get_page_size()) {
      const std::size_t class_idx{ get_class_idx(header->size_) };

      free_node_t* node{ reinterpret_cast<free_node_t*>(base_ptr) };
      node->next_ = state_.free_lists_[class_idx];
      state_.free_lists_[class_idx] = node;

      return;
    }

    if (header->size_ > (kArenaSize - get_page_size())) {
      std::byte* os_ptr{ base_ptr - sizeof(large_allocs_t) };
      large_allocs_t* node{ reinterpret_cast<large_allocs_t*>(os_ptr) };

      large_allocs_t* curr{ state_.large_allocs_head_ }, * prev{ nullptr };

      while (curr) {
        large_allocs_t* next{ xor_ptrs(prev, curr->npx_) };

        if (curr == node) {
          if (prev) {
            prev->npx_ = xor_ptrs<large_allocs_t>(xor_ptrs<large_allocs_t>(prev->npx_, curr), next);
          } else {
            state_.large_allocs_head_ = next;
          }

          if (next) {
            next->npx_ = xor_ptrs<large_allocs_t>(prev, xor_ptrs<large_allocs_t>(curr, next->npx_));
          }

          break;
        }

        prev = curr;
        curr = next;
      }

      // If this succeeds, OK. If this fails, memor leak!
      static_cast<void>(release_os(os_ptr, node->size_));
      return;
    }

    free_pages(base_ptr, header->size_ >> std::countr_zero(get_page_size()));
  }

  [[nodiscard]] static inline std::size_t get_page_size() noexcept {
    if (!page_size_) {
#ifdef _WIN32
      SYSTEM_INFO temp_;
      GetSystemInfo(&temp_);
      page_size_ = static_cast<std::size_t>(temp_.dwPageSize);
#elifdef __linux__ // ^^^ _WIN32 / !_WIN32 vvv
      page_size_ = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif // ^^^ _WIN32 ^^^
    }

    return page_size_;
  }

  static constexpr inline std::size_t kArenaSize{ 1ULL << 21 };

private:
  struct alignas(std::max_align_t) allocator_header_t {
    std::size_t size_;
    std::size_t padding_offs_;
  };

  struct free_node_t {
    free_node_t* next_;
  };

  struct arena_t {
    // A bitset of 512 bits
    std::uint64_t committed_pages_[8];

    arena_t* npx_;
  };

  struct large_allocs_t {
    large_allocs_t* npx_;
    std::size_t size_;
  };

  struct allocator_state_t {
    free_node_t* free_lists_[56];
    std::mutex mtx_;
    arena_t* arena_head_;
    large_allocs_t* large_allocs_head_;
  };

  static inline allocator_state_t state_{};
  static inline std::size_t page_size_;

  template <typename T_>
  [[nodiscard]] static inline T_* xor_ptrs(
    SELENA_FUNCTION_IN const T_* const pNext_,
    SELENA_FUNCTION_IN const T_* const pPrev_
  ) noexcept {
    return reinterpret_cast<T_*>(
      reinterpret_cast<std::uintptr_t>(pNext_) ^ reinterpret_cast<std::uintptr_t>(pPrev_)
      );
  }

  // Maps an exact requested size to a bucket index (0 to 55)
  [[nodiscard]] static inline const std::size_t get_class_idx(const std::size_t pSize_) noexcept {
    if (pSize_ <= 256) {
      // This part here creates 32 buckets spaced by 8 bytes, 32 * 8 = 256.
      // Sizes 1-8 goes to index 0, sizes 9-16 to index 1... sizes 249-256 to index 31.
      return (pSize_ - 1) >> 3; // basically, size_ / 8
    }

    // For sizes larger than 256 bytes, creating 8 byte-spaced chunks will
    // require thousands of chunks. So, switch to logarithmic scale and
    // divide into subranges, 256-512, 512-1024, etc.

    // Sub 1 to handle cases when size is power of 2.
    const std::size_t s{ pSize_ - 1 };

    // Calc. MSB. This gives which power-of-2 range this size belongs to.
    const std::size_t b{ static_cast<std::size_t>(std::bit_width(s)) - 1 };

    // Mathematically, b gives exponent, the n in 2^n. We are to divide a range (ex. 256-512)
    // into 4 subgroups. In case of exponents, division = subtraction. For example, consider 256.
    // We know 256 = 2^8. 8-2 comes to be 6. 2^6 = 64. On the other hand, 256/4 = 64.
    // This is basically that, division by 4 = subtraction of exponents by 2.
    const std::size_t shift{ b - 2 };

    // Calculates starting index of this group. Subtract 8 from b as the minimum b can be is 8.
    // log2(256) = 8. Left shift by 2 = multiply by 4. 4 bcs there are 4 subgroups inside a group.
    const std::size_t base_idx{ 32 + ((b - 8) << 2) };

    // Extract the next two bits of the size to determine which of the four subgroups (0, 1, 2 or 3)
    // this size falls into. Since the target sub bracket must always be a value between 0 and 3,
    // AND with 3.
    const std::size_t sub_idx{ (s >> shift) & 3 };

    /*
     * Let size = 300.
     * s = size - 1 = 299.
     * b = std::bit_width(s) - 1 = std::bit_width(299) - 1 = 9 - 1 = 8.
     * shift = b - 2 = 6. Look at how 300/4 = 75 has the (floor) power of 2, 64.
     * base_idx = 32 + ((b - 8) << 2) = 32 + ((8 - 8) << 2) = 32 + 0 = 32.
     * sub_idx = (s >> shift) & 3 = (299 >> 6) & 3 = 4 & 3 = 0
     * In case of 300, returned value will be base_idx + sub_idx = 32 + 0 = 32.
     */

    return base_idx + sub_idx;
  }

  // Maps a bucket index back to its aligned chunk size
  [[nodiscard]] static inline const std::size_t get_chunk_size(const std::size_t pIdx_) noexcept {
    // This entire function basically reverses the math of get_class_idx and findds the
    // nearest chunk size.

    if (pIdx_ < 32) {
      // Adds 1 and multiplies by 8.
      // Index 0 will return 8, index 1 will return 16... index 31 will return 256.
      return (pIdx_ + 1) << 3;
    }

    // Recalc. the base power-of-2 group by subtracting 32 (as the first 32 buckets are alrd
    // over with), multiplying by 4 and adding 8.
    const std::size_t b{ ((pIdx_ - 32) >> 2) + 8 };

    // Extract subbucket index (0-3).
    const std::size_t sub{ (pIdx_ - 32) & 3 };

    // Calc. shift. Reasoning is same as was in get_class_idx for shift.
    const std::size_t shift{ b - 2 };

    // Base size of the lower bound of this group (ex, 256, 512), will always be a power of 2.
    const std::size_t base{ std::size_t(1) << b };

    // Calculates the uniform size step for the 4 subgroups inside this group.
    const std::size_t step{ std::size_t(1) << shift };

    /*
     * Example, consider the group 256-512. We have 512-256 = 256. Dividing this into 4 subgroups, we get,
     * 320, 384, 448, 512. Each one increases by 64. Why? Because 256 / 4 = 64!.
     * Second, consider the result 32 obtained from the example in get_class_idx.
     * b = ((pIdx_ - 32) >> 2 + 8 = (32 - 32) >> 2 + 8 = 8.
     * In get_class_idx, we also had b = 8!
     * sub = (pIdx_ - 32) & 3 = (32 - 32) & 3 = 0.
     * shift = b - 2 = 8 - 2 = 6. In get_class_idx, we also had shift = 6!
     * base = 1 << b = 2^b = 2^8 = 256, aka this falls in group 256-512.
     * step = 1 << shift = 2^shift = 2^6 = 64, aka the step is acurately 64 for this group!
     */

    return base + ((sub + 1) * step);
  }

  /*
   * This arena alloc works on three different ways:
   * a. If required size (in alloc) is less than 2 MB, it allocates inside an arena.
   * b. If required size (in alloc) is equal to 2 MB, the alloc function internally
   *   calculates its req_size_ as 2 MB + 16 bytes (from alignment) + 16 bytes (from sizeof(allocation_header),
   *   which equals to 2,097,184. Assuming page size comes out to be 4 KB, the test "req_size_ > (arena_size - page_size_)"
   *   evaluates to true, as 2,097,184 is greater than 2,093,056. This does a direct OS allocation, aka the not-in-if block
   *   part of this function.
   * c. When an arena needs to be allocated, this function passes the alignment requirement on Windows, and allocates 4 MB
   *   on Linux. On Windows, this is handled natively. On Linux, this function truncates to match a 2 MB alignment.
   *
   * @return A pointer to the base address of allocated memory.
   */
  [[nodiscard]] static inline void* reserve_os(const std::size_t pSize_) noexcept {
#ifdef _WIN32
    if (pSize_ == kArenaSize) {
      static MEM_ADDRESS_REQUIREMENTS req{
        .LowestStartingAddress = nullptr,
        .HighestEndingAddress = nullptr,
        .Alignment = kArenaSize
      };

      static MEM_EXTENDED_PARAMETER param{
        .Type = MemExtendedParameterAddressRequirements,
        .Pointer = &req
      };

      return VirtualAlloc2(GetCurrentProcess(), nullptr, pSize_, MEM_RESERVE, PAGE_NOACCESS, &param, 1);
    }

    return VirtualAlloc2(GetCurrentProcess(), nullptr, pSize_, MEM_RESERVE, PAGE_NOACCESS, nullptr, 0);
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
    if (pSize_ == kArenaSize) {
      const std::size_t resv_size{ 2 * kArenaSize }; // Equates to 4 MB
      void* ptr{ mmap(nullptr, resv_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) };

      if (ptr == MAP_FAILED) {
        return nullptr;
      }

      const std::uintptr_t base{ reinterpret_cast<std::uintptr_t>(ptr) };
      const std::uintptr_t aligned{ (base + kArenaSize - 1) & ~(kArenaSize - 1) };
      const std::size_t pre_padding{ aligned - base };
      const std::size_t post_padding{ resv_size - pre_padding - kArenaSize };

      if (pre_padding) {
        munmap(reinterpret_cast<void*>(base), pre_padding);
      }

      if (post_padding) {
        munmap(reinterpret_cast<void*>(aligned + kArenaSize), post_padding);
      }

      return reinterpret_cast<void*>(aligned);
    }

    void* ptr{ mmap(nullptr, pSize_, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) };

    if (ptr == MAP_FAILED) {
      return nullptr;
    }

    return ptr;
#endif // ^^^ _WIN32 ^^^
  }

  [[nodiscard]] static inline bool commit_os(
    SELENA_FUNCTION_IN_MODIFIED void* const pStartPtr_,
    const std::size_t pSize_
  ) noexcept {
#ifdef _WIN32
    return VirtualAlloc2(GetCurrentProcess(), pStartPtr_, pSize_, MEM_COMMIT, PAGE_READWRITE, nullptr, 0) != nullptr;
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
    return mprotect(pStartPtr_, pSize_, PROT_READ | PROT_WRITE) == 0;
#endif // ^^^ _WIN32 ^^^
  }

  [[nodiscard]] static inline bool decommit_os(
    SELENA_FUNCTION_IN_MODIFIED void* const pStartPtr_,
    const std::size_t pSize_
  ) noexcept {
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable : 6250 28160) // Suppress "results in address space leaks" false positive
    return VirtualFreeEx(GetCurrentProcess(), pStartPtr_, pSize_, MEM_DECOMMIT) != 0;
# pragma warning(pop)
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
    madvise(pStartPtr_, pSize_, MADV_DONTNEED);
    return mprotect(pStartPtr_, pSize_, PROT_NONE) == 0;
#endif // ^^^ _WIN32 ^^^
  }

  [[nodiscard]] static inline bool release_os(
    SELENA_FUNCTION_IN_MODIFIED void* pPtr_,
    const std::size_t pSize_
  ) noexcept {
#ifdef _WIN32
    return VirtualFreeEx(GetCurrentProcess(), pPtr_, 0, MEM_RELEASE) != 0;
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
    return munmap(pPtr_, pSize_) == 0;
#endif // ^^^ _WIN32 ^^^
  }

  [[nodiscard]] static inline void* alloc_pages(
    const std::size_t pNumPages_
  ) noexcept {
    arena_t* curr{ state_.arena_head_ }, * prev{ nullptr };

    while (curr) {
      std::size_t start_page{}, consecutive{};

      for (std::size_t i{ 1 }; i < 512; ++i) {
        if (!(curr->committed_pages_[i >> 6] & (1ULL << (i & 63)))) {
          if (!consecutive) {
            start_page = i;
          }

          ++consecutive;

          if (consecutive == pNumPages_) {
            void* commit_ptr{ reinterpret_cast<std::byte*>(curr) + (start_page << std::countr_zero(get_page_size())) };

            if (!commit_os(commit_ptr, pNumPages_ << std::countr_zero(get_page_size()))) {
              return nullptr;
            }

            for (std::size_t j{ start_page }; j < start_page + pNumPages_; ++j) {
              curr->committed_pages_[j >> 6] |= 1ULL << (j & 63);
            }

            return commit_ptr;
          }
        } else {
          consecutive = 0;
        }
      }

      arena_t* next{ xor_ptrs(prev, curr->npx_) };
      prev = curr;
      curr = next;
    }

    arena_t* new_arena{ reinterpret_cast<arena_t*>(reserve_os(kArenaSize)) };

    if (!new_arena) {
      return nullptr;
    }

    if (!commit_os(new_arena, get_page_size())) {
      if (!release_os(new_arena, kArenaSize)) {
        // Memory leak!
      }

      return nullptr;
    }

    new(new_arena) arena_t();

    for (std::size_t i{}; i < 8; ++i) {
      new_arena->committed_pages_[i] = 0;
    }

    new_arena->committed_pages_[0] = 1;

    new_arena->npx_ = xor_ptrs<arena_t>(nullptr, state_.arena_head_);

    if (state_.arena_head_) {
      state_.arena_head_->npx_ = xor_ptrs<arena_t>(new_arena, xor_ptrs<arena_t>(nullptr, state_.arena_head_->npx_));
    }

    state_.arena_head_ = new_arena;

    void* commit_ptr{ reinterpret_cast<std::byte*>(state_.arena_head_) + get_page_size() };

    if (!commit_os(commit_ptr, pNumPages_ << std::countr_zero(get_page_size()))) {
      return nullptr;
    }

    for (std::size_t i{ 1 }; i < 1 + pNumPages_; ++i) {
      state_.arena_head_->committed_pages_[i >> 6] |= 1ULL << (i & 63);
    }

    return commit_ptr;
  }

  static inline void free_pages(
    SELENA_FUNCTION_IN_MODIFIED void* pPtr_,
    const std::size_t pNumPages_
  ) noexcept {
    std::byte* byte_ptr{ static_cast<std::byte*>(pPtr_) };
    arena_t* curr{ reinterpret_cast<arena_t*>(reinterpret_cast<std::uintptr_t>(pPtr_) & ~(kArenaSize - 1)) };
    std::byte* arena_byte_ptr{ reinterpret_cast<std::byte*>(curr) };
    const std::size_t start_page{ static_cast<std::size_t>(byte_ptr - arena_byte_ptr) >> std::countr_zero(get_page_size()) };

    if (decommit_os(pPtr_, pNumPages_ << std::countr_zero(get_page_size()))) {
      for (std::size_t i{ start_page }; i < start_page + pNumPages_; ++i) {
        curr->committed_pages_[i >> 6] &= ~(1ULL << (i & 63));
      }

      bool is_empty{ true };

      // When a new arena is created, its very first page (represented by 0th bit)
      // is immediately committed to store the metadata struct itself. A "completely empty"
      // arena will always have its 0th bit set to true. This makes the value of the 0th integer
      // equal to 1. If the value is anything else, it implies that other pages are currently
      // in use. Also, it can never be 0 because, as was mentioned, the first page is always filled,
      // which implies the first integer will always be at least 1.
      if (curr->committed_pages_[0] != 1) {
        is_empty = false;
      } else {
        for (std::size_t i{ 1 }; i < 8; ++i) {
          if (curr->committed_pages_[i]) {
            is_empty = false;
            break;
          }
        }
      }

      if (is_empty) {
        arena_t* c{ state_.arena_head_ };
        arena_t* p{ nullptr };

        while (c) {
          arena_t* n{ xor_ptrs(p, c->npx_) };

          if (c == curr) {
            if (p) {
              p->npx_ = xor_ptrs<arena_t>(xor_ptrs<arena_t>(p->npx_, c), n);
            } else {
              state_.arena_head_ = n;
            }

            if (n) {
              n->npx_ = xor_ptrs<arena_t>(p, xor_ptrs<arena_t>(c, n->npx_));
            }

            break;
          }

          p = c;
          c = n;
        }

        // if this succeeds, OK. If fails, memory leak!
        static_cast<void>(release_os(curr, kArenaSize));
      }
    } else {
      // Memory leak!
    }
  }
}; // class static_arena_alloc

} // namespace selena

#ifdef SELENA_ARENA_OVERLOAD_NEW_DELETE

#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable : 28251)
#endif // _WIN32

void* operator new(const std::size_t pSize_) {
  void* ptr{ selena::static_arena_alloc::alloc(pSize_) };

  if (!ptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void operator delete(void* pPtr_) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void* operator new[](const std::size_t pSize_) {
  void* ptr{ selena::static_arena_alloc::alloc(pSize_) };

  if (!ptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void operator delete[](void* pPtr_) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void operator delete(void* pPtr_, std::size_t) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void operator delete[](void* pPtr_, std::size_t) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

#ifdef SELENA_ARENA_OVERLOAD_NOTHROW_NEW_DELETE

void* operator new(const std::size_t pSize_, const std::nothrow_t&) noexcept {
  return selena::static_arena_alloc::alloc(pSize_);
}

void operator delete(void* pPtr_, const std::nothrow_t&) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void* operator new[](const std::size_t pSize_, const std::nothrow_t&) noexcept {
  return selena::static_arena_alloc::alloc(pSize_);
}

void operator delete[](void* pPtr_, const std::nothrow_t&) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

#endif // SELENA_ARENA_OVERLOAD_NOTHROW_NEW_DELETE

#ifdef SELENA_ARENA_OVERLOAD_ALIGNED_NEW_DELETE

void* operator new(const std::size_t pSize_, const std::align_val_t pAlignment_) {
  void* ptr{ selena::static_arena_alloc::alloc(pSize_, pAlignment_) };

  if (!ptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void operator delete(void* pPtr_, const std::align_val_t) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void* operator new[](const std::size_t pSize_, const std::align_val_t pAlignment_) {
  void* ptr{ selena::static_arena_alloc::alloc(pSize_, pAlignment_) };

  if (!ptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void operator delete[](void* pPtr_, const std::align_val_t) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void operator delete(void* pPtr_, const std::size_t, const std::align_val_t) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void operator delete[](void* pPtr_, const std::size_t, const std::align_val_t) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

#ifdef SELENA_ARENA_OVERLOAD_NOTHROW_NEW_DELETE

void* operator new(const std::size_t pSize_, const std::align_val_t pAlignment_, const std::nothrow_t&) noexcept {
  return selena::static_arena_alloc::alloc(pSize_, pAlignment_);
}

void operator delete(void* pPtr_, const std::align_val_t, const std::nothrow_t&) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

void* operator new[](const std::size_t pSize_, const std::align_val_t pAlignment_, const std::nothrow_t&) noexcept {
  return selena::static_arena_alloc::alloc(pSize_, pAlignment_);
}

void operator delete[](void* pPtr_, const std::align_val_t, const std::nothrow_t&) noexcept {
  selena::static_arena_alloc::free(pPtr_);
}

#endif // SELENA_ARENA_OVERLOAD_NOTHROW_NEW_DELETE

#endif // SELENA_ARENA_OVERLOAD_ALIGNED_NEW_DELETE

#ifdef _WIN32
# pragma warning(pop)
#endif // _WIN32

#endif // SELENA_ARENA_OVERLOAD_NEW_DELETE

#endif // SELENA_STATIC_ARENA_ALLOC
