#!/usr/bin/env bash
# Populate bindings/rust/vendor/ with the C++ engine sources so the crate can
# build the engine from source with no prebuilt library and no network access.
#
# crates.io only packages files that live inside the crate directory, so the
# engine sources (which normally live at the repo root) must be copied in here
# before `cargo publish`/`cargo package`. vendor/ is git-ignored (see
# bindings/rust/.gitignore) to avoid bloating the main repo; the publish
# workflow (.github/workflows/publish-rust.yml) runs this script before
# publishing so the vendored sources are packaged at publish time.
#
# Usage (from anywhere):
#   bindings/rust/vendor.sh
#
# PowerShell equivalent (run from the repo root):
#   $r = "bindings/rust/vendor"
#   Remove-Item -Recurse -Force $r -ErrorAction SilentlyContinue
#   New-Item -ItemType Directory -Force $r | Out-Null
#   foreach ($d in "framework","src","include","third_party") {
#       Copy-Item -Recurse -Force $d (Join-Path $r $d)
#   }

set -euo pipefail

# Repo root = two levels up from this script (bindings/rust -> repo root).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENDOR="$SCRIPT_DIR/vendor"

echo "Vendoring C++ engine sources from $ROOT into $VENDOR"

rm -rf "$VENDOR"
mkdir -p "$VENDOR"

# Copy the directories the build script needs as include roots. framework/ and
# src/ carry every .cpp and .h; include/ has the public C ABI header;
# third_party/ has the vendored glm and stb (note: src/capi.cpp and
# framework/Canvas.cpp include "../third_party/stb/stb_image_write.h", so
# third_party must sit as a sibling of framework/ and src/ inside vendor/).
for d in framework src include third_party; do
    if [ ! -d "$ROOT/$d" ]; then
        echo "error: expected directory $ROOT/$d not found" >&2
        exit 1
    fi
    cp -r "$ROOT/$d" "$VENDOR/$d"
done

echo "Done. vendor/ now contains: $(ls "$VENDOR" | tr '\n' ' ')"
