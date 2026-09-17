#!/usr/bin/env bash
# Removes AETHER. Your presets and settings in ~/Documents/Amanorsac Studio/AETHER are
# NOT touched (Installer & Packaging Standard P25) - delete that folder by hand if you
# also want those gone.
set -u

TARGETS=(
    "/Library/Audio/Plug-Ins/VST3/AETHER.vst3"
    "/Library/Audio/Plug-Ins/Components/AETHER.component"
    "/Library/Application Support/Amanorsac Studio/AETHER"
)

echo "AETHER uninstaller"
echo
FOUND=0
for t in "${TARGETS[@]}"; do
    if [ -e "$t" ]; then echo "  will remove: $t"; FOUND=1; fi
done
if [ "$FOUND" = 0 ]; then echo "  nothing installed."; echo; read -r -p "Press return to close. " _; exit 0; fi

echo
echo "Your presets in ~/Documents/Amanorsac Studio/AETHER will be kept."
echo
read -r -p "Remove AETHER? [y/N] " reply
case "$reply" in [yY]*) ;; *) echo "Cancelled."; read -r -p "Press return to close. " _; exit 0 ;; esac

# /Library needs an administrator, which is the one prompt this raises (P22).
echo "Removing - macOS will ask for your password."
for t in "${TARGETS[@]}"; do
    [ -e "$t" ] || continue
    /usr/bin/sudo /bin/rm -rf "$t" && echo "  removed $t"
done
/usr/bin/sudo /usr/sbin/pkgutil --forget studio.amanorsac.aether.vst3    >/dev/null 2>&1
/usr/bin/sudo /usr/sbin/pkgutil --forget studio.amanorsac.aether.au      >/dev/null 2>&1
/usr/bin/sudo /usr/sbin/pkgutil --forget studio.amanorsac.aether.support >/dev/null 2>&1

echo
echo "AETHER was removed. Your presets were kept."
read -r -p "Press return to close. " _
