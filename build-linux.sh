#!/usr/bin/env bash
# Build LiteIDE Qt 6 natif Linux. Aligné sur build/build_linux_qt6.sh.
# Pas de correctifs MinGW (vterm, terminal) : l'arbre Linux est séparé.
set -euo pipefail

: "${QMAKE:=qmake6}"
: "${LITEIDE_SRC:=/src/liteide-linux}"
: "${DIST_ROOT:=/opt/liteide-dist-linux}"

LITEIDE_ROOT="${LITEIDE_SRC}/liteidex"
BUILD_ROOT="${LITEIDE_SRC}/build"

if ! command -v "${QMAKE}" >/dev/null 2>&1; then
  echo "ERROR: ${QMAKE} introuvable." >&2
  exit 1
fi
if [[ ! -f "${LITEIDE_ROOT}/liteidex.pro" ]]; then
  echo "ERROR: sources Linux absentes (${LITEIDE_ROOT}/liteidex.pro)." >&2
  exit 1
fi

echo "== LiteIDE Linux Qt6 native build =="
echo "LITEIDE_ROOT=${LITEIDE_ROOT}"
echo "QMAKE=$(command -v "${QMAKE}")"
"${QMAKE}" -query QT_VERSION | grep -q '^6\.' || {
  echo "ERROR: ${QMAKE} n'est pas Qt 6." >&2
  exit 1
}

# liteidex.pri ajoute core5compat dès Qt 6. Avertir, ne pas bloquer :
# le chemin des en-têtes Debian varie (multiarch).
HEADERS="$("${QMAKE}" -query QT_INSTALL_HEADERS)"
if [[ ! -d "${HEADERS}/QtCore5Compat" && ! -d /usr/include/x86_64-linux-gnu/qt6/QtCore5Compat ]]; then
  echo "WARN: QtCore5Compat non trouvé sous ${HEADERS}. Le make échouera si le module manque." >&2
fi

mkdir -p "${BUILD_ROOT}"
cd "${BUILD_ROOT}"
"${QMAKE}" "${LITEIDE_ROOT}" -spec linux-g++ "CONFIG+=release" "CONFIG-=precompile_header"
make -j"${JOBS:-$(nproc)}"

# Les .pro posent le binaire dans liteidex/liteide, parfois sous build/.
BIN=""
for candidate in \
    "${LITEIDE_ROOT}/liteide/bin/liteide" \
    "${BUILD_ROOT}/liteide/bin/liteide" \
    "${LITEIDE_SRC}/liteide/bin/liteide"
do
  if [[ -x "${candidate}" ]]; then
    BIN="${candidate}"
    break
  fi
done
if [[ -z "${BIN}" ]]; then
  echo "ERROR: binaire liteide introuvable. Cherché :" >&2
  printf '  %s\n' \
    "${LITEIDE_ROOT}/liteide/bin/liteide" \
    "${BUILD_ROOT}/liteide/bin/liteide" \
    "${LITEIDE_SRC}/liteide/bin/liteide" >&2
  exit 1
fi
echo "BIN=${BIN}"

if [[ -x "${LITEIDE_ROOT}/make_tools.sh" ]]; then
  ( cd "${LITEIDE_ROOT}" && ./make_tools.sh ) || echo "WARN: make_tools.sh ignoré"
fi
if [[ -f "${LITEIDE_ROOT}/src/tools/exportqrc/main.go" ]]; then
  ( cd "${LITEIDE_ROOT}" && go run src/tools/exportqrc/main.go -root . ) || echo "WARN: exportqrc ignoré"
fi

rm -rf "${DIST_ROOT}"
mkdir -p \
  "${DIST_ROOT}/bin" \
  "${DIST_ROOT}/lib/liteide/plugins" \
  "${DIST_ROOT}/share/liteide"

cp -a "${BIN}" "${DIST_ROOT}/bin/liteide"
if [[ -f "${LITEIDE_ROOT}/liteide/lib/liteide/libliteapp.so" ]]; then
  cp -a "${LITEIDE_ROOT}/liteide/lib/liteide/libliteapp.so" "${DIST_ROOT}/lib/liteide/"
fi
if [[ -d "${LITEIDE_ROOT}/liteide/lib/liteide/plugins" ]]; then
  cp -a "${LITEIDE_ROOT}/liteide/lib/liteide/plugins/." "${DIST_ROOT}/lib/liteide/plugins/"
fi
if [[ -d "${LITEIDE_ROOT}/bin" ]]; then
  cp -a "${LITEIDE_ROOT}/bin/." "${DIST_ROOT}/bin/" 2>/dev/null || true
fi
if [[ -d "${LITEIDE_ROOT}/deploy" ]]; then
  cp -a "${LITEIDE_ROOT}/deploy/." "${DIST_ROOT}/share/liteide/"
fi
if [[ -d "${LITEIDE_ROOT}/os_deploy/linux" ]]; then
  cp -a "${LITEIDE_ROOT}/os_deploy/linux/." "${DIST_ROOT}/share/liteide/"
fi

test -x "${DIST_ROOT}/bin/liteide"
test -d "${DIST_ROOT}/share/liteide"
echo "== LiteIDE Linux staged: ${DIST_ROOT}/bin/liteide =="
