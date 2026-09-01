# DOOM 64 EX II

A modern Doom 64 engine, derived from [Doom64EX](https://github.com/svkaiser/Doom64EX)
by Samuel "Kaiser" Villarreal, based on Zohar Malamant's `rom` branch.

This is a **modified version**. See the Git history for details of the changes.

## License

GPL v2 or later. See [LICENSE](LICENSE) and [COPYING](COPYING).

## Project status

The engine builds and runs on Windows with Visual Studio 2026 and MSVC.
Rendering still uses fixed-function OpenGL 1.4 — modernising the renderer is
the current work in progress.

Fixes already applied on top of the original `rom` branch:

- Repaired the FluidSynth submodule URL (the old one pointed to a deleted repository)
- Raised the minimum CMake version and removed a stale DLL copy step
- Moved from C++14 to C++17 (the code already used nested namespaces)
- MSVC build fixes: `__GNUC__` guard around `cxxabi.h`, x86 inline assembly
  disabled on 64-bit, variable-length arrays replaced with `std::vector`,
  `radix_tree` name clash resolved
- **Fixed ZIP lump reading**: the size was read from the ZIP local file header,
  which is zero for archives written in streaming mode. This bug made every pk3
  lump come back empty, which in turn prevented maps from loading.

## Required data

The engine needs two files placed next to the executable:

- **`doom64.rom`** — a Doom 64 ROM for the Nintendo 64 (US or EU releases).
  The engine reads it directly; no conversion step is needed.
- **`doom64ex.pk3`** — generated automatically at build time, in the `build` directory.

No game data is distributed with this repository.

## Building on Windows

### Requirements

- **Visual Studio 2026** with the "Desktop development with C++" workload,
  and in particular the **C++ CMake tools for Windows** component
- **Git for Windows**

### 1. Get the source

```powershell
git clone https://github.com/Styd051/DOOM64EX-II.git
cd DOOM64EX-II
git submodule update --init --recursive
```

### 2. Install dependencies with vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install sdl2:x64-windows sdl2-net:x64-windows zlib:x64-windows `
                libpng:x64-windows fmt:x64-windows `
                boost-optional:x64-windows boost-utility:x64-windows `
                boost-circular-buffer:x64-windows boost-algorithm:x64-windows `
                boost-functional:x64-windows boost-container-hash:x64-windows `
                boost-lexical-cast:x64-windows boost-variant:x64-windows
cd ..
```

### 3. Configure and build

From a **Developer PowerShell for VS 2026**:

```powershell
cmake -B build -S . -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=PATH/TO/vcpkg/scripts/buildsystems/vcpkg.cmake `
      -DENABLE_TESTING=OFF "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
cmake --build build --config Release
```

The `CMAKE_POLICY_VERSION_MINIMUM` flag is required because the FluidSynth
submodule's `CMakeLists.txt` declares compatibility with CMake versions that
recent releases no longer accept.

The executable is produced at `build\Release\doom64ex2.exe`.

### 4. Run

Copy `build\doom64ex.pk3` and your `doom64.rom` into `build\Release\`, then:

```powershell
cd build\Release
.\doom64ex2.exe
```

## Building on Linux and macOS

The original build files are kept but have not been tested since work resumed
on this project. Dependencies are SDL2, SDL2_net, zlib, libpng, fmt, Boost and
OpenGL.

## Credits

- **Samuel "Kaiser" Villarreal** — original author of Doom64EX
- **Zohar Malamant (dotfloat / pinkwah)** — C++ rewrite and the `rom` branch
- See [AUTHORS](AUTHORS) for the full list of contributors