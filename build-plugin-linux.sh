#!/usr/bin/env bash
# Compile le plugin pour Linux natif (Qt 6 Debian) et le pose dans /dist.
# Ne pas utiliser build-plugin.sh : celui-ci exige le qmake MXE et produit une DLL.
set -euo pipefail

: "${QMAKE:=qmake6}"
PLUGIN_DIR="${PLUGIN_DIR:-/work/plugin-src/liteideaf}"
LITEIDEX="${LITEIDEX:-/src/liteide-linux/liteidex}"
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
    "CONFIG+=release" "CONFIG+=pic" "CONFIG-=precompile_header" \
    "IDE_SOURCE_TREE=${LITEIDEX}" \
    "IDE_PLUGIN_PATH=${DIST}/lib/liteide/plugins" \
    "QMAKE_CFLAGS+=-fPIC -pthread -D_REENTRANT -fno-semantic-interposition" \
    "QMAKE_CXXFLAGS+=-fPIC -pthread -D_REENTRANT -fno-semantic-interposition" \
    "QMAKE_LFLAGS+=-fPIC -pthread -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,--as-needed -Wl,--no-undefined"
make -j"${JOBS:-$(nproc)}"

# qmake plugin_with_soname peut poser libliteideaf.so, .so.1 ou .so.1.0.0,
# parfois uniquement un lien. On cherche large, liens compris.
SO="$(find "${DIST}" "${OUT}" "${PLUGIN_DIR}" \
        \( -name 'libliteideaf.so' -o -name 'libliteideaf.so.*' -o -name 'liteideaf.so' \) \
        2>/dev/null | head -n 1 || true)"
if [[ -z "${SO}" ]]; then
  echo "ERROR: libliteideaf.so introuvable après le make." >&2
  echo "-- contenu ${OUT} --" >&2
  find "${OUT}" -maxdepth 3 -type f -o -type l 2>/dev/null | head -n 40 >&2 || true
  echo "-- contenu ${DIST} --" >&2
  find "${DIST}" -maxdepth 4 \( -name '*.so' -o -name '*.so.*' \) 2>/dev/null | head -n 40 >&2 || true
  exit 1
fi
# Si c'est un lien, copier la cible réelle pour que l'hôte ait un fichier.
if [[ -L "${SO}" ]]; then
  SO="$(readlink -f "${SO}")"
fi
mkdir -p /opt/plugins-linux
cp -a "${SO}" "${OUT}/libliteideaf.so"
cp -a "${SO}" /opt/plugins-linux/libliteideaf.so

echo "== plugin so =="
ls -la "${OUT}/libliteideaf.so" /opt/plugins-linux/libliteideaf.so
echo "Puis : docker compose -f docker-compose.linux.yml run --rm export"
