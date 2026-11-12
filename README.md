# AgentPlurAI

A LiteIDE plugin that talks to AI engines from the editor. One Cline companion per open project, plus any number of local Ollama endpoints. The Z.ai key stays inside the Cline container. It is never stored by the plugin or written to the host.

## Stack

AgentPlurAI is the middle layer of a three-piece lab.

| Layer | Repository | Role |
| --- | --- | --- |
| Cluster | [mpi41-slurm-trixie](https://github.com/jmbatto/mpi41-slurm-trixie) | Debian Trixie image. Slurm 23.02, OpenMPI 4.1.8, MUNGE, supervisord. Compose brings up controller, database, and compute nodes. |
| Plugin | this repository | Qt plugin loaded by LiteIDE. Menu for prompts, history, health, quota, and project alignment. YAML contract for engines. |
| Companion | [companion-cline](https://github.com/jmbatto/agentplurai/tree/main/companion-cline) | Cline node derived from the cluster image. Adds Go and Cline. Joins the same Docker network. Holds the Z.ai token in a volume. |

The cluster is a functional simulation: jobs run as a normal user, not as root. The companion is a development harness. It sees only the volumes and network you give it. It does not mount the Docker socket, so it cannot escape to the host that runs Docker.

## What the plugin does

- Sends a prompt with a chosen scope: selection, current file, or project.
- Keeps a history of prompts and replies.
- Aligns the Cline container to the active LiteIDE project (recreates the container with the new mount, preserves the token volume).
- Reads health and remaining quota from the Cline container.
- Talks to one or more Ollama endpoints declared in YAML.
- Hides model reasoning by default. The behaviour is a setting.

The plugin does not call the Z.ai API itself. It asks the companion container to run Cline. The token is entered once inside that container.

## Layout

```
agentplurai/
├── plugin-src/liteideaf/     Qt plugin source
├── plugin-src/scaffold/      starting point for a new plugin
├── companion-cline/          Cline node (Dockerfile + compose)
├── docker-compose.linux.yml  build LiteIDE + plugin on Linux
├── docker-compose.windows.yml
└── distribute.sh             export the binary and plugins
```

## Build the plugin

Linux:

```bash
docker compose -f docker-compose.linux.yml build
docker compose -f docker-compose.linux.yml run --rm plugin
docker compose -f docker-compose.linux.yml run --rm export
```

Windows uses `docker-compose.windows.yml`. The export lands under `dist/`. Copy the LiteIDE tree and the plugin library to the host and launch the binary.

## Configure

The plugin reads `agentplurai.yaml` next to the project or from a path you set. Minimal shape:

```yaml
cline:
  container: cline-mpi
  workdir: /work/project
ollama:
  - name: qwen
    url: http://host.docker.internal:11434
    model: qwen2.5
hide_thinking: true
```

The Z.ai token is not in this file. It lives in the `cline-config` volume inside the companion.

## Security boundary

- The companion does not mount the Docker socket.
- Capabilities are dropped. `no-new-privileges` is set.
- The Cline process runs as `mpiuser` (uid 1001) through a wrapper. Supervisord stays root so the base image can start its daemons.
- MUNGE, if mounted, is read-only.
- MySQL of the cluster is on a dedicated internal network. Only `slurmdbd` can reach it.

## Build another plugin

Copy `plugin-src/scaffold/` to a new directory. Implement `LiteApi::IPlugin`. Point the build script at it. The scaffold shows the contract: load, menu actions, access to the open file and selection. See the scaffold README for the exact steps.

## Related

- Cluster image and compose: [mpi41-slurm-trixie](https://github.com/jmbatto/mpi41-slurm-trixie)
- Cline node details: [companion-cline](https://github.com/jmbatto/agentplurai/tree/main/companion-cline)
