#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container.
# Builds the RPM for $SFOS_ARCH against SailfishOS-$SFOS_RELEASE and copies
# it to /workspace/RPMS.
set -euo pipefail
: "${SFOS_RELEASE:?}" "${SFOS_ARCH:?}"
TARGET="SailfishOS-${SFOS_RELEASE}-${SFOS_ARCH}"

echo "::group::SDK targets"
sdk-assistant list 2>/dev/null || ls /srv/mer/targets 2>/dev/null || true
echo "::endgroup::"

BUILD="$HOME/build"
rm -rf "$BUILD" && mkdir -p "$BUILD"
# Copy the sources without git metadata or earlier outputs.
tar -C /workspace --exclude=./.git --exclude=./RPMS -cf - . | tar -C "$BUILD" -xf -
cd "$BUILD"

mb2 -t "$TARGET" build

mkdir -p /workspace/RPMS
cp -v RPMS/*.rpm /workspace/RPMS/
