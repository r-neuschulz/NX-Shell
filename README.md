# NX-Shell ![Github latest downloads](https://img.shields.io/github/downloads/joel16/NX-Shell/total.svg)

NX-Shell is a multi-purpose file manager for the Nintendo Switch. It handles image, archive, and text format file types directly.

Inspired by [LineageOS's file manager](https://github.com/LineageOS/android_packages_apps_CMFileManager), NX-Shell uses Dear ImGui on top of OpenGL3 for native 1080p/720p rendering at full vsync-locked 60 fps. It is localized into 12 languages, has custom theming options, and can be used docked or handheld with full touch support.

<p align="center">
  <img src="NX-Shell_Main.jpg" alt="NX-Shell Main Screenshot rendering in 720p" width="640" height="360"/>
</p>

<p align="center">
  <img src="NX-Shell_Settings.png" alt="NX-Shell Settings Screenshot rendering in 1080p" width="640" height="360"/>
</p>

# Features:

- File operations: copy, move, delete, rename, create (with Switch keyboard).
- File properties (size, created/modified/accessed timestamps) and sorting (name, date, size).
- Image viewer with caching (BMP, GIF, JPG, PGM, PPM, PNG, PSD, TGA, WEBP).
- Archive extraction (only ZIP support).
- File Preview using hex and plain text (all file types).
- Device browsing: safe, user, system, USB.
- Language support: Japanese, English, French, German, Italian, Spanish, Simplified/Traditional Chinese, Korean, Dutch, Portuguese, Russian.
- Automatic docked (1080p) or handheld (720p) rendering detection with configurable overrides.
- Themes: Dark/Light mode, accent colors, button styles.
- Safe applet mode and self-updating via GitHub.

# Building:

## Prerequisites

Follow the [devkitPro Getting Started guide](https://devkitpro.org/wiki/Getting_Started) to install the toolchain for your platform. Then install the required Switch packages:

```bash
sudo dkp-pacman -S switch-dev switch-freetype switch-curl switch-libpng switch-libjpeg-turbo switch-libwebp switch-libgif switch-jansson switch-glad switch-minizip
```

> **Note:** `switch-dev` is a meta-package that installs the base toolchain (devkitA64, libnx, switch-tools). The remaining packages are additional libraries required by NX-Shell.

## Build

```bash
cmake -B build
cmake --build build
```

The output `NX-Shell.nro` will be copied to the project root.

## Debug (nxlink)

To deploy and debug on your Switch over the network:

```bash
nxlink -a <SWITCH_IP> -s NX-Shell.nro
```

Replace `<SWITCH_IP>` with your Switch's IP address. The IP is shown when using (Y) in homebrew launcher.

# Credits:

- [PreetiSketch](https://www.youtube.com/channel/UCxg-ATCKERNRSG87bgjhoqA) for the banner.
- [Omar Cornut](https://github.com/ocornut) and contributors for [Dear ImGui](https://github.com/ocornut/imgui).
- [devkitPro](https://devkitpro.org/) maintainers and contributors for [libnx](https://github.com/switchbrew/libnx), [devkitA64](https://devkitpro.org/wiki/devkitA64), and many other packages used by this project.
- [DarkMatterCore](https://github.com/DarkMatterCore) for [libusbhsfs](https://github.com/DarkMatterCore/libusbhsfs).
- [Sean Barrett](https://github.com/nothings) for [stb_image](https://github.com/nothings/stb).
- [Grzegorz Kostka](https://github.com/gkostka) for [lwext4](https://github.com/gkostka/lwext4) and the [NTFS-3G](https://github.com/tuxera/ntfs-3g) developers.
- [xfangfang](https://github.com/xfangfang) for [wiliwili](https://github.com/xfangfang/wiliwili) and showing how to install nsps