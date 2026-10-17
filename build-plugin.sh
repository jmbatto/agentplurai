#!/usr/bin/env bash
# Compile uniquement le plugin (cible Windows / Qt6 / MXE) et le pose dans /dist.
set -euo pipefail

: "${MXE_TARGET:=x86_64-w64-mingw32.shared}"
: "${QMAKE:=/opt/mxe/usr/${MXE_TARGET}/qt6/bin/qmake}"
HOST_QT_LIB="/opt/mxe/usr/x86_64-pc-linux-gnu/lib"
# qmake MinGW est un wrapper vers qmake6 hôte ; ses .so MXE ne sont pas dans ldconfig.
export LD_LIBRARY_PATH="${HOST_QT_LIB}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

if [[ ! -x "${QMAKE}" ]]; then
  echo "ERROR: qmake MinGW introuvable (${QMAKE}). Pas de repli Linux : il produirait un .so, pas une DLL." >&2
  echo "Pour Linux natif, utilise build-plugin-linux.sh." >&2
  exit 1
fi
PLUGIN_DIR="${PLUGIN_DIR:-/work/plugin-src/liteideaf}"
LITEIDEX="${LITEIDEX:-/src/liteide/liteidex}"
DIST="${DIST:-/dist}"
OUT="${PLUGIN_DIR}/build-win"

if [[ ! -f "${PLUGIN_DIR}/liteideaf.pro" ]]; then
  echo "ERROR: pas de ${PLUGIN_DIR}/liteideaf.pro" >&2
  exit 1
fi
if [[ ! -d "${LITEIDEX}/src/api" ]]; then
  echo "ERROR: API LiteIDE introuvable (${LITEIDEX}/src/api)" >&2
  exit 1
fi

mkdir -p "${OUT}" "${DIST}/lib/liteide/plugins" "${DIST}/bin"
cd "${OUT}"
"${QMAKE}" "${PLUGIN_DIR}/liteideaf.pro" \
    "CONFIG+=release" "CONFIG-=precompile_header" \
    "IDE_SOURCE_TREE=${LITEIDEX}" \
    "IDE_PLUGIN_PATH=${DIST}/lib/liteide/plugins"
make -j"${JOBS:-$(nproc)}"

DLL="$(find "${DIST}" "${OUT}" "${PLUGIN_DIR}" -name 'liteideaf.dll' 2>/dev/null | head -n 1 || true)"
if [[ -z "${DLL}" ]]; then
  echo "ERROR: liteideaf.dll introuvable après le make." >&2
  echo "-- contenu ${OUT} --" >&2
  find "${OUT}" -maxdepth 3 2>/dev/null | head -n 40 >&2 || true
  echo "-- contenu ${DIST} --" >&2
  find "${DIST}" -name '*.dll' 2>/dev/null | head -n 40 >&2 || true
  exit 1
fi
mkdir -p /opt/plugins-win
cp -a "${DLL}" "${OUT}/liteideaf.dll"
cp -a "${DLL}" /opt/plugins-win/liteideaf.dll

echo "== plugin dll =="
ls -la "${OUT}/liteideaf.dll"
echo "Puis : docker compose -f docker-compose.windows.yml run --rm export"
