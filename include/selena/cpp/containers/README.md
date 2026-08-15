# selena/containers

This directory contains two containers constexpr capable: `selena::unordered_set` and `selena::unordered_map`. They work on open-addressing w/ linear
probing. Internally, they are flat arrays. Both of them are more or less the same:

```cpp
// Template signature of selena::unordered_map:
selena::unordered_map<key_type, val_type, max_elements, load_factor> /**/
// Template signature of selena::unordered_set:
selena::unordered_set<key_type, max_elements, load_factor> /**/
```

There are some things to be noted however:
1. `key_type` must be either a primitive type (integers, floats), a C-style string (const char* const str_{ "This is a C-style string" }), or a `std::string_view`.
Booleans are forbidden. If you are to use a hash map to store booleans, it is better if you just created a two element array and made it such that `false` accesses the element at
index 0 and `true` at index 1.
2. `val_type` can be anything. However:
  - To get `constexpr` benefits across the entire map/set, it must also be `constexpr` capable.
  - If it is a boolean, the map switches to `std::bitset` instead, to preserve space.
3. `max_elements`, as the name sounds, denotes the predicted maximum number of elements. It must be a `std::size_t`.
4. `load_factor`, as it sounds, denotes the load factor of this container. It must be a `long double`. However, if you pass anything less than or equal to 0,
or greater than 0.85, it switches to a load factor of `0.35` (to punish you!).

Usage:

```cpp
constexpr selena::unordered_set<std::uint64_t, 10, 0.35L> /**/; // OK
constexpr selena::unordered_set<std::uint64_t, 10, 3.1415L? /**/; // OK: load factor switches to 0.35
constexpr selena::unordered_set<bool, 10, 0.85L> /**/; // Error: booleans are forbidden as keys.
constexpr selena::unordered_map<std::uint64_t, bool, 10,, 0.85L> /**/; // OK: the booleans are represented with std::bitset<10> instead.

struct X {
  std::uint32_t num_;
  std::string_view str_;
};

constexpr selena::unordered_map<std::uint64_t, X, 10, 0.85L> /**/; // OK: struct X internally consists of std::uint32_t and std::string_view, both of which
// are constexpr-capable.

struct Y {
  void(*func_ptr)(const std::string&, const std::vector<int>&);
};

constexpr selena::unordered_map<std::uint64_t, Y, 10, 0.85L> /**/; // OK: struct Y is a function pointer. It is constexpr-capable.

constexpr selena::unordered_map<std::uint64_t, std::string, 10, 0.85L> /**/; // Error: std::string isn't constexpr-capable.
selena::unordered_map<std::uint64_t, std::string, 10, 0.85L> /**/; // OK: non-constexpr map. std::string is allowed here.
const selena::unordered_map<std::uint64_t, std::string, 10, 0.85L> /**/; // OK: evaluates at runtime and then becomes a const. However, if you are
// certain the strings will not be modified at runtime, consider replacing std::string with std::string_view and making the container a constexpr.
```

A certain note:

```cpp
// Uses like the structs `X` and `Y` in above are rather restrictive...

void f_0(const std::string& str, const std::vector<int>& vec);
void f_1(const std::string& str, const std::vector<int>& vec);

constexpr Y arr[2]{ f_1, f_0 }; // OK
constexpr selena::unordered_map<char, Y, 2, 0.35L> temp{ {'a', f_1}, {'b', f_0} }; // error: no instance of ctor matches arg list
constexpr selena::unordered_map<char, Y, 2, 0.35L> temp{ {'a', {f_1}}, {'b', {f_0} }}; // OK

// In other words, make sure the struct's stuff are mentioned in scopes ({}). Or you can become even more explicit:
constexpr selena::unordered_map<char, Y, 2, 0.35L> temp{ { 'a', Y{ f_1 } }, { 'b', Y{ f_0 } } }; // OK
```

Both define methods that are functionally equivalent to the standard's counterpart. It removes some methods. However, both define some more functions:

```cpp
constexpr std::size_t capacity() const noexcept; // Returns the capacity of the container, valid for both map and set. This is equivalent to max elements / load factor rounded up to the nearest power of 2.
constexpr long double max_load_factor() const noexcept; // Returns the maximum load factor (this is equivalent to the
// load factor provided in the template's initialization, so long as it didn't go out of the map/set's load factor bounds.
constexpr long double curr_load_factor() const noexcept; // The current load factor of the map/set.
constexpr explicit unordered_set(std::span<const KeyT_> span_); // Conversion constructor. Creates a unordered_set out of a predefined array of keys. The array could be a C-style array, a std::array,
// a std::vector, etc. Valid ONLY for the unordered_set.
constexpr unordered_set& operator=(std::span<const KeyT_> span_); // Conversion assignment. Clears a pre-existing unordered_set, and creates a new one in-place out of a pre-defined array of keys.
// Valid ONLY for the unordered_set.
```

Usage of the conversion operators is fairly simple:

```cpp
std::vector<std::uint64_t> vec_{ 1,2,3,4,5 };

selena::unordered_set<std::uint64_t, 10, 0.75L> set_{ vec_ }; // Conversion constructor

selena::unordered_set<std::uint64_t, 10, 0.75L> set2_;
set2_ = vec_; // Conversion assignment.

constexpr std::array<std::uint64_t, 5> arr_{ 1,2,3,4,5 };
constexpr selena::unordered_set<std::uint64_t, 10, 0.75L> set3_{ arr_ }; // Conversion constructor. Evaluated at compile-time.

std::string str_{ "This is a str" };
selena::unordered_set<char, 20, 0.75L> set4_{ str_ };

for (const std::uint64_t key_ : set_) {
  std::cout << key_ << ' ';
}

std::cout << '\n';

for (const std::uint64_t key_ : set2_) {
  std::cout << key_ << ' ';
}

std::cout << '\n';

for (const std::uint64_t key_ : set3_) {
  std::cout << key_ << ' ';
}

for (const char key_ : set4_) {
  std::cout << key_ << ' ';
}

// On my PC, this prints:
// 1 4 5 3 2
// 1 4 5 3 2
// 1 4 5 3 2
//   T s a t i r h
```
