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

# Use the current Harbour rules from Jolla's validator repository. The SDK's
# own copy (wrapped by /usr/bin/rpmvalidation, which needs a target) is the
# fallback when GitHub is unreachable.
if git clone -q --depth 1 https://github.com/sailfishos/sdk-harbour-rpmvalidator.git /tmp/rpmvalidator; then
  echo "Harbour validator: upstream $(git -C /tmp/rpmvalidator log -1 --format='%h %cd' --date=short)"
  /tmp/rpmvalidator/rpmvalidation.sh --no-color "$RPM"
else
  echo "Harbour validator: SDK copy"
  /usr/libexec/sdk-harbour-rpmvalidator/rpmvalidation.sh --no-color -g /usr/share/sdk-harbour-rpmvalidator "$RPM"
fi
