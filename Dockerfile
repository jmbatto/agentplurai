# syntax=docker/dockerfile:1.7

ARG DEBIAN=trixie-slim
ARG MXE_REF=3cd7eed9ffcc8f1a809fd4f0e6d0060b914e1d6d
ARG LITEIDE_REF=40ef40324b1997ae3cbb89e4c3a4de42e7e9af1c
ARG GO_VERSION=1.26.7

# ---------------------------------------------------------------------------
FROM debian:${DEBIAN} AS mxe
ARG MXE_REF
ARG MXE_TARGET=x86_64-w64-mingw32.shared
ARG JOBS=4

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    MXE_TARGET=${MXE_TARGET}

RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf automake autopoint bash bison bzip2 flex \
        g++ g++-multilib gettext git gperf intltool \
        libc6-dev-i386 libgdk-pixbuf-2.0-dev libltdl-dev libgl-dev \
        libpcre2-dev libssl-dev libtool-bin libxml-parser-perl \
        lzip make openssl p7zip-full patch perl pkg-config \
		curl \
	    libtool m4 libxml2 libxml2-dev iputils-ping colordiff \
	    nano jq neowofetch \
        python3 python3-mako python3-packaging python3-setuptools \
        python3-wheel python3-yaml python3-ply python3-jinja2 \
        python-is-python3 \
        ruby unzip wget xz-utils ca-certificates \
        ninja-build cmake meson file gawk \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone https://github.com/mxe/mxe.git \
    && git -C mxe checkout --detach "${MXE_REF}"

WORKDIR /opt/mxe
RUN make -j"${JOBS}" MXE_TARGETS="${MXE_TARGET}" \
        cc \
        qt6-qtbase qt6-qttools qt6-qtsvg qt6-qtimageformats qt6-qt5compat \
    && make clean-junk \
    && rm -rf pkg .ccache .git log tmp-* src \
    && test -x "usr/${MXE_TARGET}/qt6/bin/qmake"

ENV PATH="/opt/mxe/usr/bin:/opt/mxe/usr/${MXE_TARGET}/qt6/bin:${PATH}" \
    QT_MXE_PREFIX="/opt/mxe/usr/${MXE_TARGET}" \
    QMAKE="/opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/qmake"

# ---------------------------------------------------------------------------
FROM mxe AS build
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
    && /usr/local/bin/build-windows.sh \
    && mkdir -p /opt/liteide-dist \
    && cp -a /src/liteide/build/liteide/. /opt/liteide-dist/

RUN mkdir -p /opt/liteide-dist/bin \
    && cp -n /opt/mxe/usr/x86_64-w64-mingw32.shared/bin/*.dll \
            /opt/liteide-dist/bin/ \
    && cp -n /opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/*.dll \
            /opt/liteide-dist/bin/ \
    && test -f /opt/liteide-dist/bin/liteide.exe \
    && test -f /opt/liteide-dist/bin/icuuc78.dll \
    && test -f /opt/liteide-dist/bin/libbrotlidec.dll

# ---------------------------------------------------------------------------
FROM debian:${DEBIAN} AS runtime
ENV LANG=C.UTF-8 LC_ALL=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends bash \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /opt/liteide-dist /opt/liteide-dist
VOLUME ["/dist"]
WORKDIR /dist
CMD ["bash", "-lc", "cp -a /opt/liteide-dist/. /dist/ && echo '=== /dist/bin ===' && ls -la /dist/bin && test -f /dist/bin/liteide.exe"]

# ---------------------------------------------------------------------------
# Adaptateur Windows : Allégé via un Garbage Collection des statiques (.a)
FROM debian:${DEBIAN} AS dev
ARG MXE_TARGET=x86_64-w64-mingw32.shared

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    HOME=/root \
    MXE_TARGET=${MXE_TARGET} \
    LITEIDEX=/src/liteide/liteidex \
    DIST=/dist \
    GOTOOLCHAIN=local \
    PATH="/usr/local/go/bin:/opt/mxe/usr/bin:/opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin:${PATH}" \
    QMAKE="/opt/mxe/usr/x86_64-w64-mingw32.shared/qt6/bin/qmake"

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates curl git make gnupg libpcre2-16-0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/go /usr/local/go
COPY --from=build /src/liteide/liteidex /src/liteide/liteidex
COPY --from=build /opt/liteide-dist /opt/liteide-dist
COPY --from=build /opt/mxe/usr /opt/mxe/usr

# Purge chirurgicale pour ne conserver que les dll.a d'import dynamique
RUN find /opt/mxe/usr -type f -name "*.a" ! -name "*.dll.a" -delete \
    && rm -rf /opt/mxe/usr/x86_64-pc-linux-gnu/share/doc \
    && rm -rf /opt/mxe/usr/x86_64-pc-linux-gnu/share/man \
    && rm -rf /opt/mxe/usr/*/share/doc \
    && rm -rf /opt/mxe/usr/*/share/man

COPY build-plugin.sh /usr/local/bin/build-plugin.sh
COPY plugin-src /opt/plugin-skel
RUN chmod +x /usr/local/bin/build-plugin.sh \
    && mkdir -p /work

WORKDIR /work
CMD ["bash", "-lc", "test -d /work/plugin-src || cp -a /opt/plugin-skel/. /work/plugin-src/; exec bash"]

# ---------------------------------------------------------------------------
# Adaptateur Linux natif : Purifié de l'écosystème MXE, prêt pour le C/C++/Go natif
FROM debian:${DEBIAN} AS linux-dev

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    HOME=/root \
    LITEIDEX=/src/liteide/liteidex \
    DIST=/dist \
    GOTOOLCHAIN=local \
    PATH="/usr/local/go/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates curl git make gnupg \
        build-essential cmake \
        qt6-base-dev qt6-tools-dev qt6-l10n-tools \
        libpcre2-16-0 libpcre2-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/go /usr/local/go
COPY --from=build /src/liteide/liteidex /src/liteide/liteidex

COPY build-plugin.sh /usr/local/bin/build-plugin.sh
COPY plugin-src /opt/plugin-skel
RUN chmod +x /usr/local/bin/build-plugin.sh \
    && mkdir -p /work

WORKDIR /work
CMD ["bash", "-lc", "test -d /work/plugin-src || cp -a /opt/plugin-skel/. /work/plugin-src/; exec bash"]