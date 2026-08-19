---
name: better-luna-project
description: "the \"better than luna\" client/gateway being built over the TV bus"
metadata: 
  node_type: memory
  type: project
  originSessionId: 502c0e03-103c-42cf-b877-b81bcd0b7466
  modified: 2026-08-16T23:45:21.083Z
---

rex's flagship goal on the TV ([[lgtv-toolkit]]): replace luna's clunky
ergonomics with something smarter, eventually the native IPC of a kexec'd custom
OS ([[webos-boot-security]]). Building it in tiers, in `~/lgtv-toolkit/luna2/`.

Done as of 2026-08-17:
- Tier 1: harvested the whole bus -> schema.json (195 services, 4064 methods,
  993 public). `lunabus.py` = typed client: `tv.audio.getVolume()`, typos caught
  locally with suggestions, cross-bus `search()`, REPL.
- Tier 2: `gateway.py` = stdlib HTTP/SSE server + `ui.html` browser control
  panel. `lgtv gateway` launches it on 127.0.0.1:8730. POST /call, GET /stream
  (SSE subscriptions, live), /schema.json, /health. Every endpoint verified.

Transport lives in bin/lgtv: `lgtv luna` (one-shot), `lgtv luna-stream`
(subscriptions -> NDJSON via tools/ndjson_luna.py). Subscriptions only stream
with `luna-send -i -n N -f … </dev/null` over a pty; tr and python stdin
iteration both block-buffer (use python -u + iter(readline,"")). Method names
can repeat across categories (/getVolume vs /vvm/getVolume) - carry the exact
category, don't guess.

Next: carry the same schema+client into the custom OS as native IPC. Roadmap
tiers A(done Tier1/2)/B kexec PoC/C custom kernel/D OP-TEE audit from
[[webos-boot-security]] still open.
