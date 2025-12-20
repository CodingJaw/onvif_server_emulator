# AGENT Configuration for Onvif Server Emulator

## Role of This Agent
- Provide consistent assistance for maintaining the ONVIF Server Emulator (camera/NVR emulator) that exposes ONVIF-compatible SOAP/RTSP endpoints.
- Prioritize buildability and protocol correctness over new features; keep behavior predictable for tests using provided configs.
- Document assumptions and limits when implementations are partial; avoid silent API/behavior changes.

## Tech Stack & Build
- Language/standard: C++20.
- Core dependencies: Boost (system, date_time, regex, thread, asio, property-tree, signals2, optional test), GStreamer 1.16+ (core + pango, base/good/ugly/bad, x264/x265, gst-rtsp-server), Simple-Web-Server submodule.
- Build system: CMake (minimum 3.16). Default build type is Debug when unspecified; executable target `main`, static lib `onvif_server`.
- Windows guidance in `BUILD.md` (uses vcpkg). Linux instructions are TODO; prefer mirroring Windows dependency set via pkg-config for GStreamer and system Boost.
- Configuration directory argument: `main.exe <configs_dir>`; defaults to `./server_configs`. CMake copies configs into build output for standalone runs.
- Enable unit tests via `-DENABLE_UNIT_TESTS=ON` to build `unit_tests` subdirectory.

## Architecture Overview
- Entry point: `main.cpp` resolves config directory, sets up logger (console/file via `LoggerFactories`), warns on missing `GST_PLUGIN_PATH`, then constructs `osrv::Server` to initialize and run services.
- Library layout (`onvif_server`):
  - Core server wrappers: `Server` (HTTP/SOAP + RTSP wiring), `RtspServer`, interfaces in `include/IOnvifServer.h`, forward decls in `HttpServerFwd.h`.
  - Logging: `Logger.h` plus console/file/stream factories (`include/ConsoleLogger.h`, `FileLogger.h`, `StreamLogger.h`, `LoggerFactories.h`).
  - ONVIF services (namespace `osrv`): base `IOnvifService` manages handlers, configs, namespaces. Concrete services under `onvif_services/` include Device, DeviceIO, Media/Media2, Event (with PullPoint/event_generators), Imaging, PTZ, Recording Search, Replay Control, Discovery, and physical components (e.g., `IDigitalInput`). Requests are defined in `onvif/OnvifRequest.h` and per-service handler files.
  - Configuration helpers: `include/onvif_services/service_configs.h` with impl in `src/onvif_services/service_configs.cpp` to load JSON configs into property trees.
- Utility helpers: HTTP/SOAP/auth parsers (`utility/HttpHelper`, `SoapHelper`, `AuthHelper`, `HttpDigestHelper`), media/ptz configuration readers, event service support, XML parsing (`XmlParser`), and media source readers (`VideoSourceReader`, `AudioSourceReader`).
- Third-party submodule: `Simple-Web-Server` added via `add_subdirectory` and needs to be present or initialized by CMake.
- Config-driven behavior: JSON in `server_configs/` (and related directories) shapes service availability, authentication modes, logging level, port forwarding, digital inputs, event PullPoint settings, discovery responses, and recording search windows (see README).

### Utility folder notes
- Date/time helpers live in `utility/datetime.hpp` and support composing ONVIF `tt:Date`/`tt:Time` structures for Device service handlers (e.g., GetSystemDateAndTime/SetSystemDateAndTime). Reuse these utilities when adjusting clock-related logic instead of duplicating conversion code.
- Other helpers under `utility/` provide SOAP parsing, HTTP helpers, digest auth, XML parsing, and media configuration readers; prefer extending these modules when adding similar functionality.

## Coding Rules & Conventions
- Use C++20 features already present; prefer standard library utilities and `std::shared_ptr` as used throughout services.
- Keep headers self-contained and guarded with `#pragma once`; follow existing include style (project headers in quotes, system/Boost in angle brackets).
- Maintain namespace `osrv` for server code; avoid introducing new global namespaces.
- Preserve logging patterns: use `ILogger` interface, keep log level checks minimal, and ensure user-facing warnings/errors remain informative.
- Avoid broad behavior changes to SOAP/RTSP responses; extend via service handlers while keeping existing request mappings.
- Keep configuration keys backward compatible; when adding new options, set sensible defaults and document in README/BUILD/config comments if user-facing.
- Do not wrap imports/includes in try/catch. Keep exception handling localized around operational code paths (e.g., server initialization) consistent with current style.
- When touching submodule code, prefer contributing via upstream unless necessary; note submodule updates explicitly.

## Response / Patch Style for Future AI
- Summaries must cite files/line ranges touched. Tests/checks list commands with emoji prefixes (✅ pass, ⚠️ warning/limitation, ❌ fail) and cite terminal output.
- Keep changes minimal and scoped. Explain any partial implementations or limitations in PR body and commit message when relevant.
- For configuration or protocol changes, describe expected client impact and how to exercise via `server_configs`.

## Limits, State of the Project, and Outstanding Work
- Completed/available: core ONVIF services scaffolding; config-driven setup; logging; RTSP/HTTP server integration; Windows build recipe; submodule wiring.
- Partial/needs attention: Linux build instructions (marked TODO); some config options may be hardcoded or ignored (see README notes for Event PullPoint timeouts, Discovery static responses, Media2 enablement); validate that `server_configs` options affect runtime behavior before relying on them.
- External dependencies (Boost/GStreamer) must be available on host; missing `GST_PLUGIN_PATH` triggers warnings but may degrade media features.
- Unit tests are optional and off by default; CI expectations may vary—run relevant tests when altering service logic.

## Event subscription / PullPoint guidance
- PullPoint subscriptions now support topic-filtered generators, bounded per-subscription queues, and proper WS-Notification faults (`wstop:ResourceUnknown`) for unknown/expired references across PullMessages, Renew, and Unsubscribe paths.
- Long-poll waits honor zero/positive client timeouts while respecting the `IgnoreClientsTimeout` config; expiration timers cancel pending waits to prevent success responses after expiry.
- When implementing new event generators or subscription-dependent features, attach them through the existing NotificationsManager/PullPoint flow instead of bypassing the filters/queueing.

## How to Build & Run (Quick Reference)
1. Initialize submodules if missing: `git submodule update --init --recursive` (or ensure `Simple-Web-Server/CMakeLists.txt` exists).
2. Configure build:
   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```
   Add toolchain/pkg paths for Boost/GStreamer as needed (see `BUILD.md` for Windows example).
3. Build targets:
   ```
   cmake --build build
   ```
4. Run server (from repo or build dir) specifying configs if not default:
   ```
   ./build/main ./server_configs
   ```

