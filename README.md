# Tux Manager

A Linux Task Manager alternative built with Qt6, inspired by the Windows Task Manager but designed to go further - providing deep visibility into system processes, performance metrics, users, and services.

## Memory view
![Screenshot](screenshots/readme.png)
## CPU view
![Screenshot](screenshots/cpu.png)
## GPU view
![Screenshot](screenshots/gpu.png)

## Building

### qmake

```bash
# cd to root of the repo and then:
mkdir build && cd build
qmake6 ../src
make -j$(nproc)
./tux-manager
```

## Core philosophy and goals of this project

* KISS - keep it simple stupid
* Lean and clean codebase, minimal system footprint (low RAM and CPU usage)
* Stability and reliability, easy debugging
* No overengineered or unnecessary extra features
* Simple packaging flow - for each packaging tool, there should be a script or 1 line command
* Minimal dependencies on 3rd party libs besides Qt so that building anywhere should be trivial
* Keep everything well documented

## License

GPL-3.0-or-later
