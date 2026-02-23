# Contributing to mt-dlp

First off, thank you for considering contributing to `mt-dlp`! This project aims to be a robust, high-performance download utility, and we value technical precision and systems-level thinking.

Whether you're looking to refactor our "monolithic" main file, optimize thread logic, or fix a bug, your help is welcome!

## Getting Started

### Prerequisites

  * **C++ Compiler**: Must support **C++20**.
  * **CMake**: Version 3.16 or higher.
  * **Dependencies**: `libcurl` and `ftxui`.

### Building the Project

1.  **Clone the repository:**
  ```bash
  git clone https://github.com/Omega493/mt-dlp.git
  cd mt-dlp
  ```

2. **Building it on your System**
    * **Windows:**
    It is recommended to use [Visual Studio](https://visualstudio.microsoft.com) for all building and development workloads. Please make sure `vcpkg` is on your system, and `libcurl` and `ftxui` are available via `vcpkg`. Target the `x64-windows` triplet.

    * **Linux:**
    The project defines a `build.sh` at the root directory. Invoke it using `sh build.sh` - it shall automatically install all dependencies and build the executable. Or, do the manual way:
      ```bash
      # For Debian-based OSes
      sudo apt update
      sudo apt install build-essential cmake ninja-build gcc g++ \
        libcurl4-openssl-dev libftxui-dev -y

      # For Fedora / RHEL / centOS
      sudo dnf upgrade --refresh -y
      sudo dnf install make cmake ninja-build gcc g++ \
        libcurl-devel ftxui-devel -y

      # For Arch Linux
      sudo pacman -Syu --noconfirm
      sudo pacman -S --needed --noconfirm base-devel cmake ninja curl

      WORK_DIR=$(mktemp -d)
      
      git clone --depth 1 https://github.com/ArthurSonzogni/FTXUI.git "$WORK_DIR/FTXUI"

      cmake -S "$WORK_DIR/FTXUI" -B "$WORK_DIR/FTXUI/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DFTXUI_BUILD_EXAMPLES=OFF \
        -DFTXUI_BUILD_DOCS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local
        
      sudo cmake --build "$WORK_DIR/FTXUI/build" --target install

      rm -rf $WORK_DIR

      # Common for all
      cmake --preset="linux-debug" # If on debug mode
      cmake --preset="linux-release" # If on release mode

      cmake --build --preset=<preset_name> # Replace with whatever preset you started with
      ```
      
      
## Development Workflow

1.  **Fork the Project**: Create your own copy of the repository.
2.  **Create your Feature Branch**: `git checkout -b feature/AmazingFeature`.
3.  **Commit your Changes**: `git commit -m 'Add some AmazingFeature'`.
    - We appreciate signed commits (commits signed with a PGP signature). Please refer to [this video](https://www.youtube.com/watch?v=xj9OiJL56pM) on how to set a PGP signature on your device. (Disclaimer: Neither am I the creator of this video, nor am I related to them - this video serves only as a educational guide provided here.)
4.  **Push to the Branch**: `git push origin feature/AmazingFeature`.
5.  **Open a Pull Request**: Describe your changes clearly.
6. If you need file links for debugging various features, please check out `tests/tests.md`. It curates a bunch of "speedtest" links.

### Restricted Files (Vendored Code)
* **`include/selena/`**: Files in this directory belong to the standalone [Selena Library](https://github.com/Omega493/selena).
  * **Do not edit these files directly in this repository.**
  * If you find a bug or want to add a feature to `selena`, please submit a Pull Request to the original [Selena repository](https://github.com/Omega493/selena).
  * We periodically sync this folder with the upstream version. Any local changes made here will be overwritten/rejected.

## Coding Conventions

**IMPORTANT:** We adhere to a strict set of coding standards. Pull Requests that do not follow these rules will be requested for changes.

### General Formatting

* **Indentation:** Use **2 spaces** exclusively. Do not use tabs.
* **Line Endings:** Must be **LF** (Unix style), not CRLF.
* **Encoding:** Files must be **UTF-8** (without BOM).
* **Naming:**
  * Classes: `PascalCase`
  * Variables, Functions, Methods / Member Functions: `snake_case`
  * Global Constants: `SCREAMING_SNAKE_CASE`

### C++ Modernity & Safety

* **Explicit Typing:** Do **NOT** use `auto` unless absolutely necessary (e.g., structured bindings or unnameable lambda types). Always explicitly list the type.
* **Namespaces:** Do **NOT** use `using namespace`. Use explicit qualification (e.g., `std::vector` instead of `vector`).
* **Initialization:** Unless required otherwise, use uniform initialization `{}`.
  * **Spacing:** There must be a space between the braces and the content.
  * *Example:* `std::vector<int> vec{ 2, -4, 45, 0 };`
* **Global Constants:** Do NOT use macros for constants. Use `constexpr` instead.

### Control Flow & Syntax

* **Ignored Return Values:** If you call a function with a return value but ignore it, or if the function returns `void`, you must manually cast the call to `void`.
  * *Example:*
    ```cpp
    void foo() {
      // ...
    }

    bool faz() {
      // ...
    }

    void bar() {
      // ...
      (void)foo();
      (void)faz();
      // ...
    }
    ```

## Areas needing Help

If you are looking for a place to start, check the Issues tab or consider these architectural goals:

1.  **Refactoring `main.cpp`**: The current main file is a monolith. Breaking it into logical classes/modules is a high priority.
2.  **Connection Pooling**: Implementing a system where multiple threads share a single HTTP connection to reduce overhead and avoid 429/403 errors.
3.  **Multi-file Support**: Extending the logic to handle a queue of multiple downloads.

## License

By contributing, you agree that your contributions will be licensed under the **GPLv3**. When splitting code into new files (e.g., `src/ui.cpp`), you must copy the license header from main.cpp to the top of the new file.
