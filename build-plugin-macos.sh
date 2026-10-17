#!/usr/bin/env bash
# Plugin AgentPlurAI pour LiteIDE macOS. À lancer sur un Mac, après build-macos.sh.
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: ce script doit tourner sur macOS." >&2
  exit 1
fi

: "${QMAKE:=qmake6}"
PLUGIN_DIR="${PLUGIN_DIR:-${PWD}/plugin-src/liteideaf}"
LITEIDEX="${LITEIDEX:-${PWD}/liteide/liteidex}"
DIST="${DIST:-${PWD}/dist-mac}"
OUT="${PLUGIN_DIR}/build-mac"

if ! command -v "${QMAKE}" >/dev/null 2>&1; then
  echo "ERROR: ${QMAKE} introuvable." >&2
  exit 1
fi
if [[ ! -f "${PLUGIN_DIR}/liteideaf.pro" ]]; then
  echo "ERROR: pas de ${PLUGIN_DIR}/liteideaf.pro" >&2
  exit 1
fi
if [[ ! -d "${LITEIDEX}/src/api" ]]; then
  echo "ERROR: API LiteIDE introuvable (${LITEIDEX}/src/api). Lance build-macos.sh d'abord." >&2
  exit 1
fi

mkdir -p "${OUT}"
cd "${OUT}"
"${QMAKE}" "${PLUGIN_DIR}/liteideaf.pro" \
    "CONFIG+=release" "CONFIG-=precompile_header" \
    "IDE_SOURCE_TREE=${LITEIDEX}" \
    "IDE_PLUGIN_PATH=${OUT}"
make -j"${JOBS:-$(sysctl -n hw.ncpu)}"

SO="$(find "${OUT}" -name 'libliteideaf.dylib' -o -name 'libliteideaf.so' | head -n 1 || true)"
if [[ -z "${SO}" ]]; then
  echo "ERROR: plugin introuvable après le make." >&2
  exit 1
fi

PLUGINS="${DIST}/LiteIDE.app/Contents/PlugIns"
mkdir -p "${PLUGINS}"
cp -a "${SO}" "${PLUGINS}/$(basename "${SO}")"

echo "== plugin macOS =="
ls -la "${PLUGINS}/$(basename "${SO}")"
echo "Livré dans ${DIST}/LiteIDE.app"
