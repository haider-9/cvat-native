#!/usr/bin/env bash
# Detached update helper for Linux. The Qt app starts this process, then exits;
# only after that can the running executable be replaced on disk.
set -u

install_dir="${1:-}"
zip_file="${2:-}"
old_pid="${3:-}"
exe_name="${4:-cvat-native}"

log() { printf '[cvat-native updater] %s\n' "$1" >&2; }

case "$old_pid" in
    ''|*[!0-9]*) log "invalid process id: $old_pid"; exit 1 ;;
esac
if [ -z "$install_dir" ] || [ -z "$zip_file" ] || [ ! -f "$zip_file" ]; then
    log "usage: updater.sh <install-dir> <update.zip> <old-pid> [exe-name]"
    exit 1
fi

# Give the main process time to close its database and release its files.
attempt=0
while [ "$attempt" -lt 60 ] && [ -d "/proc/$old_pid" ]; do
    sleep 0.5
    attempt=$((attempt + 1))
done
if [ -d "/proc/$old_pid" ]; then
    log "CVAT Native is still running; update cancelled"
    exit 1
fi
sleep 1

if ! command -v unzip >/dev/null 2>&1; then
    log "unzip is required to apply updates"
    exit 1
fi
if ! unzip -o -q "$zip_file" -d "$install_dir"; then
    log "could not extract $zip_file into $install_dir"
    exit 1
fi

app="$install_dir/bin/$exe_name"
[ -x "$app" ] || app="$install_dir/$exe_name"
chmod +x "$app" 2>/dev/null || true
rm -f "$zip_file"

if [ ! -x "$app" ]; then
    log "updated executable not found at $app"
    exit 1
fi
nohup "$app" >/dev/null 2>&1 &
log "update applied; restarted $app"
