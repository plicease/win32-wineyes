# wineyes

A Win32 clone of the classic X11 `xeyes`: a pair of eyes that sit on your
desktop and follow the mouse cursor wherever it goes.

- Eyes track the cursor anywhere on the screen, across all monitors.
- Only the eyes are drawn; everything around them is transparent and
  click-through.
- Sized from your monitor's physical DPI to match the original 150x100
  `xeyes` on a CRT of the era (about 1.9" x 1.25"), regardless of the
  Windows display scaling setting.

## Install

Download the latest `wineyes-X.Y.Z-setup.exe` from
[Releases](https://github.com/plicease/win32-wineyes/releases) and run it.
It installs for the current user by default (no admin prompt) and adds a
Start menu entry. Nothing is put on the desktop.

Prefer not to install? The release also has a standalone
`wineyes-X.Y.Z-x64.exe` you can run from anywhere.

Requires 64-bit Windows 10 or 11 (x64, or ARM64 Windows 11 via emulation).

## Usage

| Action                 | Effect                                    |
|------------------------|-------------------------------------------|
| Left-drag on an eye    | Move the window                           |
| Mouse wheel            | Grow / shrink                             |
| Right-click            | Menu: Always on top, Reset size, Exit     |
| Esc                    | Exit                                      |

## Building

Requires Visual Studio (or the Build Tools) with the C++ desktop workload.

```
build.bat
```

This finds Visual Studio automatically if `cl` isn't already on your `PATH`,
and produces `wineyes.exe`.

To build the installer as well (installs Inno Setup via Chocolatey if it's
missing; output goes to `dist\`):

```
powershell -ExecutionPolicy Bypass -File installer.ps1 -Version 1.2.3
```

The icon `wineyes.ico` is checked in; regenerate it with `make-icon.ps1` if
you change its look.

## Releasing

Pushing a `vX.Y.Z` tag builds the program and installer in GitHub Actions and
publishes them as a GitHub release:

```
git tag -a v1.2.3 -m "wineyes 1.2.3"
git push origin v1.2.3
```

## License

MIT; see [LICENSE](LICENSE).
