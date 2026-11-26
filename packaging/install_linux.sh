#!/usr/bin/env bash
# Installs Raj T Vocal for the current user (no sudo needed).
#   VST3 -> ~/.vst3        LV2 -> ~/.lv2        app -> ~/.local/bin + menu entry
# Run with --system to install for all users (/usr/local/lib/...; needs sudo).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NAME="Raj T Vocal"

if [ "${1:-}" = "--system" ]; then
  VST3="/usr/local/lib/vst3"; LV2="/usr/local/lib/lv2"; BIN="/usr/local/bin"; APPS="/usr/local/share/applications"
else
  VST3="$HOME/.vst3"; LV2="$HOME/.lv2"; BIN="$HOME/.local/bin"; APPS="$HOME/.local/share/applications"
fi
mkdir -p "$VST3" "$LV2" "$BIN" "$APPS"

rm -rf "$VST3/$NAME.vst3" "$LV2/$NAME.lv2"
cp -R "$HERE/$NAME.vst3" "$VST3/"
cp -R "$HERE/$NAME.lv2"  "$LV2/"
install -m 755 "$HERE/$NAME" "$BIN/raj-t-vocal"

cat > "$APPS/raj-t-vocal.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$NAME
Comment=Talk box synth: play the keys, mouth the words
Exec=$BIN/raj-t-vocal
Categories=AudioVideo;Audio;Music;
Terminal=false
EOF

echo "Installed:"
echo "  VST3 -> $VST3/$NAME.vst3"
echo "  LV2  -> $LV2/$NAME.lv2"
echo "  App  -> $BIN/raj-t-vocal"
echo "Rescan plug-ins in your DAW (Reaper, Bitwig, Ardour, Carla...)."
echo "Runtime needs: libasound2 (ALSA) or JACK, libfreetype6, libx11-6."
