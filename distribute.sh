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

# Linux : le binaire est lié à libliteapp.so.1. La livraison ne contient
# souvent que le lien libliteapp.so. On récupère la cible réelle dans
# l'arbre de build, puis on pose le lanceur.
if [[ "${TARGET}" == "linux" ]]; then
  LIBDIR="${DIST}/lib/liteide"
  SRC_LIB="/src/liteide-linux/liteidex/liteide/lib/liteide"
  shopt -s nullglob
  for lib in "${SRC_LIB}"/libliteapp.so*; do
    cp -a --no-preserve=ownership "${lib}" "${LIBDIR}/"
  done
  shopt -u nullglob
  if [[ -L "${LIBDIR}/libliteapp.so" && ! -e "${LIBDIR}/libliteapp.so" ]]; then
    if [[ -f "${SRC_LIB}/libliteapp.so" ]]; then
      cp -aL --no-preserve=ownership "${SRC_LIB}/libliteapp.so" "${LIBDIR}/libliteapp.so.1"
      ln -sf libliteapp.so.1 "${LIBDIR}/libliteapp.so"
    fi
  fi
  if [[ ! -e "${LIBDIR}/libliteapp.so.1" ]]; then
    echo "ERROR: libliteapp.so.1 absent dans ${LIBDIR} et ${SRC_LIB}" >&2
    ls -la "${LIBDIR}" "${SRC_LIB}" 2>/dev/null || true
    exit 1
  fi

  # Qt 6.11 vient de l'image, pas de l'hôte. On copie les .so demandés
  # par le binaire et le plugin de plateforme, puis un qt.conf.
  QT_LIB="${DIST}/lib/qt6"
  QT_PLUG="${DIST}/plugins"
  mkdir -p "${QT_LIB}" "${QT_PLUG}/platforms"
  ldd "${DIST}/bin/liteide" | awk '/=>/ { print $3 }' | while read -r so; do
    case "${so}" in
      *libQt6*|*libicu*|*libpcre2*|*libdouble-conversion*|*libb2*|*libmd4c*)
        cp -a --no-preserve=ownership "${so}" "${QT_LIB}/" 2>/dev/null || true
        ;;
    esac
  done
  for plug in /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms/libqxcb.so \
              /usr/lib/qt6/plugins/platforms/libqxcb.so; do
    if [[ -f "${plug}" ]]; then
      cp -a --no-preserve=ownership "${plug}" "${QT_PLUG}/platforms/"
      ldd "${plug}" | awk '/=>/ { print $3 }' | while read -r so; do
        case "${so}" in
          *libQt6*|*libxcb*|*libxkb*|*libicu*|*libpcre2*)
            cp -a --no-preserve=ownership "${so}" "${QT_LIB}/" 2>/dev/null || true
            ;;
        esac
      done
      break
    fi
  done
  cat > "${DIST}/bin/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Libraries = lib/qt6
Plugins = plugins
EOF

  mv "${DIST}/bin/liteide" "${DIST}/bin/liteide.bin"
  cat > "${DIST}/bin/liteide" <<'EOF'
#!/bin/sh
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export LD_LIBRARY_PATH="${ROOT}/lib/liteide:${ROOT}/lib/qt6${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export QT_PLUGIN_PATH="${ROOT}/plugins"
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
