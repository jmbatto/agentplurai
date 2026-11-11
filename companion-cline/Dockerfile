# Cline node. Inherits supervisord + SLURMPARAM handling from the base.
# Do not set ENTRYPOINT or CMD: the base CMD launches supervisord,
# which starts munged and (when SLURMPARAM=slurmd) slurmd.
# This file only adds Go/Cline and strips privilege from mpiuser (1001).
FROM jmbatto/m2chps-mpi41-slurm:latest

ARG GO_VERSION=1.27.4
ARG CLINE_VERSION=3.0.74

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 \
    GOTOOLCHAIN=local \
    GOPROXY=https://proxy.golang.org,direct \
    CGO_ENABLED=0 \
    PATH="/opt/venv/bin:/usr/local/go/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    VIRTUAL_ENV=/opt/venv \
    CLINE_TELEMETRY_DISABLED=true \
    DO_NOT_TRACK=1 \
    CLINE_OTEL_TELEMETRY_ENABLED=false

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl \
    && curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && npm install -g "cline@${CLINE_VERSION}" \
    && ln -sf "$(find /usr/local/lib/node_modules -path '*/bin/cline' -type f | head -1)" /usr/local/bin/cline \
    && command -v cline \
    && curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" \
         | tar -C /usr/local -xz \
    && ln -sf /usr/local/go/bin/go /usr/local/bin/go \
    && ln -sf /usr/local/go/bin/gofmt /usr/local/bin/gofmt \
    && rm -rf /var/lib/apt/lists/*

RUN git config --system user.name agent \
    && git config --system user.email agent@local \
    && git config --system init.defaultBranch agent \
    && git config --system --add safe.directory '*'

ARG OPENMPI_VERSION=4.1.8
RUN curl -fsSL "https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-${OPENMPI_VERSION}.tar.gz" \
      | tar -xz -C /opt \
    && mv "/opt/openmpi-${OPENMPI_VERSION}" /opt/openmpi-src \
    && chown -R 1001:1001 /opt/openmpi-src \
    && test -f /opt/openmpi-src/opal/mca/btl/tcp/btl_tcp.c

RUN if [ -f /etc/profile.d/slurm_completion.sh ]; then \
      mkdir -p /usr/share/bash-completion/completions; \
      mv /etc/profile.d/slurm_completion.sh /usr/share/bash-completion/completions/; \
    fi
	
RUN mkdir -p /home/mpiuser/.cline \
    && ln -sfn /home/mpiuser/.cline /root/.cline
# Strip privilege from 1001.
# - no sudo
# - no read on munge key or munge dirs
# - no write outside /work/project, /home/mpiuser, /opt/openmpi-src, /tmp
RUN sed -i '/mpiuser/d' /etc/sudoers \
    && rm -f /etc/sudoers.d/*mpiuser* 2>/dev/null || true \
    && chown munge:munge /etc/munge /var/log/munge /var/lib/munge /run/munge \
    && chmod 700 /etc/munge /var/log/munge /var/lib/munge \
    && chmod 755 /run/munge \
    && if [ -f /etc/munge/munge.key ]; then \
         chown munge:munge /etc/munge/munge.key && chmod 400 /etc/munge/munge.key; \
       fi \
    && mkdir -p /work/project /home/mpiuser/.cline \
    && chown -R 1001:1001 /work/project /home/mpiuser/.cline \
    && ln -sf /usr/local/bin/mpirun /usr/local/bin/mpiexec

# No ENTRYPOINT, no CMD. Base image CMD runs supervisord.
# Compose must set SLURMPARAM=slurmd and must not set user:.
WORKDIR /work/project
