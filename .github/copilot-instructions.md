# GitHub Copilot Instructions

## Project overview

**Tux Manager** is a Linux system monitor application built with **Qt6** (qmake, `.ui` files).
It is inspired by the Windows Task Manager but targets Linux and aims to expose significantly more detail.

## Key architecture

- `MainWindow` hosts a `QTabWidget` with four top-level tabs: **Processes**, **Performance**, **Users**, **Services**.
- Each tab is an independent `QWidget` subclass with its own `.h`, `.cpp`, and `.ui` file.
- `Configuration` (singleton, access via `CFG->…`) persists all user settings through `QSettings`.
- `Logger` (singleton) provides structured, levelled logging via the macros `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`.

## Coding style

Follow the guidelines in [CODING_STYLE.md](../src/CODING_STYLE.md).

## Important conventions

- Always register new source files in `SystemInfo.pro` (`SOURCES`, `HEADERS`, `FORMS`).
- New persisted settings belong in `Configuration`: add a public member with a default value, and a matching `s.value()`/`s.setValue()` call pair in `Load()`/`Save()`.
- Use `LOG_*` macros instead of `qDebug()` / `printf`.
- Use `CFG->…` instead of passing configuration down through constructors.
- Prefer `QString::arg()` for string formatting; avoid `std::string` unless interfacing with non-Qt APIs.
- All UI strings that may be shown to the user must be wrapped in `tr()`.
