# companion-cline

A Cline node that joins the [mpi41-slurm-trixie](https://github.com/jmbatto/mpi41-slurm-trixie) cluster. It is the development harness for [AgentPlurAI](https://github.com/jmbatto/agentplurai).

## Role

The base image provides Slurm, OpenMPI, MUNGE, and supervisord. This image adds:

- Go (pinned version)
- Cline (pinned version), installed globally
- OpenMPI sources under `/opt/openmpi-src` for reference
- A wrapper so the `cline` command runs as `mpiuser` (uid 1001)

The container stays root. Supervisord needs root to start munged and slurmd. The wrapper drops privileges only for the Cline process.

## Why a companion

AgentPlurAI does not call Z.ai directly. It asks this container to run Cline. The Z.ai token is stored in the `cline-config` volume, mounted at `/home/mpiuser/.cline`. It is never written to the plugin or to the host filesystem outside that volume.

The companion sees the project through a bind mount at `/work/project`. When LiteIDE switches project, the plugin recreates the container with the new mount and keeps the same token volume.

## Build and run

From this directory, with the cluster already up on the external network `mpinet`:

```bash
docker compose build
docker compose up -d
```

Enter the token once:

```bash
docker exec -it cline-mpi cline auth
```

The plugin then talks to the container by name.

## Compose contract

The compose file does not set `user`, `entrypoint`, or `command`. The base image owns process startup. The service joins the external network `mpinet` and reuses the cluster volumes for the project, the job directory, and the shared MPI path.

MUNGE is mounted read-only when present. The companion can authenticate to the cluster. It cannot rewrite the key.

## What stays on the host

| Path in container | Origin | Notes |
| --- | --- | --- |
| `/work/project` | cluster volume or host bind | ownership is that of the source; the image does not chown it at runtime |
| `/home/mpiuser/.cline` | `cline-config` volume | token and Cline state |
| `/etc/munge` | cluster volume, `:ro` | optional |

The Docker socket is not mounted. The agent cannot see or control the Docker daemon on the host.

## Limits

- `docker exec` without going through the `cline` wrapper is still root. The plugin must call `cline`, not a raw shell that writes into the project.
- Jobs submitted from this container as root arrive as root on the compute nodes. Prefer submitting as `mpiuser`, or submit from a compute node.
- Resource limits (cgroup) are not active. The base image does not have `CAP_SYS_ADMIN`. `DefMemPerCPU` is declarative.

## Related

- Cluster: [mpi41-slurm-trixie](https://github.com/jmbatto/mpi41-slurm-trixie)
- Plugin and entry point: [agentplurai](https://github.com/jmbatto/agentplurai)
