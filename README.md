# StaxStudio

StaxStudio is a lightweight, Windows-first, open-source screen recording and
live-streaming application.

Our goal is simple: OBS-level performance, Streamlabs-level simplicity, and a
modern interface that makes professional recording and streaming approachable.

## Current status

StaxStudio has a Qt/QML Studio workspace, Windows capture and audio inputs,
local recording, Custom RTMP streaming, and in-development platform connections.
The `develop` branch is active development; `main` contains validated checkpoints.

## Planned direction

- Low-overhead Windows capture with platform-specific implementations isolated
  behind core interfaces.
- Modular audio, encoder, recording, streaming, and pipeline components.
- Hardware encoder support where available, with a software fallback.
- A clear Qt/QML UI that keeps advanced controls available without overwhelming
  new users.

## Development

The application requires a C++20 compiler, CMake 3.24 or later, Qt 6 with Core,
Network, QML, Quick, and Quick Controls 2, and the FFmpeg development libraries.

### YouTube connection (development)

YouTube sign-in requires a Google Cloud OAuth **Desktop app** Client ID.
Enable the YouTube Data API v3 for that project, configure its OAuth consent
screen, and add your account as a test user while the app is in testing mode.
The channel must be eligible for YouTube Live. A distributor can configure the
public application Client ID at build time. Do not put OAuth tokens or other
private credentials in this repository.

```powershell
cmake -S . -B C:\Users\omenh\StaxStudio-build2 -DSTAX_GOOGLE_OAUTH_CLIENT_ID="YOUR_DESKTOP_CLIENT_ID.apps.googleusercontent.com"
```

For development, the following environment variable overrides the configured
public ID. It is not required for ordinary users of a configured build.

On Windows, set the Client ID for your user account and then restart StaxStudio:

```powershell
setx STAX_GOOGLE_CLIENT_ID "YOUR_DESKTOP_CLIENT_ID.apps.googleusercontent.com"
```

Open Stream Setup, select YouTube, and choose **Connect YouTube**. The system
browser handles consent and returns to a temporary `127.0.0.1` callback.
StaxStudio uses PKCE and stores tokens in Windows Credential Manager. Disconnect
removes the saved credentials. A confidential client secret is not required for
Google's Desktop app flow.

### Twitch connection (development)

Create a **public** Twitch developer application and provide its Client ID. The
device-code flow uses the system browser and does not bundle or require a Twitch
client secret. After authorizing, StaxStudio loads the channel, can update its
title, category, language, and tags, and can obtain the stream key using the
authorized Twitch API. It selects an ingest server from Twitch's official ingest
list. A distributor can configure the public ID at build time with
`-DSTAX_TWITCH_OAUTH_CLIENT_ID="YOUR_TWITCH_CLIENT_ID"`. For development,
set the override below and restart StaxStudio:

```powershell
setx STAX_TWITCH_CLIENT_ID "YOUR_TWITCH_CLIENT_ID"
```

### Kick connection (development)

Kick currently requires both a Client ID and a **client secret** for its OAuth
token exchange, even with PKCE. Use credentials from your own Kick developer app;
never commit them to Git or distribute them as shared desktop credentials.
The current environment-variable flow is for development only. A consumer
release needs a secure token-exchange service before Kick account connection
can be offered without exposing the client secret.
Register `http://localhost:49494/oauth/callback` as that app's exact redirect
URI, then set these values and restart StaxStudio:

```powershell
setx STAX_KICK_CLIENT_ID "YOUR_KICK_CLIENT_ID"
setx STAX_KICK_CLIENT_SECRET "YOUR_KICK_CLIENT_SECRET"
setx STAX_KICK_REDIRECT_PORT "49494"
```

StaxStudio uses the system browser, a localhost callback, PKCE and state
validation, and Windows Credential Manager for user tokens. It can load your
Kick account and update documented channel title/category metadata. Kick's
published channel schema currently does not define an ingest URL or stream key
field, so the Kick page does not offer one-click Go Live. Use Custom RTMP with
credentials obtained from your Kick dashboard until a supported API contract is
available. Streaming to multiple destinations at once is not yet supported.

### Recording diagnostics

Detailed recording diagnostics are off by default. Enable them for new sessions
under **Settings → Advanced → Diagnostics**. They are written to the application's
local data `logs` folder, never beside the recording. **Export Diagnostics**
creates a sanitized JSONL copy for troubleshooting. Existing media and older
diagnostic files are left untouched.

## License

Copyright (C) 2026 MrHassaanX.

StaxStudio is licensed under the GNU General Public License, version 3 or any
later version. See [LICENSE](LICENSE) for the full license text.
