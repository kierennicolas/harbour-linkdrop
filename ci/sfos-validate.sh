#!/bin/bash
# Runs inside the Sailfish OS Platform SDK container: Jolla's Harbour RPM
# validator, plus a summary of what the package contains.
set -euo pipefail
: "${SFOS_ARCH:?}"
shopt -s nullglob
rpms=(/workspace/RPMS/harbour-linkdrop-[0-9]*."${SFOS_ARCH}".rpm)
[ ${#rpms[@]} -eq 1 ] || { echo "expected exactly one app RPM, found: ${rpms[*]:-none}"; exit 1; }
RPM="${rpms[0]}"

echo "::group::Package info"
rpm -qpi "$RPM"
echo "--- files"
rpm -qplv "$RPM"
echo "--- requires"
rpm -qpR "$RPM"
echo "::endgroup::"

VALIDATOR=$(command -v rpmvalidation.sh || true)
if [ -z "$VALIDATOR" ] && [ -x /usr/share/sdk-harbour-rpmvalidator/rpmvalidation.sh ]; then
  VALIDATOR=/usr/share/sdk-harbour-rpmvalidator/rpmvalidation.sh
fi
if [ -z "$VALIDATOR" ]; then
  git clone -q --depth 1 https://github.com/sailfishos/sdk-harbour-rpmvalidator.git /tmp/rpmvalidator
  VALIDATOR=/tmp/rpmvalidator/rpmvalidation.sh
fi
echo "Using validator: $VALIDATOR"
cd "$(dirname "$VALIDATOR")"
"$VALIDATOR" "$RPM"
