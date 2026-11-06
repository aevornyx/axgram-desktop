#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 4 ]]; then
    echo "Usage: $0 <source directory> <AxGram ELF> <linuxdeploy AppImage> <output directory>" >&2
    exit 2
fi

source_dir=$(realpath -e -- "$1")
binary=$(realpath -e -- "$2")
linuxdeploy=$(realpath -e -- "$3")
version=$(awk '$1 == "AppVersionStrSmall" { sub(/\r$/, "", $2); print $2 }' "$source_dir/Telegram/build/version")
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
test -x "$binary"
LC_ALL=C readelf -h "$binary" | grep -E 'Machine:.*X86-64' >/dev/null
mkdir -p -- "$4"
output_dir=$(realpath -e -- "$4")

stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
appdir="$stage/AppDir"
desktop="$appdir/usr/share/applications/com.axgram.desktop.desktop"
icon="$appdir/usr/share/icons/hicolor/256x256/apps/com.axgram.desktop.png"
install -Dm755 "$binary" "$appdir/usr/bin/AxGram"
install -Dm644 "$source_dir/lib/xdg/com.axgram.desktop.desktop" "$desktop"
install -Dm644 "$source_dir/Telegram/Resources/art/icon256.png" "$icon"
install -Dm644 "$source_dir/LICENSE" "$appdir/usr/share/doc/axgram/LICENSE"
install -Dm644 "$source_dir/NOTICE.md" "$appdir/usr/share/doc/axgram/NOTICE.md"
install -Dm644 "$source_dir/Telegram/SourceFiles/ax/libs/sqlite/SQLCIPHER-LICENSE.md" \
    "$appdir/usr/share/doc/axgram/SQLCIPHER-LICENSE.md"

# The AppImage launcher handles activation without a system D-Bus service.
sed -i -e 's/\r$//' -e 's/^Exec=env DESKTOPINTEGRATION=1 AxGram /Exec=AxGram /' \
    -e '/^TryExec=/d' -e 's/^DBusActivatable=true$/DBusActivatable=false/' "$desktop"

output_name="AxGram-$version-x86_64.AppImage"
test ! -e "$output_dir/$output_name"
cd -- "$stage"
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 OUTPUT="$output_name" "$linuxdeploy" \
    --exclude-library '*' --appdir "$appdir" --executable "$appdir/usr/bin/AxGram" \
    --desktop-file "$desktop" --icon-file "$icon" --output appimage
test -s "$output_name"
install -m755 "$output_name" "$output_dir/$output_name"
cd -- "$output_dir"
sha256sum -- "$output_name" > "$output_name.sha256"
