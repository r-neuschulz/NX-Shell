# NX-Shell NSP Forwarder

This directory contains the source code for the NSP forwarder - a small homebrew loader that can be installed to the Nintendo Switch home menu and launches NX-Shell.nro from the SD card.

## What is an NSP Forwarder?

An NSP forwarder is a minimal NSP (Nintendo Switch Package) that, when installed to the home menu, acts as a launcher for an NRO file on the SD card. This provides:

- **Full memory access**: Running from home menu gives the application full system memory instead of the limited applet mode memory
- **Convenient launching**: Start NX-Shell directly from the home menu without going through the Homebrew Menu

## Building

The NSP forwarder is built automatically as part of NX-Shell:

```bash
cmake -B build
cmake --build build
```

### Requirements

- **hacbrewpack**: Required for NSP creation. Install from https://github.com/The-4n/hacBrewPack
- **nspmini library**: Automatically fetched via FetchContent

## NRO Search Paths

The forwarder searches for NX-Shell.nro in the following locations (in order):

1. `sdmc:/switch/NX-Shell/NX-Shell.nro`
2. `sdmc:/switch/NX-Shell.nro`
3. `sdmc:/NX-Shell.nro`
4. Falls back to `sdmc:/hbmenu.nro` if not found

## Warning

⚠️ **Installing NSP forwarders may result in a console ban if used online.** Use at your own risk.

## Credits

- Based on [nx-hbloader](https://github.com/switchbrew/nx-hbloader) by switchbrew
- NSP installation via [nspmini](https://github.com/StarDustCFW/nspmini) by StarDustCFW
