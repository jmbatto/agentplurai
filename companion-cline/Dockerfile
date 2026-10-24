# Image dérivée : OpenMPI 4.1.8 + Slurm déjà présents.
# On ajoute seulement Go et Cline. Pas de openmpi-bin (conflit avec /usr/local).
# Pas de socket Docker : l'agent ne voit pas le démon.
FROM jmbatto/m2chps-mpi41-slurm:latest

ARG GO_VERSION=1.27.2
ARG CLINE_VERSION=3.0.70

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
    && command -v cline \
    && test -x "$(command -v cline)" \
    && curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" \
         | tar -C /usr/local -xz \
    && ln -sf /usr/local/go/bin/go /usr/local/bin/go \
    && ln -sf /usr/local/go/bin/gofmt /usr/local/bin/gofmt \
    && rm -rf /var/lib/apt/lists/* \
    && go version | grep -q "${GO_VERSION}" \
    && cline --version || true

RUN git config --system user.name agent \
    && git config --system user.email agent@local \
    && git config --system init.defaultBranch agent \
    && git config --system --add safe.directory '*'

# Sources OpenMPI alignées sur l'image Trixie (4.1.8).
# L'image de base les supprime après le make install.
ARG OPENMPI_VERSION=4.1.8
RUN curl -fsSL "https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-${OPENMPI_VERSION}.tar.gz" \
      | tar -xz -C /opt \
    && mv "/opt/openmpi-${OPENMPI_VERSION}" /opt/openmpi-src \
    && chown -R 1001:1001 /opt/openmpi-src \
    && test -f /opt/openmpi-src/opal/mca/btl/tcp/btl_tcp.c

# slurm_completion.sh est dans /etc/profile.d sur l'image de base.
# Un bash -lc (plugin) le source et il écrit sur stderr si bash-completion
# manque. On le sort du chemin de login. La complétion Slurm n'est pas utile ici.
RUN if [ -f /etc/profile.d/slurm_completion.sh ]; then \
      mkdir -p /usr/share/bash-completion/completions; \
      mv /etc/profile.d/slurm_completion.sh /usr/share/bash-completion/completions/; \
    fi

COPY entrypoint-cline.sh /usr/local/bin/entrypoint-cline.sh
COPY supervisord-cline.conf /etc/supervisor/conf.d/supervisord-cline.conf
# Install as root. Supervisord starts munged (as munge) then stays up.
# Same pattern as the other cluster nodes.
RUN chmod 755 /usr/local/bin/entrypoint-cline.sh \
    && mkdir -p /work/project /home/mpiuser/.cline /run/munge \
    && chown -R 1001:1001 /work/project /home/mpiuser/.cline \
    && chown munge:munge /run/munge \
    && ln -sf /usr/local/bin/mpirun /usr/local/bin/mpiexec \
    && test -x /usr/local/bin/mpicc \
    && test -x /opt/venv/bin/python3 \
    && test -x "$(command -v cline)"

USER root
WORKDIR /work/project
ENTRYPOINT ["/usr/local/bin/entrypoint-cline.sh"]
CMD ["supervisord", "-c", "/etc/supervisor/conf.d/supervisord-cline.conf"]
