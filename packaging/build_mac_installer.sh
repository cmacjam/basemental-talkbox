#!/usr/bin/env bash
# Builds one macOS installer (.pkg) containing AU, VST3 and the Standalone app.
# Usage: packaging/build_mac_installer.sh <artefacts-dir> <version> <out-dir>
set -euo pipefail
ART="$1"; VERSION="$2"; OUT="$3"
NAME="Raj T Vocal"
ID="com.basemental.rajtvocal"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

stage () { # $1 = source bundle, $2 = payload subdir
  mkdir -p "$WORK/$2"
  cp -R "$1" "$WORK/$2/"
}

stage "$ART/AU/$NAME.component"     au
stage "$ART/VST3/$NAME.vst3"        vst3
stage "$ART/Standalone/$NAME.app"   app

pkgbuild --root "$WORK/au"   --identifier "$ID.au"   --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" "$WORK/au.pkg"
pkgbuild --root "$WORK/vst3" --identifier "$ID.vst3" --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3"       "$WORK/vst3.pkg"
pkgbuild --root "$WORK/app"  --identifier "$ID.app"  --version "$VERSION" --install-location "/Applications"                       "$WORK/app.pkg"

if [ -d "$ART/AAX/$NAME.aaxplugin" ]; then
  stage "$ART/AAX/$NAME.aaxplugin" aax
  pkgbuild --root "$WORK/aax" --identifier "$ID.aax" --version "$VERSION" --install-location "/Library/Application Support/Avid/Audio/Plug-Ins" "$WORK/aax.pkg"
fi

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$NAME $VERSION</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>
    <line choice="au"/><line choice="vst3"/><line choice="app"/>$( [ -f "$WORK/aax.pkg" ] && echo '<line choice="aax"/>' )
  </choices-outline>
  <choice id="au"   title="Audio Unit (Logic Pro, GarageBand, MainStage)"><pkg-ref id="$ID.au"/></choice>
  <choice id="vst3" title="VST3"><pkg-ref id="$ID.vst3"/></choice>
  <choice id="app"  title="Standalone app"><pkg-ref id="$ID.app"/></choice>
  $( [ -f "$WORK/aax.pkg" ] && echo "<choice id=\"aax\" title=\"AAX (Pro Tools)\"><pkg-ref id=\"$ID.aax\"/></choice><pkg-ref id=\"$ID.aax\">aax.pkg</pkg-ref>" )
  <pkg-ref id="$ID.au">au.pkg</pkg-ref>
  <pkg-ref id="$ID.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="$ID.app">app.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$OUT/Raj-T-Vocal-$VERSION-macOS.pkg"
echo "Built $OUT/Raj-T-Vocal-$VERSION-macOS.pkg"
