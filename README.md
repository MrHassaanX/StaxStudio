# StaxStudio

StaxStudio is a lightweight, Windows-first, open-source screen recording and
live-streaming application.

Our goal is simple: OBS-level performance, Streamlabs-level simplicity, and a
modern interface that makes professional recording and streaming approachable.

## Current status

StaxStudio is in its foundation phase. The repository currently contains the
CMake, Qt 6, and QML application scaffold, plus a non-functional navigation UI.
Capture, audio, encoding, recording, and streaming are not implemented yet.

## Planned direction

- Low-overhead Windows capture with platform-specific implementations isolated
  behind core interfaces.
- Modular audio, encoder, recording, streaming, and pipeline components.
- Hardware encoder support where available, with a software fallback.
- A clear Qt/QML UI that keeps advanced controls available without overwhelming
  new users.

## Development

The foundation requires a C++20 compiler, CMake 3.24 or later, and Qt 6.5 or
later with Core, QML, Quick, and Quick Controls 2.

## License

Copyright (C) 2026 MrHassaanX.

StaxStudio is licensed under the GNU General Public License, version 3 or any
later version. See [LICENSE](LICENSE) for the full license text.
