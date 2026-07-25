# Tron Light Pods

![C++](https://img.shields.io/badge/C%2B%2B-C%2B%2B20-00599C?style=flat&logo=cplusplus&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3_Core-5586A4?style=flat&logo=opengl&logoColor=white)
![Windows](https://img.shields.io/badge/Platform-Win32-0078D6?style=flat&logo=windows&logoColor=white)
![CMake](https://img.shields.io/badge/Build-CMake_%2B_Ninja-064F8C?style=flat&logo=cmake&logoColor=white)
![GCC](https://img.shields.io/badge/Compiler-GCC-4EAA25?style=flat&logo=gnu&logoColor=white)
![License](https://img.shields.io/badge/License-All_Rights_Reserved-red?style=flat)

A game inspired by the light cycle game from the original 1982 movie, **Tron**.

<p align="center">
  <img src="assets/video/demo-1.webp" alt="Tron Light Pods">
</p>

**Tron Light Pods** is a game demo written in 1999 to attract VC capital for a new game studio. The project ultimately never got off the ground, but the demo remains.

## 📚 Table of Contents

- [🔨 Building](#-building)
- [🎮 Playing](#-playing)
- [📄 License](#-license)

## 🔨 Building

Requires a C++20 compiler, CMake 3.20 or newer, and Ninja. On Windows, MSYS2 or MinGW-w64 both work. The renderer is Win32 and WGL, so the game is Windows-only.

```
cmake -S src -B build -G Ninja
cmake --build build
```

That produces `build/tron-light-pods-1-9.exe`. It compiles clean under `-Wall -Wextra`.

## 🎮 Playing

```
build\tron-light-pods-1-9 [--opponents N] [--grid N] [--seed N] [--window WIDTHxHEIGHT]
```

You are in a cubic arena, riding a light pod that never stops. Turns do not take effect immediately, they are *queued*, and commit when the pod reaches the next grid node, which is what the arrow indicator in the HUD is showing you. Everyone leaves a solid trail behind them, including you. The goal is to block opponents so that they collide with your light trail, until you are the last player remaining. 

| Key | Action |
|---|---|
| Arrows / numpad | Navigate the menu; steer in play. |
| Enter | Activate the selected menu item. |
| Esc | Back out to the menu from play, and to Exit Game at the menu. |
| Shift / Ctrl | Accelerate and brake (numpad `+` and `−` also work). |
| N | Toggle the proximity sensor. |
| L | Show your own trail in the chase view. |
| F1 / F2 / F3 | Cockpit, third person chase, and spot-plane views. |
| F12 | Debug view. Orbit the arena from a distance, with the player removed. |

## 📄 License

**All rights reserved**, 1999<br>This code is published for reference and study, not for reuse. It is not open source. See [LICENSE](LICENSE) for the full terms.
