# SystemInfo

A Linux system monitor application built with Qt6, inspired by the Windows Task Manager but designed to go further — providing deep visibility into system processes, performance metrics, users, and services.

## Features

| Tab | Description |
|-----|-------------|
| **Processes** | List and manage running processes (PID, CPU, memory, user, etc.) |
| **Performance** | Real-time graphs for CPU, memory, disk, and network usage |
| **Users** | Active user sessions and per-user resource consumption |
| **Services** | systemd service status and basic management |

## Requirements

- Qt 6.x (widgets module)
- CMake 3.16+ **or** qmake (project ships with a `.pro` file)
- GCC / Clang with C++17 support
- Linux (primary target)

## Building

### qmake

```bash
mkdir build && cd build
qmake ../SystemInfo.pro
make -j$(nproc)
./SystemInfo
```

### Qt Creator

Open `SystemInfo.pro` in Qt Creator and press **Run**.

## Project Structure

```
SystemInfo/
├── main.cpp
├── mainwindow.{h,cpp,ui}       # Main window — hosts the tab widget
├── processeswidget.{h,cpp,ui}  # Processes tab
├── performancewidget.{h,cpp,ui}# Performance tab
├── userswidget.{h,cpp,ui}      # Users tab
├── serviceswidget.{h,cpp,ui}   # Services tab
└── SystemInfo.pro
```

## License

MIT
