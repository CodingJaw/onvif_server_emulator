# Rust helper for the ExternalToggle generator

This helper binary provides a minimal way to toggle the `ExternalToggle` event generator without sending ONVIF SOAP requests. It connects to the emulator's TCP listener and sends ASCII `ON`/`OFF` messages (terminated by a newline) that the server translates into calls to the generator's `Trigger` method.

## Configuration
1. Enable the generator and IPC listener in `server_configs/event.config`:
   - Set `ExternalToggle.GenerateEvents` to `true`.
   - Ensure `ExternalToggle.IpcPort` and `ExternalToggle.IpcAddress` point to an available local endpoint (defaults: `127.0.0.1:10080`).
2. Start the emulator with your configs directory (`./build/main ./server_configs`). When the external generator is enabled, the C++ server registers it for both the TCP listener and an exported C ABI shim (`osrv_external_toggle_trigger`).

## Building and running the helper
```bash
cd rust_bridge
cargo run --release
```

- Override the target address with the `EXTERNAL_TOGGLE_ADDR` environment variable (`host:port`).
- At runtime, type `on`, `off`, or `toggle` to push a new state; `quit` exits.

## Wire protocol
- Transport: TCP
- Message: ASCII `ON\n` or `OFF\n` (case-insensitive). The listener also accepts `1\n`/`0\n` or `true\n`/`false\n`.

## C ABI entry points
If you want to call into the server process directly (e.g., from another native module), two C symbols are exported by the main binary:
- `void osrv_external_toggle_trigger(int state);` — non-zero triggers `true`, zero triggers `false`.
- `int osrv_external_toggle_available();` — returns `1` when the generator has been registered.

These are available once the server has created and registered the external generator (i.e., when `ExternalToggle.GenerateEvents` is `true`).
