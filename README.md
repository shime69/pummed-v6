# pummed - CS:GO HvH Cheat v6

[![Educational Purpose Only](https://img.shields.io/badge/Purpose-Educational-blue.svg)](https://github.com/yourusername/pummed)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C.svg)](https://isocpp.org/)
[![Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](https://windows.com)

> **⚠️ IMPORTANT DISCLAIMER**: This project is created **SOLELY for educational purposes**. It demonstrates reverse engineering concepts, memory management, and game hacking techniques in a controlled environment. Using this against Valve's Terms of Service may result in VAC bans. The author does not condone cheating in online games.

## 📋 Overview

**Pummed v6** is a feature-rich HvH (Hack vs Hack) cheat for Counter-Strike: Global Offensive. Built entirely in C++, this DLL injection-based cheat provides various enhancements for educational understanding of game internals and competitive hacking mechanics.

## 🚀 Features

### Core Features
- **Aimbot**: Advanced target acquisition with multiple bone selection options
- **Triggerbot**: Automated firing when crosshair is on enemy
- **Ragebot**: Full-auto aim with hitchance and minimum damage settings
- **Visuals**: ESP boxes, health bars, player names, and weapon info
- **Misc**: Bhop, thirdperson, radar, and more

### Technical Specifications
- **Language**: C++ (C++17 standard)
- **Architecture**: x86 (32-bit)
- **Injection Method**: Manual mapping / LoadLibrary
- **Build System**: Visual Studio 2019/2022 solution
- **Dependencies**: MinHook (included as submodule)

## 🔧 Building from Source

### Prerequisites
- Visual Studio 2019 or 2022 with Desktop development with C++ workload
- Windows SDK (10.0+)
- Git (for cloning with submodules)

### Build Steps

1. **Clone the repository with submodules**
   ```bash
   git clone --recursive https://github.com/shime69/pummed-v6.git
   cd pummed-v6
Open the solution

Navigate to pummed/pummed.sln

Open with Visual Studio

Select build configuration

Release | x86 for release build

Debug | x86 for debug build with symbols

Build the project

Press Ctrl+Shift+B or go to Build → Build Solution

The compiled DLL will be in pummed/Release/ or pummed/Debug/

Building via Command Line
bash
# Using MSBuild
msbuild pummed.sln /p:Configuration=Release /p:Platform=Win32

# Using devenv
devenv pummed.sln /Build Release


🛠️ Technologies Used
C++17: Modern C++ features

WinAPI: Windows system interaction

DirectX 9: Rendering and overlay

MinHook: Minimalistic x86/x64 API hooking library

Pattern Scanning: Dynamic signature scanning for offsets

## 📚 Educational Concepts Demonstrated
DLL Injection: Understanding process memory manipulation

API Hooking: Intercepting game functions (CreateMove, PaintTraverse, etc.)

Memory Management: Reading/writing process memory

Reverse Engineering: Understanding game structures and classes

Rendering Techniques: Drawing overlays with DirectX

Multithreading: Managing concurrent operations

Vector Mathematics: Aimbot calculations, trajectory prediction

## 🚫 Anti-Cheat Considerations
This project is for educational research only. Be aware that:

VAC (Valve Anti-Cheat) detects memory modifications

Online use will result in permanent account bans

Offline mode is the only safe environment for testing

## 🔒 Legal & Ethical Notice
THIS SOFTWARE IS PROVIDED FOR EDUCATIONAL PURPOSES ONLY.
BY USING THIS SOFTWARE, YOU AGREE TO THE FOLLOWING:

1. You will not use this software on VAC-secured servers
2. You understand that using cheats violates CS:GO's Terms of Service
3. You accept full responsibility for any consequences
4. You will use this knowledge ethically and responsibly
5. This project does not encourage or promote cheating in online games

The authors are not responsible for any misuse of this software.
## 🤝 Contributing
This is an educational project. Contributions that improve:

Code quality and documentation

Educational explanations

Build process improvements

Bug fixes

are welcome. Please create a pull request with clear descriptions of changes.

## ⚖️ License
This project is licensed under the MIT License - see the LICENSE file for details.
Note: The educational purpose clause overrides any commercial implications.
