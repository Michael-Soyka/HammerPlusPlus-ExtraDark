# Hammer++ extra dark patcher

<img width="2560" height="1408" alt="image" src="https://github.com/user-attachments/assets/f98d41db-e120-4fe8-b169-d3a134540dab" />
<p align="center">Hammer++ - dark theme</p>

Our in-house patcher for [Hammer++](https://ficool2.github.io/HammerPlusPlus-Website/).

English Version | [Русская версия](README_ru.md)

What it does:
- Forces Hammer++ to start through Vulkan (DXVK)
- Dark theme

## Installation

**Hammer++ must be closed.**

To install the patch, run **patch.bat** (unpack the release first if you downloaded one).

To restore the original, open a terminal in the patcher folder and run:
```
patch.bat -Restore
```

Alternatively, from a terminal:
```
patch.bat C:\path\to\bin (-Restore)
```

## Configuration

The `hppxdark.ini` file next to `hppxdark.dll`:

```ini
[ForceVulkan]
Enabled=1    ; 0 - D3D9 (default) / 1 - auto -vulkan

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

- Hammer++ (8872) for 64-bit Source SDK 2013 games, installed in the game's `bin/x64`
- Visual Studio \>=2022
- The "Desktop development with C++" workload
- CMake (may come with the workload)


## Building

```
cmake -B build -A x64
cmake --build build --config Release
```

CMake options:
- `HPPXDARK_LOG` - process logging, off by default.
- `OTM_BUILD`    - for in-house builds.

## Authors

- Ficool2 - Creator of the Hammer++ :blue_heart:
- Michael Soyka (Moon-6) - Structure, proj direction, tests
- Claude - General patcher code
