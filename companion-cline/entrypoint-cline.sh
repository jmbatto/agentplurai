#!/bin/bash
# Prepare dirs, then hand off to supervisord (PID 1).
# munged -F runs as user munge, same as the other nodes.
set -e
mkdir -p /run/munge /var/log/munge /var/lib/munge /work/project/.agentplurai
chown munge:munge /run/munge /var/log/munge /var/lib/munge
chmod 755 /run/munge
touch /work/project/.agentplurai/prompts.log /work/project/.agentplurai/actions.log
chown -R 1001:1001 /work/project/.agentplurai 2>/dev/null || true
export PATH="/opt/venv/bin:/usr/local/go/bin:/usr/local/bin:/usr/bin:${PATH}"
export VIRTUAL_ENV=/opt/venv
exec "$@"
