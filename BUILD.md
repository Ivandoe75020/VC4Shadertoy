# Building Video converter for Shadertoy

Copyright (C) Asger Andersen

This document explains how to build and package the program from source on Windows. FFmpeg is linked statically, but the WinUI 3 application must be distributed with the Windows App SDK runtime files and resources in the Release output directory. Do not distribute the application executable by itself.

---

## 1. Requirements

| Tool | Version | Notes |
|---|---|---|
| Windows | 10 (1809) or 11, x64 | Media Foundation must be present (not missing on "N" editions, install the Media Feature Pack there) |
| Visual Studio | 2022 (17.x) | Workload "Desktop development with C++" |
| Windows SDK | 10.0.22621.0 | Installed with the workload above |
| MSVC toolset | v143 (x64) | Installed with the workload above |
| MSYS2 | current | Default location `C:\msys64`, or set `MSYS2_ROOT` |
| NSIS | 3.x | Required only to build the installer; the build script expects `C:\Program Files (x86)\NSIS\makensis.exe` |

The Windows App SDK, C++/WinRT and the Windows SDK build tools are restored automatically from NuGet when the solution is built (internet access is needed the first time).

### MSYS2 packages

Open an MSYS2 shell and run:

```
pacman -S make diffutils nasm pkgconf
```

`nasm` is strongly recommended: without it, FFmpeg is built without x86 assembly and the RGB to YUV conversion is noticeably slower, which matters for 1080p at 60 fps.

---

## 2. Project layout

| Path | Content |
|---|---|
| `src/` | Core C++ code: export pipeline, interfaces, FFmpeg layer |
| `ui/` | WinUI 3 interface (XAML and code-behind) |
| `assets/` | Interface icons and the executable/installer icon |
| `third_party/ffmpeg/src/` | FFmpeg 8.1.3 source (unmodified) |
| `third_party/ffmpeg/licenses/` | FFmpeg license texts and notice |
| `third_party/ffmpeg/x64/` | Generated: FFmpeg headers and static libraries |
| `tools/` | Build and check scripts |
| `obj/`, `bin/` | Generated: intermediate files and program output |

---

## 3. Step 1: build FFmpeg (once)

From a regular PowerShell window, at the project root:

```
powershell -ExecutionPolicy Bypass -File tools\build_ffmpeg.ps1
```

The script:

1. Finds Visual Studio 2022 and loads the x64 MSVC environment.
2. Starts MSYS2 and runs `tools/build_ffmpeg.sh`.
3. Configures FFmpeg with the options of `tools/ffmpeg_config.sh` (static, MSVC, `-MD` runtime, LGPL only).
4. Runs `tools/verify_ffmpeg_lgpl.sh`, which stops the build if a GPL, non-free or external library is enabled.
5. Compiles FFmpeg and installs it into `third_party/ffmpeg/x64`.

When it succeeds, these files exist:

```
third_party\ffmpeg\x64\include\...
third_party\ffmpeg\x64\lib\avcodec.lib
third_party\ffmpeg\x64\lib\avformat.lib
third_party\ffmpeg\x64\lib\avutil.lib
third_party\ffmpeg\x64\lib\swscale.lib
third_party\ffmpeg\x64\lib\swresample.lib
```

This step only needs to be repeated if the FFmpeg source or `tools/ffmpeg_config.sh` changes. The first build takes several minutes.

### Enabled FFmpeg components

| Role | Component |
|---|---|
| H.264 encoder | `h264_mf` (Windows Media Foundation) |
| Audio encoder | `aac` (native) |
| Audio decoder | `mp3float` (native) |
| Demuxer | `mp3` |
| Muxer | `mp4` |
| Conversion | `libswscale`, `libswresample` |
| Protocol | `file` |

`libx264`, `--enable-gpl`, `--enable-nonfree` and `--enable-version3` are never used.

---

## 4. Step 2: build the program

### Visual Studio

