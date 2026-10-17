#!/usr/bin/env bash
# Pose le binaire LiteIDE et le plugin de la cible demandée dans /dist.
# Usage: distribute.sh windows|linux
# REQUIRE_PLUGIN=1 (défaut) échoue si la DLL ou le .so n'est pas là.
set -euo pipefail

TARGET="${1:-}"
DIST="${DIST:-/dist}"
REQUIRE_PLUGIN="${REQUIRE_PLUGIN:-1}"

case "${TARGET}" in
  windows)
    IDE_SRC="${LITEIDE_WIN:-/opt/liteide-dist-win}"
    IDE_BIN="bin/liteide.exe"
    PLUGIN_NAME="liteideaf.dll"
    PLUGIN_CANDIDATES=(
      "/work/plugin-src/liteideaf/build-win/liteideaf.dll"
      "${PLUGIN_WIN:-/opt/plugins-win/liteideaf.dll}"
    )
    ;;
  linux)
    IDE_SRC="${LITEIDE_LINUX:-/opt/liteide-dist-linux}"
    IDE_BIN="bin/liteide"
    PLUGIN_NAME="libliteideaf.so"
    PLUGIN_CANDIDATES=(
      "/work/plugin-src/liteideaf/build-linux/libliteideaf.so"
      "/work/plugin-src/liteideaf/build-linux/libliteideaf.so.1"
      "${PLUGIN_LINUX:-/opt/plugins-linux/libliteideaf.so}"
    )
    ;;
  *)
    echo "ERROR: cible requise: windows ou linux." >&2
    exit 2
    ;;
esac

if [[ ! -e "${IDE_SRC}/${IDE_BIN}" ]]; then
  echo "ERROR: binaire absent: ${IDE_SRC}/${IDE_BIN}" >&2
  exit 1
fi

PLUGIN_SRC=""
for candidate in "${PLUGIN_CANDIDATES[@]}"; do
  if [[ -f "${candidate}" ]]; then
    PLUGIN_SRC="${candidate}"
    break
  fi
done
if [[ -z "${PLUGIN_SRC}" && "${REQUIRE_PLUGIN}" == "1" ]]; then
  echo "ERROR: plugin ${PLUGIN_NAME} introuvable." >&2
  printf '  cherché: %s\n' "${PLUGIN_CANDIDATES[@]}" >&2
  exit 1
fi

rm -rf "${DIST:?}/"*
mkdir -p "${DIST}"
# --no-preserve=ownership : un bind mount hôte refuse souvent chown.
cp -a --no-preserve=ownership "${IDE_SRC}/." "${DIST}/"
mkdir -p "${DIST}/lib/liteide/plugins"
if [[ -n "${PLUGIN_SRC}" ]]; then
  cp -a --no-preserve=ownership "${PLUGIN_SRC}" "${DIST}/lib/liteide/plugins/${PLUGIN_NAME}"
fi

test -e "${DIST}/${IDE_BIN}"
if [[ "${REQUIRE_PLUGIN}" == "1" ]]; then
  test -f "${DIST}/lib/liteide/plugins/${PLUGIN_NAME}"
fi

# Linux : le binaire est lié à libliteapp.so.1, qui n'est pas dans le
# chemin de recherche par défaut. On renomme le binaire et on pose un
# lanceur qui fixe LD_LIBRARY_PATH avant l'exec.
if [[ "${TARGET}" == "linux" ]]; then
  LIBDIR="${DIST}/lib/liteide"
  if [[ -f "${LIBDIR}/libliteapp.so" && ! -e "${LIBDIR}/libliteapp.so.1" ]]; then
    ln -s libliteapp.so "${LIBDIR}/libliteapp.so.1"
  fi
  if [[ -f "${LIBDIR}/libliteapp.so" && ! -e "${LIBDIR}/libliteapp.so.1.0" ]]; then
    ln -s libliteapp.so "${LIBDIR}/libliteapp.so.1.0"
  fi
  mv "${DIST}/bin/liteide" "${DIST}/bin/liteide.bin"
  cat > "${DIST}/bin/liteide" <<'EOF'
#!/bin/sh
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export LD_LIBRARY_PATH="${ROOT}/lib/liteide${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${ROOT}/bin/liteide.bin" "$@"
EOF
  chmod 755 "${DIST}/bin/liteide" "${DIST}/bin/liteide.bin"
fi

echo "== distribution ${TARGET} =="
echo "IDE  ${DIST}/${IDE_BIN}"
if [[ -n "${PLUGIN_SRC}" ]]; then
  echo "PLUGIN ${DIST}/lib/liteide/plugins/${PLUGIN_NAME} <= ${PLUGIN_SRC}"
else
  echo "PLUGIN absent (REQUIRE_PLUGIN=0)"
fi
ls -la "${DIST}/bin" "${DIST}/lib/liteide/plugins"
