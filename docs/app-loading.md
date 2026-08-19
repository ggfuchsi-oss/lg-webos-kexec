---
name: webos-sam-app-loading
description: how SAM loads/registers apps on webOS 6.5 and the dev/listApps trap
metadata: 
  node_type: memory
  type: reference
  originSessionId: 502c0e03-103c-42cf-b877-b81bcd0b7466
  modified: 2026-08-16T23:04:02.673Z
---

On webOS 6.5 (verified on rex's LG TV, [[lgtv-toolkit]]), SAM
(com.webos.applicationManager, /usr/sbin/sam) discovers apps from paths in
`/etc/palm/sam-conf.json` → `ApplicationPaths`. The `typeByDir:"dev"` path is
`/media/developer/apps/usr/palm/applications`.

SAM reads `appinfo.json` from that dir **on demand** — no inotify watch on the
app dirs (its only inotify watches are `/tmp/systrim`), no rescan method
(`dev/rescan` does not exist). So copying a directory into that path IS
installing an app; it is live the instant the files land — no installer, no
signature, no restart. This is what `lgtv hotload` exploits (~3s cycle).

THE TRAP: `luna://com.webos.applicationManager/dev/listApps` is a partial/stale
cache. An app can be fully installed, launchable, and in the launcher while
absent from that list. Use `/listApps` (non-dev) instead — `getAppInfo` and
`launch` are the reliable "is it registered?" checks. This cost ~an hour of
chasing a nonexistent "registration failure".

`appInstallService/dev/install` only ACKs (returnValue:true) then unpacks
async; it queries SAM but never pushes a registration. `opkg` targets the
read-only/full rootfs and is NOT a usable install fallback; raw filesystem copy
is. The jailer sandbox for dev apps is still active (gated on absent
`/var/luna/preferences/jailer_disabled`); a `--service` JS service runs as root
and sidesteps it.
