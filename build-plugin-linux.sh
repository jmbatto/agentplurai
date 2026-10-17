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

# DESTDIR pose le .so dans /dist (éphémère). On le copie aussi dans le
# répertoire de build, monté depuis l'hôte, pour que distribute.sh le trouve.
SO=""
for candidate in \
    "${DIST}/lib/liteide/plugins/libliteideaf.so" \
    "${OUT}/libliteideaf.so" \
    "${OUT}/libliteideaf.so.1"
do
  if [[ -e "${candidate}" ]]; then
    SO="${candidate}"
    break
  fi
done
if [[ -z "${SO}" ]]; then
  SO="$(find "${DIST}/lib/liteide/plugins" "${OUT}" -name 'libliteideaf.so*' -type f 2>/dev/null | head -n 1 || true)"
fi
if [[ -z "${SO}" ]]; then
  echo "ERROR: libliteideaf.so introuvable après le make." >&2
  exit 1
fi
mkdir -p /opt/plugins-linux
cp -a "${SO}" "${OUT}/libliteideaf.so"
cp -a "${SO}" /opt/plugins-linux/libliteideaf.so

echo "== plugin so =="
ls -la "${OUT}/libliteideaf.so" /opt/plugins-linux/libliteideaf.so
echo "Puis : docker compose --profile export run --rm export-linux"
