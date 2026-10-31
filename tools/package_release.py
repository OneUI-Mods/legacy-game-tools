#!/usr/bin/env python3
"""Package only a verified signed release and source archive from a clean Git revision."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
import struct

ROOT = Path(__file__).resolve().parent.parent
CERTIFICATE = "f3a3227d5836f3906b033b82e894b493f82b779fda1d3972b30ebfd7e2cff3f0"


def run(*args, capture=False):
    return subprocess.run(
        [str(a) for a in args], cwd=ROOT, check=True, text=True, capture_output=capture
    )


def validate(apk, tools):
    badging = run(tools / "aapt2", "dump", "badging", apk, capture=True).stdout
    if (
        "package: name='dev.ryuya.legacygametools'" not in badging
        or "application-debuggable" in badging
        or "targetSdkVersion:'37'" not in badging
    ):
        raise RuntimeError("Unexpected or debuggable APK")
    manifest = run(
        tools / "aapt2",
        "dump",
        "xmltree",
        apk,
        "--file",
        "AndroidManifest.xml",
        capture=True,
    ).stdout
    if ("io.github.libxposed.service.XposedProvider" not in manifest
            or "dev.ryuya.legacygametools.XposedService" not in manifest):
        raise RuntimeError("Missing official API 102 app-side Binder provider")

    if any(
        word in manifest.lower()
        for word in [
            "testactivity",
            "probeactivity",
            "fixtureactivity",
            "instrumentation",
        ]
    ):
        raise RuntimeError("Test component in release")
    with zipfile.ZipFile(apk) as archive:
        prefix = "META-INF/xposed/"
        props = archive.read(prefix + "module.prop").decode()
        if "minApiVersion=102" not in props or "targetApiVersion=102" not in props:
            raise RuntimeError("API 102 metadata missing")
        if (
            archive.read(prefix + "java_init.list").decode().strip()
            != "dev.ryuya.legacygametools.HookEntry"
        ):
            raise RuntimeError("Unexpected module entry")
        if (
            archive.read(prefix + "native_init.list").decode().strip()
            != "liblegacycompat.so"
        ):
            raise RuntimeError("Unexpected native module entry")
        if "lib/arm64-v8a/liblegacycompat.so" not in archive.namelist():
            raise RuntimeError("Native compatibility library missing")
        native = archive.read("lib/arm64-v8a/liblegacycompat.so")
        if native[:6] != b"\x7fELF\x02\x01":
            raise RuntimeError("Expected a little-endian 64-bit ELF library")
        phoff = struct.unpack_from("<Q", native, 32)[0]
        phentsize, phnum = struct.unpack_from("<HH", native, 54)
        for number in range(phnum):
            header = phoff + number * phentsize
            kind = struct.unpack_from("<I", native, header)[0]
            alignment = struct.unpack_from("<Q", native, header + 48)[0]
            if kind == 1 and alignment < 16384:
                raise RuntimeError("Native ELF segments must support 16KiB pages")
        for name in [
            "APP_LICENSE.txt",
            "THIRD_PARTY_NOTICES.txt",
            "Apache-2.0.txt",
            "LIBCXX-NOTICE.txt",
        ]:
            if not archive.read("assets/" + name).strip():
                raise RuntimeError("Missing bundled license")
        scopes = set(archive.read(prefix + "scope.list").decode().split())
        if scopes != {"com.bushiroad.d4djddedition", "com.sega.pjsekai.re"}:
            raise RuntimeError("Unexpected hook scope")
    run(tools / "zipalign", "-c", "-P", "16", "4", apk)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-ref", default="HEAD")
    parser.add_argument("--check-release-apk", type=Path)
    args = parser.parse_args()
    tools = (
        Path(os.environ.get("ANDROID_HOME", Path.home() / "Library/Android/sdk"))
        / "build-tools/37.0.0"
    )
    if args.check_release_apk:
        validate(args.check_release_apk.resolve(), tools)
        print(
            "Release APK contract verified. Distribution additionally requires the release certificate."
        )
        return
    if run("git", "status", "--porcelain", capture=True).stdout.strip():
        raise RuntimeError("Commit all release sources first")
    revision = run("git", "rev-parse", args.source_ref, capture=True).stdout.strip()
    if revision != run("git", "rev-parse", "HEAD", capture=True).stdout.strip():
        raise RuntimeError("Build checkout must match the source revision")
    if not os.environ.get("ONEUI_SIGNING_PROPERTIES"):
        raise RuntimeError(
            "Set ONEUI_SIGNING_PROPERTIES to the private signing configuration"
        )
    run("python3", "tools/check_native.py")
    run(ROOT / "gradlew", ":app:assembleRelease", ":app:lintRelease")
    output = ROOT / "app/build/outputs/apk/release"
    metadata = json.loads((output / "output-metadata.json").read_text())
    if metadata["variantName"] != "release" or len(metadata["elements"]) != 1:
        raise RuntimeError("Expected a single release variant")
    item = metadata["elements"][0]
    apk = output / item["outputFile"]
    validate(apk, tools)
    signature = run(
        tools / "apksigner", "verify", "--verbose", "--print-certs", apk, capture=True
    ).stdout
    if CERTIFICATE not in signature:
        raise RuntimeError("Release certificate does not match existing installations")
    destination = ROOT / "artifacts/release" / item["versionName"]
    destination.mkdir(parents=True, exist_ok=True)
    name = "LegacyGameTools-" + item["versionName"]
    target = destination / (name + "-release.apk")
    shutil.copy2(apk, target)
    source = destination / (name + "-source.zip")
    run(
        "git",
        "archive",
        "--format=zip",
        "--prefix=" + name + "/",
        "--output=" + str(source),
        revision,
    )
    sums = "".join(
        hashlib.sha256(p.read_bytes()).hexdigest() + "  " + p.name + "\n"
        for p in [target, source]
    )
    (destination / "SHA256SUMS.txt").write_text(sums)
    (destination / "verification.txt").write_text(
        "Source revision: " + revision + "\nVariant: release\n" + signature
    )
    print("Verified release:", target)


if __name__ == "__main__":
    main()
