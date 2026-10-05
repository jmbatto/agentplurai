#!/usr/bin/env bash
# Cross-compile LiteIDE Qt 6 / MinGW-w64 (MXE).
# Équivalent : build_windows_msvc_qt6.cmd + deploy_windows_qt6.cmd
set -euo pipefail

: "${MXE_TARGET:=x86_64-w64-mingw32.shared}"
: "${QT_MXE_PREFIX:=/opt/mxe/usr/${MXE_TARGET}}"

resolve_qmake() {
  local c
  for c in \
      "${QMAKE:-}" \
      "/opt/mxe/usr/${MXE_TARGET}/qt6/bin/qmake" \
      "/opt/mxe/usr/bin/${MXE_TARGET}-qmake-qt6" \
      "/opt/mxe/usr/bin/${MXE_TARGET}-qmake" \
      "/opt/mxe/usr/x86_64-pc-linux-gnu/qt6/bin/qmake"
  do
    [[ -n "${c}" && -x "${c}" ]] || continue
    if "${c}" -query QT_VERSION 2>/dev/null | grep -q '^6\.'; then
      echo "${c}"
      return 0
    fi
  done
  echo "ERROR: aucun qmake Qt 6 exécutable. Candidats :" >&2
  ls -la /opt/mxe/usr/bin/*qmake* /opt/mxe/usr/*/qt6/bin/qmake 2>/dev/null || true
  return 1
}

if [[ -d /src/liteide/build ]]; then
  BUILD_ROOT=/src/liteide/build
else
  BUILD_ROOT="$(pwd)"
fi
LITEIDE_ROOT="${LITEIDE_ROOT:-$(cd "${BUILD_ROOT}/../liteidex" && pwd)}"
SRC_ROOT="$(cd "${LITEIDE_ROOT}/.." && pwd)"

QMAKE="$(resolve_qmake)"
echo "== LiteIDE Windows Qt6 cross-build =="
echo "BUILD_ROOT=${BUILD_ROOT}"
echo "LITEIDE_ROOT=${LITEIDE_ROOT}"
echo "QMAKE=${QMAKE}"
"${QMAKE}" -v
QT_VER="$("${QMAKE}" -query QT_VERSION || true)"
echo "QT_VERSION=${QT_VER}"
case "${QT_VER}" in
  6.*) ;;
  *) echo "ERROR: qmake n'est pas Qt 6 (obtenu: '${QT_VER}')" >&2; exit 1 ;;
esac
go version

export GOPATH="${LITEIDE_ROOT}"
export GOOS=windows GOARCH=amd64 CGO_ENABLED=0
mkdir -p "${LITEIDE_ROOT}/bin"

go install -ldflags "-s -w" github.com/visualfc/gotools@latest || true
go install -ldflags "-s -w" github.com/visualfc/gocode@latest || true
go install -ldflags "-s -w" github.com/fatih/gomodifytags@latest || true
if [[ -d "${HOME}/go/bin/windows_amd64" ]]; then
  cp -f "${HOME}/go/bin/windows_amd64/"*.exe "${LITEIDE_ROOT}/bin/" 2>/dev/null || true
fi
if [[ -d "${GOPATH}/bin" ]]; then
  find "${GOPATH}/bin" -maxdepth 2 -name '*.exe' -exec cp -f {} "${LITEIDE_ROOT}/bin/" \;
fi

(
  cd "${LITEIDE_ROOT}"
  GOOS=linux GOARCH=amd64 CGO_ENABLED=0 \
    go run src/tools/exportqrc/main.go -root . || \
    echo "WARN: exportqrc ignoré"
)

cd "${LITEIDE_ROOT}"
# terminal.dll : -llibvterm (qtLibraryName(libvterm)) → ld MinGW cherche liblibvterm.
# Plugin hors besoin pour valider liteide.exe + agentplurai.
if [[ -f src/plugins/plugins.pro ]]; then
  sed -i -E '/^[[:space:]]*terminal[[:space:]]*(\\?)$/d' src/plugins/plugins.pro
fi
rm -f Makefile Makefile.* .qmake.stash
"${QMAKE}" "${LITEIDE_ROOT}" "CONFIG+=release" "CONFIG-=precompile_header"

MAKE_J="${JOBS:-$(nproc)}"
make -j"${MAKE_J}"

STAGE="${BUILD_ROOT}/liteide"
rm -rf "${STAGE}"
mkdir -p \
  "${STAGE}/bin" \
  "${STAGE}/lib/liteide/plugins" \
  "${STAGE}/share/liteide"

cp -f "${LITEIDE_ROOT}/LICENSE.LGPL" "${STAGE}/" 2>/dev/null || true
cp -f "${LITEIDE_ROOT}/LGPL_EXCEPTION.TXT" "${STAGE}/" 2>/dev/null || true
cp -f "${SRC_ROOT}/README.md" "${STAGE}/" 2>/dev/null || true
cp -f "${SRC_ROOT}/CONTRIBUTORS" "${STAGE}/" 2>/dev/null || true

