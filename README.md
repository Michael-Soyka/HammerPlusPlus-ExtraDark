# Hammer++ extra dark patcher

<img width="2560" height="1408" alt="image" src="https://github.com/user-attachments/assets/f98d41db-e120-4fe8-b169-d3a134540dab" />
<p align="center">Hammer++ - dark theme</p>

Our in-house patcher for [Hammer++](https://ficool2.github.io/HammerPlusPlus-Website/).

English Version | [Русская версия](README_ru.md)

What it does:
- Dark theme

## Installation

**Hammer++ must be closed.**

Run **hppxdark_setup.exe** (unpack the release first): enter the game `bin` folder, pick it with **Browse...** or drop it on the window, then press **Patch**. On a patched Hammer++ there are three buttons instead: **Remove** brings back the original, **Repatch** writes `hppxdark.dll` again (named **Update** when the installed one is outdated), **Launch** starts Hammer++.

The window also shows the installed version next to the installer's one and whether it is outdated.

The installer finds `hammerplusplus.exe` in `bin`, `bin\x64` or `bin\win64` and patches `hammerplusplus\bin\hammerplusplus_dll.dll` (the original is saved as `hammerplusplus_dll.dll.orig`). An install made by an earlier version of the patcher (a patched `hammerplusplus.exe`) is rolled back automatically.

## Configuration

The `hppxdark.ini` file next to `hppxdark.dll`, in `hammerplusplus\bin`:

```ini
[DarkTheme]
Enabled=1    ; 0 - light theme / 1 - custom dark theme
```

### Palette

Dark theme colors are set in the `[Palette]` section, as `#RRGGBB` or `R,G,B`.

*Every key is optional: missing ones keep their defaults, and a value that can't be parsed is not applied (a build with `HPPXDARK_LOG` notes it in the log).*

```ini
[Palette]
Face=#2D2D30
Text=220,220,220
```

| Key | Default | Used for |
|-|-|-|
| `Face` | `45,45,48` | background of panels, dialogs, buttons, toolbars, splitters |
| `Window` | `30,30,30` | background of lists, trees, views; unselected tabs |
| `Edit` | `37,37,38` | background of input fields and `ListBox` lists |
| `Text` | `220,220,220` | text of windows, menus, buttons, tooltips |
| `GrayText` | `128,128,128` | disabled text, text of unselected tabs |
| `Menu` | `43,43,43` | menu bar and menus |
| `MenuHot` | `62,62,66` | menu bar item under the cursor |
| `Highlight` | `0,120,215` | selection in lists and fields |
| `HighlightText` | `255,255,255` | selection text |
| `Light` | `62,62,66` | light edge of 3D borders, scroll bars |
| `Hilight` | `80,80,84` | highlight of 3D borders |
| `Shadow` | `28,28,30` | shadow of 3D borders |
| `DarkShadow` | `15,15,15` | dark shadow of 3D borders |
| `Tooltip` | `50,50,52` | tooltip background |
| `HotLight` | `86,156,214` | links and hot-tracked items |
| `Workspace` | `28,28,28` | background of the empty MDI area |
| `Frame` | `20,20,20` | window frames |
| `Border` | `100,100,104` | flat borders of fields, tabs, group boxes and the status bar |

## Requirements

- Hammer++ 8873, 32- or 64-bit: patching checked on the 2013mp, 2013sp, l4d2, portal2 (x86), gmod and tf2 (x64) builds
- Visual Studio \>=2022
- The "Desktop development with C++" workload
- CMake (may come with the workload)


## Building

```
cmake -B build -A x64
cmake --build build --config Release
```

The result is `bin\hppxdark_setup.exe` with both proxy DLLs inside: the x64 configuration builds the x86 `hppxdark.dll` as a nested Win32 configuration. 
## Versions

`MAJOR.MINOR.BUILD.REVISION`, written on every build by `cmake/version.cmake`:
- `MAJOR.MINOR` is set by hand in `project( hppxdark VERSION ... )` of `CMakeLists.txt`;
- `BUILD` is the number of days since 2000-01-01, `REVISION` the seconds since midnight divided by 2 (UTC), the same scheme as .NET `1.0.*` versions.

A later build always has a bigger version, the installer and both DLLs of one build share it. GitHub Actions builds add `-git` to the text version (`1.0.9812.38990-git`); the comparison goes by the numbers. The version goes to the version resource of `hppxdark.dll` and `hppxdark_setup.exe` (Properties - Details in Explorer); the installer compares the installed `hppxdark.dll` with its own.

CMake options:
- `HPPXDARK_LOG` - process logging, off by default.

## Authors

- Ficool2 - Creator of the Hammer++ :blue_heart:
- Michael Soyka (Moon-6) - Structure, proj direction, tests
- Claude - General patcher code
