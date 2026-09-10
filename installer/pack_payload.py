#!/usr/bin/env python3
"""Builds installer/native/payload.zip from a finished Release build.

AETHER ships as a plugin only, so the zip holds:
    VST3/AETHER.vst3/...   -> the user's per-user VST3 folder
    README.md              -> the support folder next to the uninstaller

Usage: python3 pack_payload.py <artefacts-dir> [out-zip]
  e.g. python3 pack_payload.py build/windows-vs2026/AETHER_artefacts/Release
"""
import os, sys, zipfile

def add_tree(z, root, arc_prefix):
    for dirpath, _, filenames in os.walk(root):
        for name in sorted(filenames):
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            z.write(full, f"{arc_prefix}/{rel}")

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    artefacts = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "native", "payload.zip")
    here = os.path.dirname(os.path.abspath(__file__))
    readme = os.path.join(here, "..", "README.md")

    vst3 = os.path.join(artefacts, "VST3", "AETHER.vst3")
    if not os.path.exists(vst3):
        print(f"error: missing {vst3} - build Release first", file=sys.stderr)
        return 1

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        add_tree(z, vst3, "VST3/AETHER.vst3")
        if os.path.exists(readme):
            z.write(readme, "README.md")

    print(f"payload: {out}  ({os.path.getsize(out) / 1048576:.1f} MB)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
