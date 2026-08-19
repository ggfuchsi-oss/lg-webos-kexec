---
name: webos-tv-surface
description: "rex's webOS TV attack/mod surface - ports, inspector, filesystem"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 502c0e03-103c-42cf-b877-b81bcd0b7466
  modified: 2026-08-16T23:04:14.678Z
---

Recon of rex's LG TV ([[lgtv-toolkit]] `lgtv recon`), webOS 6.5.3:

Filesystem: rootfs is squashfs + read-only overlay over `/overlay/bsppart`.
`/`, `/usr`, `/lib`, `/etc`, `/var` are `ro` — edits there vanish on reboot.
Persistent writable: `/media`, `/var/lib/webosbrew`, `/mnt/lg/user`,
`/mnt/lg/cmn_data`, `/home`. That's why homebrew persistence re-applies from
`/var/lib/webosbrew/startup.sh` at boot instead of patching the rootfs.

Listening ports (0.0.0.0 unless noted): 22 dropbear, 23 telnetd (NO AUTH,
LAN-open), 53 connmand (lo), 1228-2039 upnpd, 3000/3001/18181/36866 xinetd diag
(chargen/daytime/echo), 7000 airplay, 9998 WebAppMgr, 57934/5 amazon-alexa (lo).

Port 9998 = Chrome DevTools Protocol 1.3, Chromium 79. Every running web app is
an inspectable target (live DOM, JS eval in-context, script list). Dev/3rd-party
web apps appear automatically; LG system apps (com.webos.app.*) do NOT unless
WAM is launched with inspection enabled. Bound to localhost — needs ssh tunnel.
Driven by `lgtv eval/dom/websrc/inspect` + `tools/cdp.py`.

~302 luna services on the bus; `lgtv services [filter]` maps name→binary,
`ls-monitor -i <svc>` introspects methods. Per-luna-call latency ~0.12s with ssh
ControlMaster multiplexing (on by default in the toolkit), ~0.49s without.
