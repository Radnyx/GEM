# GEM
Gameboy Emulator (2015)

Features:

* Includes debugger to disassemble and step through assembly instructions and a memory inspector.

* Supports basic TAS (Tool-Assisted-Speedrun) input playback, but generally desyncs after a few minutes.

Future work:

* Since `0xCB` extension opcodes are uncommon, they have only been implemented on a per need basis.
* Proper bank switching.

## Demo
https://www.youtube.com/watch?v=Wyak6hNqcgI

![Game Boy Emulator](https://github.com/Radnyx/GEM/assets/10569289/0b8284bc-77e0-4485-b37f-09a40c424e8c)

## References

CPU Instruction Set: https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html

I can't recall which spec I used, but the Game Boy architecture is incredibly well documented. Here are some great references:

* http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf
* https://gekkio.fi/files/gb-docs/gbctr.pdf
* https://ia903208.us.archive.org/9/items/GameBoyProgManVer1.1/GameBoyProgManVer1.1.pdf

## Disclaimer

I am not affiliated with Nintendo.

Please do not distribute dumped games or firmware that is under copyright. Respect the copyright laws where you live.
