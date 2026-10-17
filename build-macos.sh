#!/usr/bin/env bash
# LiteIDE Qt 6 natif macOS. À lancer sur un Mac avec Xcode et Qt 6.
# Pas de cross-compilation depuis Linux : le SDK Apple n'est pas redistribuable.
set -euo pipefail

: "${QMAKE:=qmake6}"
: "${LITEIDE_SRC:=${PWD}/liteide}"
: "${DIST_ROOT:=${PWD}/dist-mac}"
: "${LITEIDE_REF:=40ef40324b1997ae3cbb89e4c3a4de42e7e9af1c}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: ce script doit tourner sur macOS (Xcode + Qt 6)." >&2
  echo "Un conteneur Linux ne peut pas produire un binaire Mach-O." >&2
  exit 1
fi
if ! command -v "${QMAKE}" >/dev/null 2>&1; then
  echo "ERROR: ${QMAKE} introuvable. brew install qt, puis export PATH." >&2
  exit 1
fi
"${QMAKE}" -query QT_VERSION | grep -q '^6\.' || {
  echo "ERROR: ${QMAKE} n'est pas Qt 6." >&2
  exit 1
}

if [[ ! -d "${LITEIDE_SRC}/liteidex" ]]; then
  git clone https://github.com/visualfc/liteide.git "${LITEIDE_SRC}"
  git -C "${LITEIDE_SRC}" checkout --detach "${LITEIDE_REF}"
fi

LITEIDE_ROOT="${LITEIDE_SRC}/liteidex"
BUILD_ROOT="${LITEIDE_SRC}/build"
mkdir -p "${BUILD_ROOT}"
cd "${BUILD_ROOT}"

echo "== LiteIDE macOS Qt6 =="
echo "QMAKE=$(command -v "${QMAKE}")"
"${QMAKE}" "${LITEIDE_ROOT}" -spec macx-clang "CONFIG+=release" "CONFIG-=precompile_header"
make -j"${JOBS:-$(sysctl -n hw.ncpu)}"

APP="${LITEIDE_ROOT}/liteide/bin/LiteIDE.app"
if [[ ! -d "${APP}" ]]; then
  echo "ERROR: ${APP} absent après le build." >&2
  exit 1
fi

if [[ -x "${LITEIDE_ROOT}/make_tools.sh" ]]; then
  ( cd "${LITEIDE_ROOT}" && ./make_tools.sh ) || echo "WARN: make_tools.sh ignoré"
fi

rm -rf "${DIST_ROOT}"
mkdir -p "${DIST_ROOT}"
cp -a "${APP}" "${DIST_ROOT}/"
if [[ -d "${LITEIDE_ROOT}/deploy" ]]; then
  mkdir -p "${DIST_ROOT}/LiteIDE.app/Contents/Resources"
  cp -a "${LITEIDE_ROOT}/deploy/." "${DIST_ROOT}/LiteIDE.app/Contents/Resources/"
fi
if [[ -d "${LITEIDE_ROOT}/os_deploy/macosx" ]]; then
  cp -a "${LITEIDE_ROOT}/os_deploy/macosx/." "${DIST_ROOT}/LiteIDE.app/Contents/Resources/"
fi

test -d "${DIST_ROOT}/LiteIDE.app"
echo "== LiteIDE macOS staged: ${DIST_ROOT}/LiteIDE.app =="
