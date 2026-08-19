# RexBus — RexOS-TV native IPC

The "better than luna" idea: luna is JSON-over-unix-socket + a permission gate
over ~302 services. Our custom OS deserves a typed, low-latency bus from the
start. We carry the **harvested webOS luna schema** in as the contract.

- `schema.json` — copied from `~/lgtv-toolkit/luna2/schema.json`: **195 services,
  4064 methods** (the full bus map). This is the IPC surface we want to expose
  natively on RexOS-TV.
- `rexbus-stub.py` — a placeholder daemon: loads the schema, answers a health
  ping, and prints the service count. Real transport (shared-mem ring / VCPU
  IPC / unix sockets) lands in a later phase. It runs on the PC for now so the
  schema is exercised, and is the seed for the on-TV bus.

## Roadmap

1. ✅ harvest schema (done, in `luna2/`)
2. 🟡 stub daemon (this dir)
3. ⏳ define the on-TV transport (reuse the VCPU RPC ring we'll need for graphics)
4. ⏳ typed client (port `lunabus.py` semantics: `tv.audio.getVolume()` etc.)
5. ⏳ gateway (HTTP/SSE) like the webOS `gateway.py`
