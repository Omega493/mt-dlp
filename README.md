# mt-dlp

`mt-dlp` (Multi-Threaded Downloader Program) is a high-performance, open-source file download for the terminal, engineered to maximize throughput on high-bandwidth connections.
Unlike standard single-threaded downloaders (like `wget` or browser defaults) which are often bottlenecked by server-side per-connection speed limits, `mt-dlp` leverages concurrency to saturate available bandwidth.

## Key Features

- **Parallel Execution**: Dynamically spawns worker via `libcurl`'s multi-handles to download multiple chunks simultaneously.
- **Rich TUI**: Powered by `FTXUI`, providing granular visualization of per-thread progress, speed, and status.
- **Resource Efficiency**: Powered by the in-house arena allocator, [`selena::static_arena_alloc`](https://codeberg.org/Omega493/selena/src/branch/main/include/selena/allocators),
  which provides a relatively higher cache locality as compared to OS defaults.
- **System Integration**: Directly interfaces with `libcurl` for robust protocol handling while managing raw system resources via RAII and manual memory safeguards.

## Building

### Prerequisites

- **C++ Compiler**: Must support **C++26**.
- **CMake**: Version 4.3 or higher.
- **Dependencies**: `libcurl` and `ftxui`.

### Building the Project

**1.**  **Clone the Repository:**
  ```bash
  git clone https://github.com/Omega493/mt-dlp.git
  cd mt-dlp
  ```

**2.** **Installing the Dependencies:**
    * **Windows:**
    Please make sure `vcpkg` is on your system, and `libcurl` and `ftxui` are available via `vcpkg`. Target the `x64-windows` triplet.

    * **Linux:**
    Enter the following commands:
    ```bash
    # For all three types of OSes, if you feel like using Clang, replace "gcc g++" with "clang lld".
    # For Arch-based OSes, you are to just specify those two.
    # For Debian-based OSes
    sudo apt update
    sudo apt install build-essential cmake ninja-build gcc g++ libcurl4-openssl-dev libftxui-dev -y

    # For Fedora / RHEL / centOS
    sudo dnf upgrade --refresh -y
    sudo dnf install make cmake ninja-build gcc g++ libcurl-devel ftxui-devel -y

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
    ```

**3.** **Building it on your System:**
For Windows, you can open Visual Studio and press Ctrl + Shift + B to build. If you feel like using the terminal, then, open `x64 Native Tools Command Prompt for VS`. Then, navigate to
wherever you cloned this project at. For Linux, directly navigate to the project folder. You are to then run:
```bash
cmake --preset=<preset_name>
cmake --build --preset=<preset_name> # The preset name must be equal to whatever you chose at the previus command.
```

The CMake presets are named in the following format: `<os_name>-x64-<compiler_name>-<build_type>`.
  - OS Name is between `windows` and `linux`.
  - Compiler name can be `clang` or `msvc` on Windows, or `clang` or `gcc` on Linux.
  - Build type can be `debug`, `release`, or `relwithdebinfo`. `relwithdebinfo` is the same as `release`, the flags passed are the same as well.
    The extra thing it does is that it generates debug information (PDB files).

Examples:
  - `windows-x64-msvc-debug` Implies that the OS is Windows, the compiler is MSVC and the build type is debug.
  - `windows-x64-clang-release` Implies that the OS is Windows, the compiler is Clang (clang-cl) and build type is release.
  - `linux-x64-gcc-release` Implies that the OS is Linux, the compiler is `gcc/g++` and build type is release.
  - `linux-x64-clang-relwithdebinfo` Implies that the OS is Linux, the compiler is `clang/clang++` and build type is Release with Debug Info.

For details on the exact flags being passed, head over to `CMakePresets.json`.

**4.** **Installing it on your System:**
    * **Windows:**
    After the build process is over, simply place the executable's path to your system's environment variable.
    Or, you can manually copy paste `mt-dlp.exe`, `libcurl.dll` and `zlib1.dll` (or `z.dll`) to some different folder and add them to your system's path.
  
    * **Linux:**
    Navigate to the folder where the build artifacts are stored. It should be of format `/path/tp/mt-dlp/build/linux/<preset_name>`. Preset name is the same as the one
    you picked when building the sources. Then, simply run `sudo cp mt-dlp /usr/bin`.

## Usage

Whether you are on Windows or Linux, the usage remains the same:
```bash
mt-dlp <url>
```

Replace `<url>` with an actual URL. Please note that only HTTP / HTTPs URLs are accepted.

When provided with the URL, the tool will automatically perform a HEAD request to the source server and request it for the file's size and whether it accepts ranges.
Should the answer to both questions be true, the downloader automatically slashes the file into 8 individual chunks and starts downloading them at once.

## Contributing

Please head over to `CONTRIBUTING.md` for steps on how to contribute to this project.
