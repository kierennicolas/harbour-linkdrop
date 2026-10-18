#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container. Builds a tiny harness for
# the target, then loads the app's QML with the real Sailfish Silica and
# Sailfish Pickers modules (under qemu for ARM targets) and fails on any QML
# error or warning that points at the app's own files.
set -uo pipefail
: "${SFOS_RELEASE:?}" "${SFOS_ARCH:?}"
TARGET="SailfishOS-${SFOS_RELEASE}-${SFOS_ARCH}"
SB2="sb2 -t $TARGET"

echo "::group::Target facts"
for d in /usr/lib/qt5/plugins/platforms /usr/lib64/qt5/plugins/platforms /usr/lib/qt5/qml/Sailfish /usr/lib64/qt5/qml/Sailfish; do
  $SB2 test -d "$d" && { echo "$d:"; $SB2 ls "$d"; }
done
echo "Silica:  $($SB2 rpm -q sailfishsilica-qt5 2>&1)"
echo "Pickers: $($SB2 rpm -qf /usr/lib/qt5/qml/Sailfish/Pickers/qmldir /usr/lib64/qt5/qml/Sailfish/Pickers/qmldir 2>&1 | grep -v 'No such' )"
echo "::endgroup::"

has_minimal() {
  $SB2 test -e /usr/lib/qt5/plugins/platforms/libqminimal.so || $SB2 test -e /usr/lib64/qt5/plugins/platforms/libqminimal.so
}
has_pickers() {
  $SB2 test -e /usr/lib/qt5/qml/Sailfish/Pickers/qmldir || $SB2 test -e /usr/lib64/qt5/qml/Sailfish/Pickers/qmldir
}

if ! has_minimal || ! has_pickers; then
  echo "::group::Installing missing runtime pieces into the target"
  $SB2 -m sdk-install -R zypper --non-interactive refresh || true
  $SB2 -m sdk-install -R zypper --non-interactive search -s 'qt5-plugin-platform*' 'sailfish-components-pickers*' || true
  has_minimal || $SB2 -m sdk-install -R zypper --non-interactive install qt5-plugin-platform-minimal || true
  has_pickers || $SB2 -m sdk-install -R zypper --non-interactive install sailfish-components-pickers-qt5 || true
  echo "::endgroup::"
fi
has_minimal || { echo "::error::No 'minimal' Qt platform plugin available in $TARGET"; exit 2; }

# Install the freshly built RPM into the target so the check runs against
# the packaged QML and icons, exactly as they land on the phone.
shopt -s nullglob
rpms=(/workspace/RPMS/harbour-linkdrop-[0-9]*."${SFOS_ARCH}".rpm)
[ ${#rpms[@]} -eq 1 ] || { echo "::error::expected one app RPM, found: ${rpms[*]:-none}"; exit 2; }
echo "::group::Install ${rpms[0]##*/} into $TARGET"
$SB2 -m sdk-install -R rpm -Uvh --force "${rpms[0]}" || { echo "::error::RPM install into target failed"; exit 1; }
$SB2 rpm -q harbour-linkdrop
echo "::endgroup::"

SRC="$HOME/smoke-src"
rm -rf "$SRC" && mkdir -p "$SRC"
tar -C /workspace --exclude=./.git --exclude=./RPMS -cf - . | tar -C "$SRC" -xf -
mkdir -p "$SRC/tests/qmlcheck/build" && cd "$SRC/tests/qmlcheck/build"

echo "::group::Build qmlcheck for $TARGET"
$SB2 qmake ../qmlcheck.pro && $SB2 make -j"$(nproc)" || { echo "::error::qmlcheck build failed"; exit 2; }
echo "::endgroup::"

mkdir -p /tmp/xdg && chmod 700 /tmp/xdg
$SB2 /bin/sh -c 'XDG_RUNTIME_DIR=/tmp/xdg QT_QPA_PLATFORM=minimal QV4_FORCE_INTERPRETER=1 ./qmlcheck "$1"' _ /usr/share/harbour-linkdrop/qml