if [[ -d "${LITEIDE_ROOT}/liteide/bin" ]]; then
  find "${LITEIDE_ROOT}/liteide/bin" -maxdepth 1 -type f \( -name '*.exe' -o -name '*.dll' \) \
    -exec cp -f {} "${STAGE}/bin/" \;
fi
if [[ -d "${LITEIDE_ROOT}/liteide/lib/liteide/plugins" ]]; then
  find "${LITEIDE_ROOT}/liteide/lib/liteide/plugins" -maxdepth 1 -name '*.dll' \
    -exec cp -f {} "${STAGE}/lib/liteide/plugins/" \;
fi
find "${LITEIDE_ROOT}" -maxdepth 4 \( -name 'liteide.exe' -o -name 'liteapp.dll' \) \
  -exec cp -f {} "${STAGE}/bin/" \; || true
cp -f "${LITEIDE_ROOT}/bin/"*.exe "${STAGE}/bin/" 2>/dev/null || true

if [[ -d "${LITEIDE_ROOT}/deploy" ]]; then
  cp -a "${LITEIDE_ROOT}/deploy/." "${STAGE}/share/liteide/"
fi
if [[ -d "${LITEIDE_ROOT}/os_deploy/windows" ]]; then
  cp -a "${LITEIDE_ROOT}/os_deploy/windows/." "${STAGE}/share/liteide/"
fi

WDEP=""
for c in \
    "/opt/mxe/usr/bin/${MXE_TARGET}-windeployqt-qt6" \
    "/opt/mxe/usr/bin/${MXE_TARGET}-windeployqt" \
    "${QT_MXE_PREFIX}/qt6/bin/windeployqt"
do
  if [[ -x "${c}" ]]; then WDEP="${c}"; break; fi
done

deploy_one() {
  local f="$1"
  if [[ -f "${f}" && -n "${WDEP}" ]]; then
    echo "windeployqt --dir ${STAGE}/bin ${f}"
    "${WDEP}" --release --dir "${STAGE}/bin" "${f}" || true
  fi
}

# Même liste que deploy_windows_qt6.cmd
deploy_one "${STAGE}/lib/liteide/plugins/liteeditor.dll"
deploy_one "${STAGE}/lib/liteide/plugins/welcome.dll"
deploy_one "${STAGE}/lib/liteide/plugins/litebuild.dll"
deploy_one "${STAGE}/lib/liteide/plugins/dlvdebugger.dll"
deploy_one "${STAGE}/bin/liteapp.dll"
deploy_one "${STAGE}/bin/liteide.exe"

QT_BIN="${QT_MXE_PREFIX}/qt6/bin"
QT_PLUGINS="${QT_MXE_PREFIX}/qt6/plugins"
MINGW_BIN="${QT_MXE_PREFIX}/bin"
copy_if() { [[ -f "$1" ]] && cp -f "$1" "${STAGE}/bin/"; }
for dll in \
    Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Xml Qt6Svg Qt6PrintSupport \
    Qt6Core5Compat \
    libgcc_s_seh-1 libstdc++-6 libwinpthread-1 \
    zlib1 libpng16-16 libharfbuzz-0 libpcre2-16-0 libfreetype-6 \
    libglib-2.0-0 libintl-8 libiconv-2 libpcre-1 libbz2 libzstd \
    icuuc78 icuin78 icuio78 icudt78 \
    libbrotlidec libbrotlicommon libbrotlienc \
    libpcre2-8-0 libpcre2-32-0 libdouble-conversion \
    libmd4c libmd4c-html libpng16-16
do
  copy_if "${QT_BIN}/${dll}.dll" || true
  copy_if "${MINGW_BIN}/${dll}.dll" || true
done
mkdir -p "${STAGE}/bin/platforms" "${STAGE}/bin/imageformats" "${STAGE}/bin/iconengines" "${STAGE}/bin/styles"
[[ -d "${QT_PLUGINS}/platforms" ]] && cp -f "${QT_PLUGINS}/platforms/"qwindows*.dll "${STAGE}/bin/platforms/" 2>/dev/null || true
[[ -d "${QT_PLUGINS}/imageformats" ]] && cp -f "${QT_PLUGINS}/imageformats/"*.dll "${STAGE}/bin/imageformats/" 2>/dev/null || true
[[ -d "${QT_PLUGINS}/iconengines" ]] && cp -f "${QT_PLUGINS}/iconengines/"*.dll "${STAGE}/bin/iconengines/" 2>/dev/null || true
[[ -d "${QT_PLUGINS}/styles" ]] && cp -f "${QT_PLUGINS}/styles/"*.dll "${STAGE}/bin/styles/" 2>/dev/null || true

echo "== staging =="
find "${STAGE}" -maxdepth 3 -type f | head -n 100
test -f "${STAGE}/bin/liteide.exe"
echo "OK ${STAGE}/bin/liteide.exe"
