# syntax=docker/dockerfile:1.7
#
# Une image, deux cibles.
#   windows : LiteIDE MinGW + plugin DLL
#   linux   : LiteIDE natif + plugin .so
# La distribution copie le binaire et le plugin de la cible dans /dist.
# Elle échoue si l'un des deux manque.

ARG DEBIAN=trixie-slim
ARG MXE_REF=3cd7eed9ffcc8f1a809fd4f0e6d0060b914e1d6d
ARG LITEIDE_REF=40ef40324b1997ae3cbb89e4c3a4de42e7e9af1c
ARG GO_VERSION=1.26.7

# ---------------------------------------------------------------------------
FROM debian:${DEBIAN} AS mxe
ARG MXE_REF
ARG JOBS=auto
ARG MXE_TARGET=x86_64-w64-mingw32.shared

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    MXE_TARGET=${MXE_TARGET}

# Cœurs physiques visibles par le conteneur. Repli 6 si la détection échoue.
RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf automake autopoint bash bison bzip2 flex \
        g++ g++-multilib gettext git gperf intltool \
        libc6-dev-i386 libgdk-pixbuf-2.0-dev libltdl-dev libgl-dev \
        libpcre2-dev libssl-dev libtool-bin libxml-parser-perl \
        lzip make openssl p7zip-full patch perl pkg-config \
        curl libtool m4 libxml2 libxml2-dev \
        python3 python3-mako python3-packaging python3-setuptools \
        python3-wheel python3-yaml python3-ply python3-jinja2 \
        python-is-python3 \
        ruby unzip wget xz-utils ca-certificates \
        ninja-build cmake meson file gawk \
    && rm -rf /var/lib/apt/lists/* \
    && if [ "${JOBS}" = "auto" ] || [ -z "${JOBS}" ]; then \
         JOBS="$(awk '/^physical id/ {p=$$4} /^core id/ {print p","$$4}' /proc/cpuinfo | sort -u | wc -l)"; \
         [ "$${JOBS}" -ge 1 ] 2>/dev/null || JOBS="$(nproc)"; \
         [ "$${JOBS}" -ge 1 ] 2>/dev/null || JOBS=6; \
       fi \
    && echo "${JOBS}" > /etc/build-jobs \
    && echo "JOBS=$(cat /etc/build-jobs)"

WORKDIR /opt
RUN git clone https://github.com/mxe/mxe.git \
    && git -C mxe checkout --detach "${MXE_REF}"

WORKDIR /opt/mxe
RUN JOBS="$(cat /etc/build-jobs)" \
    && make -j"${JOBS}" MXE_TARGETS="${MXE_TARGET}" \
        cc \
        qt6-qtbase qt6-qttools qt6-qtsvg qt6-qtimageformats qt6-qt5compat \
    && make clean-junk \
    && rm -rf pkg .ccache .git log tmp-* src \
    && test -x "usr/${MXE_TARGET}/qt6/bin/qmake"

ENV PATH="/opt/mxe/usr/bin:/opt/mxe/usr/${MXE_TARGET}/qt6/bin:${PATH}" \
    QT_MXE_PREFIX="/opt/mxe/usr/${MXE_TARGET}" \
    QMAKE_WIN="/opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/qmake"

# ---------------------------------------------------------------------------
FROM mxe AS build-win
ARG LITEIDE_REF
ARG GOPROXY=https://proxy.golang.org,direct
ARG GO_VERSION

ENV GOPROXY=${GOPROXY} \
    GOPATH=/src/liteide/liteidex \
    GOOS=windows \
    GOARCH=amd64 \
    CGO_ENABLED=0 \
    GOTOOLCHAIN=local \
    PATH="/usr/local/go/bin:${PATH}"

RUN wget -qO- "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" \
        | tar -C /usr/local -xz \
    && go version | grep -q "${GO_VERSION}"

WORKDIR /src
RUN git clone https://github.com/visualfc/liteide.git \
    && git -C liteide checkout --detach "${LITEIDE_REF}" \
    && sed -i 's/^TARGET = libvterm/TARGET = vterm/' \
         liteide/liteidex/src/3rdparty/libvterm/libvterm.pro \
    && sed -i 's/qtLibraryName(libvterm)/qtLibraryName(vterm)/g' \
         liteide/liteidex/src/utils/vterm/vterm.pri \
         liteide/liteidex/src/3rdparty/libvterm/libvterm.pri \
    && sed -i -E '/^[[:space:]]*terminal[[:space:]]*(\\?)$/d' \
         liteide/liteidex/src/plugins/plugins.pro \
    && rm -rf liteide/.git

WORKDIR /src/liteide/build
COPY build-windows.sh /usr/local/bin/build-windows.sh
RUN chmod +x /usr/local/bin/build-windows.sh \
    && JOBS="$(cat /etc/build-jobs)" /usr/local/bin/build-windows.sh \
    && mkdir -p /opt/liteide-dist-win \
    && cp -a /src/liteide/build/liteide/. /opt/liteide-dist-win/ \
    && mkdir -p /opt/liteide-dist-win/bin \
    && cp -n /opt/mxe/usr/x86_64-w64-mingw32.shared/bin/*.dll \
            /opt/liteide-dist-win/bin/ \
    && cp -n /opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/*.dll \
            /opt/liteide-dist-win/bin/ \
    && test -f /opt/liteide-dist-win/bin/liteide.exe \
    && test -f /opt/liteide-dist-win/bin/icuuc78.dll \
    && test -f /opt/liteide-dist-win/bin/libbrotlidec.dll

# ---------------------------------------------------------------------------
FROM debian:${DEBIAN} AS build-linux
ARG LITEIDE_REF
ARG GO_VERSION
ARG JOBS=auto

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    PATH="/usr/local/go/bin:${PATH}" \
    QMAKE_LINUX="qmake6"

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates curl git make \
        build-essential cmake \
        qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-5compat-dev qt6-svg-dev \
        libpcre2-dev pkg-config wget \
    && rm -rf /var/lib/apt/lists/* \
    && if [ "${JOBS}" = "auto" ] || [ -z "${JOBS}" ]; then \
         JOBS="$(awk '/^physical id/ {p=$$4} /^core id/ {print p","$$4}' /proc/cpuinfo | sort -u | wc -l)"; \
         [ "$${JOBS}" -ge 1 ] 2>/dev/null || JOBS="$(nproc)"; \
         [ "$${JOBS}" -ge 1 ] 2>/dev/null || JOBS=6; \
       fi \
    && echo "${JOBS}" > /etc/build-jobs \
    && wget -qO- "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" \
        | tar -C /usr/local -xz \
    && go version | grep -q "${GO_VERSION}" \
    && command -v qmake6

WORKDIR /src
RUN git clone https://github.com/visualfc/liteide.git liteide-linux \
    && git -C liteide-linux checkout --detach "${LITEIDE_REF}" \
    && rm -rf liteide-linux/.git

COPY build-linux.sh /usr/local/bin/build-linux.sh
RUN chmod +x /usr/local/bin/build-linux.sh \
    && JOBS="$(cat /etc/build-jobs)" /usr/local/bin/build-linux.sh \
    && test -x /opt/liteide-dist-linux/bin/liteide

# ---------------------------------------------------------------------------
# Image unique. Les deux IDE, les deux toolchains, les deux scripts.
FROM build-win AS dev

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    HOME=/root \
    LITEIDEX=/src/liteide/liteidex \
    LITEIDE_WIN=/opt/liteide-dist-win \
    LITEIDE_LINUX=/opt/liteide-dist-linux \
    QMAKE_WIN=/opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/qmake \
    QMAKE_LINUX=qmake6 \
    GOTOOLCHAIN=local \
    PATH="/usr/local/go/bin:/opt/mxe/usr/bin:/opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates curl git make \
        build-essential cmake \
        qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-5compat-dev qt6-svg-dev \
        libpcre2-16-0 libpcre2-dev pkg-config \
    && rm -rf /var/lib/apt/lists/* \
    && command -v qmake6 \
    && test -x "${QMAKE_WIN}"

COPY --from=build-linux /opt/liteide-dist-linux /opt/liteide-dist-linux
COPY --from=build-linux /src/liteide-linux /src/liteide-linux
COPY build-plugin.sh /usr/local/bin/build-plugin.sh
COPY build-plugin-linux.sh /usr/local/bin/build-plugin-linux.sh
COPY distribute.sh /usr/local/bin/distribute.sh
COPY plugin-src /opt/plugin-skel
RUN chmod +x /usr/local/bin/build-plugin.sh \
        /usr/local/bin/build-plugin-linux.sh \
        /usr/local/bin/distribute.sh \
    && mkdir -p /work /opt/plugins-win /opt/plugins-linux \
    && test -f /opt/liteide-dist-win/bin/liteide.exe \
    && test -x /opt/liteide-dist-linux/bin/liteide

WORKDIR /work
CMD ["bash", "-lc", "test -d /work/plugin-src || cp -a /opt/plugin-skel/. /work/plugin-src/; exec bash"]

# ---------------------------------------------------------------------------
# Chemin Linux. Pas de MXE dans le PATH : qmake6 ne lie que le Qt Debian.
FROM build-linux AS dev-linux

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    HOME=/root \
    LITEIDEX=/src/liteide-linux/liteidex \
    LITEIDE_LINUX=/opt/liteide-dist-linux \
    QMAKE=/usr/bin/qmake6 \
    QMAKE_LINUX=/usr/bin/qmake6 \
    GOTOOLCHAIN=local \
    PATH="/usr/local/go/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig"

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates \
        build-essential \
        qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-5compat-dev qt6-svg-dev \
        libpcre2-dev pkg-config \
    && rm -rf /var/lib/apt/lists/* \
    && command -v qmake6 \
    && qmake6 -query QT_VERSION

COPY build-plugin-linux.sh /usr/local/bin/build-plugin-linux.sh
COPY distribute.sh /usr/local/bin/distribute.sh
COPY plugin-src /opt/plugin-skel
RUN chmod +x /usr/local/bin/build-plugin-linux.sh /usr/local/bin/distribute.sh \
    && mkdir -p /work /opt/plugins-linux \
    && test -x /opt/liteide-dist-linux/bin/liteide

WORKDIR /work
CMD ["bash", "-lc", "test -d /work/plugin-src || cp -a /opt/plugin-skel/. /work/plugin-src/; exec bash"]
