#!/usr/bin/env bash
# Compile le plugin pour Linux natif (Qt 6 Debian) et le pose dans /dist.
# Ne pas utiliser build-plugin.sh : celui-ci exige le qmake MXE et produit une DLL.
set -euo pipefail

: "${QMAKE:=qmake6}"
PLUGIN_DIR="${PLUGIN_DIR:-/work/plugin-src/liteideaf}"
LITEIDEX="${LITEIDEX:-/src/liteide/liteidex}"
DIST="${DIST:-/dist}"
OUT="${PLUGIN_DIR}/build-linux"

if ! command -v "${QMAKE}" >/dev/null 2>&1; then
  echo "ERROR: ${QMAKE} introuvable. Image linux-dev attendue (qt6-base-dev)." >&2
  exit 1
fi
if [[ ! -f "${PLUGIN_DIR}/liteideaf.pro" ]]; then
  echo "ERROR: pas de ${PLUGIN_DIR}/liteideaf.pro" >&2
  exit 1
fi
if [[ ! -d "${LITEIDEX}/src/api" ]]; then
  echo "ERROR: API LiteIDE introuvable (${LITEIDEX}/src/api)" >&2
  exit 1
fi

mkdir -p "${OUT}" "${DIST}/lib/liteide/plugins"
cd "${OUT}"
"${QMAKE}" "${PLUGIN_DIR}/liteideaf.pro" \
    "CONFIG+=release" "CONFIG-=precompile_header" \
    "IDE_SOURCE_TREE=${LITEIDEX}" \
    "IDE_PLUGIN_PATH=${DIST}/lib/liteide/plugins"
make -j"${JOBS:-$(nproc)}"

echo "== plugin so =="
find "${DIST}/lib/liteide/plugins" "${OUT}" -name '*liteideaf*' -o -name '*.so' | head
echo "Copier le .so dans le lib/liteide/plugins d'un LiteIDE Linux, puis relancer l'IDE."
