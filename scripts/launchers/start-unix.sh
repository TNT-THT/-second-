#!/bin/sh
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
[ -f "$ROOT/app/dist/index.html" ] || { echo 'Extract the entire PetDesk ZIP first.' >&2; exit 1; }
version='@VERSION@'; os=$(uname -s); arch=$(uname -m)
case "$os:$arch" in
  Darwin:arm64) platform=darwin-arm64; sha='@MAC_ARM64@' ;;
  Darwin:x86_64) platform=darwin-x64; sha='@MAC_X64@' ;;
  Linux:x86_64) platform=linux-x64; sha='@LINUX_X64@' ;;
  Linux:aarch64|Linux:arm64) platform=linux-arm64; sha='@LINUX_ARM64@' ;;
  *) echo 'Unsupported architecture. See source build instructions.' >&2; exit 1 ;;
esac
if [ "$os" = Darwin ]; then base="$HOME/Library/Caches/PetDesk"; else base="${XDG_CACHE_HOME:-$HOME/.cache}/PetDesk"; fi
dest="$base/runtime-$version-$platform"
if [ ! -f "$dest/.verified" ]; then
  umask 077; mkdir -p "$base"; tmp=$(mktemp -d "$base/setup.XXXXXXXX")
  trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
  echo "First launch: downloading official Electron $version from GitHub; checking SHA-256."
  curl --fail --location --show-error --retry 2 -o "$tmp/runtime.zip" "https://github.com/electron/electron/releases/download/v$version/electron-v$version-$platform.zip"
  if [ "$os" = Darwin ]; then actual=$(shasum -a 256 "$tmp/runtime.zip"); else actual=$(sha256sum "$tmp/runtime.zip"); fi
  [ "${actual%% *}" = "$sha" ] || { echo 'Runtime checksum mismatch. Not launching.' >&2; exit 1; }
  [ ! -e "$dest" ] || { echo "Incomplete cached runtime: $dest. Inspect it, then retry." >&2; exit 1; }
  mkdir "$tmp/files"; if [ "$os" = Darwin ]; then ditto -x -k "$tmp/runtime.zip" "$tmp/files"; else unzip -q "$tmp/runtime.zip" -d "$tmp/files"; fi
  printf '%s\n' "$sha" > "$tmp/files/.verified"; mv "$tmp/files" "$dest"; rm -rf -- "$tmp"; trap - EXIT HUP INT TERM
fi
if [ "$os" = Darwin ]; then exec /usr/bin/open -n "$dest/Electron.app" --args "$ROOT/app"; else exec "$dest/electron" "$ROOT/app"; fi
