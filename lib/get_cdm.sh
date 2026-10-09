#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

case "$(uname -m)" in
	x86_64|amd64)
		CDM_ARCH="x86_64"
		FILE_ARCH_PATTERN='x86-64|x86_64'
		;;
	aarch64|arm64|armv7*|armv6*)
		CDM_ARCH="arm"
		FILE_ARCH_PATTERN='aarch64|arm64|ARM'
		;;
	*)
		echo "Unsupported host architecture: $(uname -m)" >&2
		exit 1
		;;
esac

# Search browser-managed CDM locations only; do not fetch an unverified archive.
SEARCH_ROOTS=(
	"${HOME}/.mozilla/firefox"
	"${HOME}/.config/chromium"
	"${HOME}/.config/google-chrome"
	"${HOME}/.config/google-chrome-beta"
	"${HOME}/.config/chromium-beta"
	"${HOME}/snap/firefox/common/.mozilla/firefox"
	"${HOME}/snap/chromium/common/chromium"
	"${HOME}/.var/app/org.mozilla.firefox"
	"${HOME}/.var/app/com.google.Chrome"
	"${HOME}/.var/app/org.chromium.Chromium"
	/opt/google/chrome
	/opt/google/chrome-beta
	/usr/lib/firefox
	/usr/lib/chromium
	/usr/lib/chromium-browser
	/usr/lib/google-chrome
	/usr/lib64/chromium
)

mapfile -d '' -t CANDIDATES < <(
	find "${SEARCH_ROOTS[@]}" -type f -name libwidevinecdm.so -print0 2>/dev/null \
		| sort -z -V
)

if ((${#CANDIDATES[@]} == 0)); then
	cat >&2 <<'EOF'
Could not find libwidevinecdm.so in the usual Firefox/Chrome/Chromium locations.
Open one of those browsers, enable/install Widevine DRM, and run this script again.
EOF
	exit 1
fi

# Versioned browser directories sort naturally, so select the newest candidate.
SOURCE="${CANDIDATES[${#CANDIDATES[@]} - 1]}"
if command -v file >/dev/null 2>&1; then
	SOURCE_DESCRIPTION="$(file -b -- "${SOURCE}")"
	if [[ ! "${SOURCE_DESCRIPTION}" =~ ${FILE_ARCH_PATTERN} ]]; then
		echo "Found a Widevine CDM for the wrong architecture: ${SOURCE_DESCRIPTION}" >&2
		echo "Source: ${SOURCE}" >&2
		exit 1
	fi
fi

DESTINATION="${REPO_ROOT}/lib/WidevineCdm/${CDM_ARCH}/libwidevinecdm.so"
install -D -m 0644 -- "${SOURCE}" "${DESTINATION}"

echo "Found Widevine CDM: ${SOURCE}"
echo "Installed project copy: ${DESTINATION}"
if ((${#CANDIDATES[@]} > 1)); then
	echo "Other browser copies found: $((${#CANDIDATES[@]} - 1))"
fi
