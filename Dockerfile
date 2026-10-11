# ---------------------------------------------------------------------------
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

# NOUVEAU : Purge chirurgicale de l'espace MXE pour réduire drastiquement la taille
# On supprime toutes les librairies statiques (*.a) SAUF les librairies d'import dynamiques (*.dll.a)
# On supprime également les documentations et manpages inutiles dans un conteneur.
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