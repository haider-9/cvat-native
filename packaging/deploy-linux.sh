#!/usr/bin/env bash
# Bundle the Qt runtime needed by a release ZIP. The CMake install step already
# placed bin/, share/ and the updater helper in $1; this copies Qt libraries and
# plugins next to them and writes a qt.conf so a clean Linux machine can run it.
set -euo pipefail

stage="$1"
qt_root="$2"

libdir="$stage/lib"
plugdir="$libdir/plugins"
mkdir -p "$libdir" "$plugdir"

copy_qt_deps() {
    local file="$1" resolved
    while IFS= read -r resolved; do
        [ -n "$resolved" ] || continue
        case "$resolved" in
            "$qt_root"/*)
                cp -L "$resolved" "$libdir/$(basename "$resolved")"
                ;;
        esac
    done < <(ldd "$file" 2>/dev/null | awk '$2 == "=>" { print $3 }')
}

copy_qt_deps "$stage/bin/cvat-native"
cp -a "$qt_root/plugins/." "$plugdir/"
find "$plugdir" -name '*.debug' -type f -delete

while IFS= read -r -d '' plugin; do
    copy_qt_deps "$plugin"
done < <(find "$plugdir" -name '*.so' -type f -print0)

# Qt libraries and plugins ship with RUNPATHs pointing into the build runner.
# Rewrite them so the bundled copies are found after extraction elsewhere.
while IFS= read -r -d '' lib; do
    patchelf --set-rpath '$ORIGIN' "$lib"
done < <(find "$libdir" -maxdepth 1 -name 'lib*.so*' -type f -print0)

while IFS= read -r -d '' plugin; do
    patchelf --set-rpath '$ORIGIN/../..:$ORIGIN/../../..' "$plugin"
done < <(find "$plugdir" -name '*.so' -type f -print0)

printf '[Paths]\nPlugins=../lib/plugins\n' > "$stage/bin/qt.conf"
printf 'Bundled Qt runtime from %s into %s\n' "$qt_root" "$stage"