1. Open `VideoConverterForShadertoy.sln`.
2. Select the `Release | x64` (or `Debug | x64`) configuration. Only x64 is configured.
3. Build the solution (Ctrl+Shift+B). NuGet packages are restored automatically. Release builds are automatically trimmed of build-only files and unused runtime content after linking.

### Command line

From an "x64 Native Tools Command Prompt for VS 2022", at the project root:

```
msbuild VideoConverterForShadertoy.sln /restore /p:Configuration=Release /p:Platform=x64
```

### Release output and distribution

```
bin\x64\Release\VideoConverterForShadertoy.exe
bin\x64\Release\*.dll
bin\x64\Release\*.pri
bin\x64\Release\assets\
bin\x64\Release\en-us\
bin\x64\Release\licenses\
bin\x64\Release\Microsoft.UI.Xaml\
bin\x64\Release\ui\
```

The Release folder contains the executable and the WinUI/Windows App SDK runtime DLLs and resources required to run it. The build cleanup removes debug symbols, linker intermediates, metadata and unused runtime/language files. Distribute the complete `bin\x64\Release` folder or use the installer; do not copy only the `.exe`. Keep the `licenses` folder with the application.

### Build the installer

Install NSIS 3.x in its default location, then run this command from the project root:

```
powershell -ExecutionPolicy Bypass -File tools\build_installer.ps1
```

The script packages the current `bin\x64\Release` folder and writes `dist\VideoConverterForShadertoy-1.0.0-Setup.exe`. Build the Release configuration first. The installer uses the application icon, creates Start menu and desktop shortcuts, and includes the FFmpeg LGPL 2.1 license page.

---

## 5. Step 3: check the project conventions

```
powershell -ExecutionPolicy Bypass -File tools\check_conventions.ps1
```

The script checks that the source contains no comments, that no version-control folder or forbidden external repository reference exists, that the name and copyright are present, and that no theme is forced. It must print `All conventions are respected.`

---

## 6. Replacing FFmpeg (LGPL)

FFmpeg is licensed under the LGPL 2.1 or later and is linked statically. To use a modified or different FFmpeg:

1. Replace the content of `third_party/ffmpeg/src` with the desired FFmpeg source.
2. Run `tools\build_ffmpeg.ps1` again.
3. Rebuild the solution. The program is relinked with the new libraries.

The full text of the obligations and the component list is in `third_party/ffmpeg/licenses/FFmpeg-NOTICE.txt`.

---

## 7. Troubleshooting

| Problem | Cause and fix |
|---|---|
| `Visual Studio 2022 with the C++ desktop workload is required.` | Install the "Desktop development with C++" workload. |
| `MSYS2 was not found.` | Install MSYS2 or set the `MSYS2_ROOT` environment variable to its folder. |
| `cl.exe` not found, or configure fails on "link" | Run `tools\build_ffmpeg.ps1` rather than `build_ffmpeg.sh` directly, so the MSVC environment is loaded. |
| `nasm was not found` warning | Run `pacman -S nasm` in MSYS2. The build still works without it, only slower. |
| `Unable to open avcodec.lib` when linking | Step 1 was not done, or failed. Check that `third_party\ffmpeg\x64\lib` contains the five `.lib` files. |
| `NSIS compiler not found` | Install NSIS 3.x in `C:\Program Files (x86)\NSIS`, then run `tools\build_installer.ps1` again. |
| Installer script cannot find the Release executable | Build the `Release | x64` configuration before running `tools\build_installer.ps1`. |
| LNK4098 or duplicate symbol errors about the C runtime | FFmpeg must be built with `-MD` (already set in `tools/ffmpeg_config.sh`). Rebuild FFmpeg after any change. |
| `The H.264 encoder is not available` at export time | Media Foundation is missing (Windows "N" edition): install the Media Feature Pack. |
| FFmpeg configuration check failed | The configuration was changed to include a GPL, non-free or external library. Restore `tools/ffmpeg_config.sh`. |
