#!/bin/bash
# Crée le dossier d'historique dans le projet monté, puis lance la commande.
set -e
mkdir -p /work/project/.agentplurai
touch /work/project/.agentplurai/prompts.log /work/project/.agentplurai/actions.log
exec "$@"
