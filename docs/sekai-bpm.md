# Sekai BPM Base36 compatibility

This profile targets only com.sega.pjsekai.re 1.0.0 / code 200005 in its main process, with the existing game compatibility switch enabled. D4U never enables it. Existing local D4U gRPC changes are carried forward in the isolated build snapshot; their original working tree is not modified.

## Native boundary

The native installer requires libil2cpp.so Build ID `cdd49c9a6324f45258da7e2fa21ac5d918950d4c`, executable ELF segments, and the exact four-word prologues of Regex.ctor (`0x1f6ab34`) and LoadBpmInfo (`0x2fb72e8`). The BL instruction at `0x2fb7348` must be `0x97becdfb`. IL2CPP string exports must resolve to the reviewed library's expected addresses.

Only a Regex constructor invoked with return PC `base+0x2fb734c` and exact UTF-16 pattern `^#BPM(?<bpm_id>\d{2}):(?<bpm>.+)` receives `^#BPM(?<bpm_id>[0-9A-Za-z]{2}):(?<bpm>.+)` instead. The hook forwards every other call unchanged, including identical patterns from other callers. Strings use IL2CPP exports and a newly allocated normal managed argument; the shared original string is never mutated, and no unrooted managed pointer is cached. The constructor ABI is `(Regex*, String*, MethodInfo*) -> void`.

The replacement is a single Regex constructor hook. No LoadBpmInfo wrapper/TLS scope, chart rewriting, BPM rounding, audio conversion, rendering change, global Regex setting or game APK patch is involved. `sekai_bpm_base36_ready` indicates installation; `sekai_bpm_base36_applied` indicates the first actual target replacement. The older app status still describes memory compatibility; use these specific log markers for BPM verification.

## Host validation

`node tools/check_native.mjs` compiles/runs the existing overflow tracker test and new caller/pattern isolation tests. ASCII Base36 definitions include 0A/0Z/1A/ZZ and lowercase IDs; malformed lengths/punctuation, another caller and similar patterns are rejected. C++ regex tests exercise the ASCII class, not the old game's .NET runtime.

`node tools/check_bpm_chart.mjs PATH_TO_SUS` reads a private chart without modifying it and verifies BPM references. The reported 0792_01 MASTER has 117 definitions (old digit-only pattern accepts 39) and 146 nonzero BPM references. Original SHA-256: `951dfa7b3ac3b26c1164bb0b8af1c66a189b328fefeadee65c1c8bb69012f7fc`. No chart or proprietary game data is distributed.

The requesting task's temporary Frida experiment already reached 117 definitions / 146 BPM events / 463 notes, then aborted during later initialization. This does not prove an HCA codec fault or native module fault. First retest the permanent hook without Frida; if the abort persists, compare the same chart with the controlled HCA2 audio and inspect the original abort message/tombstone before any further patch. No phone install, force-stop, restart, hook attachment or volume change is performed by this build task.

## Packaging

`node tools/package_release.mjs --source-ref HEAD` mirrors the existing Python packaging checks without invoking Python, and runs both native test suites. It requires a clean committed tree and the private signing-properties path via environment, then checks release variant, API 102 entry/scope, absent test components, SDK 37, arm64/16KiB ELF, zip alignment, notices and the existing distribution certificate. Source ZIP and checksums refer to that exact commit. Runtime behavior still needs the device-owning task's test; this candidate is not installed or published as a GitHub Release here.
