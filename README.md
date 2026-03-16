
<h2 id="codeorb-start" style="display:none;"></h2>

[![GitHub](https://img.shields.io/github/license/fractal-programming/code-orb?style=plastic&color=orange)](https://en.wikipedia.org/wiki/GNU_General_Public_License#Version_3)
[![GitHub Release](https://img.shields.io/github/v/release/fractal-programming/code-orb?color=orange&style=plastic)](https://github.com/fractal-programming/code-orb/releases)

![Windows](https://img.shields.io/github/actions/workflow/status/fractal-programming/code-orb/windows.yml?style=plastic&logo=github&label=Windows)
![Linux](https://img.shields.io/github/actions/workflow/status/fractal-programming/code-orb/linux.yml?style=plastic&logo=linux&logoColor=white&label=Linux)
![macOS](https://img.shields.io/github/actions/workflow/status/fractal-programming/code-orb/macos.yml?style=plastic&logo=apple&label=macOS)
![FreeBSD](https://img.shields.io/github/actions/workflow/status/fractal-programming/code-orb/freebsd.yml?style=plastic&logo=freebsd&label=FreeBSD)
![ARM, RISC-V & MinGW](https://img.shields.io/github/actions/workflow/status/fractal-programming/code-orb/cross.yml?style=plastic&logo=gnu&label=ARM%2C%20RISC-V%20%26%20MinGW)

[![Discord](https://img.shields.io/discord/960639692213190719?style=plastic&color=purple&logo=discord)](https://discord.gg/FBVKJTaY)
[![Twitch Status](https://img.shields.io/twitch/status/Naegolus?label=twitch.tv%2FNaegolus&logo=Twitch&logoColor=%2300ff00&style=plastic&color=purple)](https://twitch.tv/Naegolus)

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/fractal-programming/code-orb/main/doc/res/codeorb.jpg" style="width: 700px; max-width:100%"/>
  </kbd>
</p>

## The Microcontroller Debugger

When working with small targets, simple log outputs are often the only feedback available.
With [CodeOrb](https://github.com/fractal-programming/code-orb#codeorb-start) on the PC and the
[SystemCore](https://github.com/fractal-programming/SystemCore#processing-start) on the target,
we have two additional channels: a task viewer and a command interface.
The task viewer provides a detailed insight into the entire system, whereas the command interface gives full control over the microcontroller.

CodeOrb is essentially a multiplexer service running on the PC that transmits and receives these three channels of information via UART from and to the microcontroller.
The channels can then be viewed on the PC or over the network using a Telnet client such as PuTTY.

## Features

- Full control over target
- Crystal-clear insight into the system
- Through three dedicated channels
  - Process Tree
  - Log
  - Command Interface
    - Interactive
    - Automatic

## How To Use

### Topology

This repository provides `CodeOrb` the microcontroller debugger highlighted in orange. Check out the [example for STM32](https://github.com/fractal-programming/hello-world-stm32) as well!

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/fractal-programming/code-orb/main/doc/system/topology.svg" style="width: 300px; max-width:100%"/>
  </kbd>
</p>

### Integrate the SystemCore into the target

TODO: Separate README file

Meanwhile: Check out the [example for STM32](https://github.com/fractal-programming/hello-world-stm32)

### Start CodeOrb on the PC

On Windows
```
.\CodeOrb.exe -d COM1
```

On UNIX systems
```
./codeorb -d /dev/ttyACM0
```

The output should look like this
<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/fractal-programming/code-orb/main/doc/screenshots/Screenshot%20from%202025-06-19%2022-29-20.png" style="width: 700px; max-width:100%"/>
  </kbd>
</p>


### User Interface

As soon as the multiplexing service CodeOrb has been started you can connect to the channels via Telnet. You can use IPv4 or IPv6

For the **Process Tree**
```
telnet :: 3000
```

For the **Log**
```
telnet :: 3002
```

For the **Interactive Command Interface**
```
telnet :: 3004
```

For the **Automatic Command Interface**
```
echo "toggle" | nc :: 3006
external Twitch jobs enabled
```

<p align="center">
  <kbd>
    <img src="https://raw.githubusercontent.com/fractal-programming/code-orb/main/doc/screenshots/Screenshot%20from%202025-05-26%2022-25-18.png" style="width: 700px; max-width:100%"/>
  </kbd>
</p>

## How To Build

### Requirements

You will need [meson](https://mesonbuild.com/) and [ninja](https://ninja-build.org/) for the build.
Check the instructions for your OS on how to install these tools.

### Steps

Clone repo
```
git clone https://github.com/fractal-programming/code-orb.git --recursive
```

Enter the directory
```
cd code-orb
```

Setup build directory
```
meson setup build-native
```

Build the application
```
ninja -C build-native
```
