#!/usr/bin/env bash
# Build LiteIDE Qt 6 natif Linux. Pas de correctifs MinGW (vterm, terminal).
set -euo pipefail

: "${QMAKE:=qmake6}"
: "${LITEIDE_SRC:=/src/liteide-linux}"
: "${DIST_ROOT:=/opt/liteide-dist-linux}"

if ! command -v "${QMAKE}" >/dev/null 2>&1; then
  echo "ERROR: ${QMAKE} introuvable." >&2
  exit 1
fi
if [[ ! -d "${LITEIDE_SRC}/liteidex" ]]; then
  echo "ERROR: sources Linux absentes (${LITEIDE_SRC}/liteidex)." >&2
  exit 1
fi

BUILD_ROOT="${LITEIDE_SRC}/build"
mkdir -p "${BUILD_ROOT}"
cd "${BUILD_ROOT}"

echo "== LiteIDE Linux Qt6 native build =="
echo "LITEIDE_SRC=${LITEIDE_SRC}"
echo "QMAKE=$(command -v "${QMAKE}")"
"${QMAKE}" -query QT_VERSION

"${QMAKE}" "${LITEIDE_SRC}/liteidex/liteidex.pro" \
    "CONFIG+=release" "CONFIG-=precompile_header"
make -j"${JOBS:-$(nproc)}"

if [[ ! -x "${BUILD_ROOT}/liteide/bin/liteide" ]]; then
  echo "ERROR: liteide Linux absent après le build." >&2
  exit 1
fi

rm -rf "${DIST_ROOT}"
mkdir -p "${DIST_ROOT}"
cp -a "${BUILD_ROOT}/liteide/." "${DIST_ROOT}/"
test -x "${DIST_ROOT}/bin/liteide"
echo "== LiteIDE Linux staged: ${DIST_ROOT}/bin/liteide =="
