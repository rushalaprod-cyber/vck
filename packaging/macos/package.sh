#!/bin/bash
# Construit l'installeur macOS de vck (VST3 + AU + application).
# Usage : packaging/macos/package.sh <dossier artefacts Release> <dossier de sortie>
set -euo pipefail

ART="$1"
OUT="$2"
VER="1.2.0"
STAGE="$(mktemp -d)"
mkdir -p "$OUT" "$STAGE/vst3" "$STAGE/au" "$STAGE/app"
OUT="$(cd "$OUT" && pwd)"   # chemin absolu : le zip est cree apres un cd

cp -R "$ART/VST3/vck.vst3"        "$STAGE/vst3/"
cp -R "$ART/AU/vck.component"     "$STAGE/au/"
cp -R "$ART/Standalone/vck.app"   "$STAGE/app/"

# Signature ad-hoc (obligatoire sur Apple Silicon)
for b in "$STAGE/vst3/vck.vst3" "$STAGE/au/vck.component" "$STAGE/app/vck.app"; do
    codesign --force --deep --sign - "$b"
done

make_pkg () {
    local name="$1" location="$2" id="$3"
    pkgbuild --analyze --root "$STAGE/$name" "$STAGE/$name.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$STAGE/$name.plist"
    pkgbuild --root "$STAGE/$name" --component-plist "$STAGE/$name.plist" \
             --install-location "$location" --identifier "$id" --version "$VER" "$STAGE/$name.pkg"
}

make_pkg vst3 "/Library/Audio/Plug-Ins/VST3"       com.vck.vst3
make_pkg au   "/Library/Audio/Plug-Ins/Components" com.vck.au
make_pkg app  "/Applications"                      com.vck.app

productbuild --package "$STAGE/vst3.pkg" --package "$STAGE/au.pkg" --package "$STAGE/app.pkg" \
             "$OUT/vck-$VER-macos.pkg"

# Archive zip en secours (installation manuelle)
( cd "$STAGE" && mkdir -p zip && cp -R vst3/vck.vst3 au/vck.component app/vck.app zip/ \
  && cd zip && ditto -c -k --sequesterRsrc --keepParent . "$OUT/vck-$VER-macos.zip" )

echo "OK : $OUT/vck-$VER-macos.pkg"
