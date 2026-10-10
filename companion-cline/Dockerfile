# Companion métier cline-project. Pas l'image de construction du plugin.
# Le socket Docker n'est pas monté : l'agent ne voit pas le démon.
FROM debian:trixie-slim

ARG GO_VERSION=1.27.2
ARG CLINE_VERSION=3.0.70

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 \
    DEBIAN_FRONTEND=noninteractive \
    GOTOOLCHAIN=local \
    GOPROXY=https://proxy.golang.org,direct \
    CGO_ENABLED=0 \
    PATH="/usr/local/go/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    CLINE_TELEMETRY_DISABLED=true \
    DO_NOT_TRACK=1 \
    CLINE_OTEL_TELEMETRY_ENABLED=false

RUN apt-get update && apt-get install -y --no-install-recommends \
        bash ca-certificates curl git make openssh-client \
    && curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && npm install -g "cline@${CLINE_VERSION}" \
    && curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" \
         | tar -C /usr/local -xz \
    && ln -sf /usr/local/go/bin/go /usr/local/bin/go \
    && ln -sf /usr/local/go/bin/gofmt /usr/local/bin/gofmt \
    && rm -rf /var/lib/apt/lists/* \
    && go version | grep -q "${GO_VERSION}"

# Le plugin lance `bash -lc`. Un shell login réécrit PATH et retire /usr/local/go/bin.
# /usr/local/bin survit à ce reset : c'est lui que voit `run_commands` de Cline.
RUN git config --system user.name agent \
    && git config --system user.email agent@local \
    && git config --system init.defaultBranch agent \
    && git config --system --add safe.directory '*'

# Banc Linux du problème. Ne fait pas partie du livrable Windows.
RUN apt-get update && apt-get install -y --no-install-recommends openmpi-bin \
    && rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR /work/project
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["bash"]
