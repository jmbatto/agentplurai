# AgentPlurAI for LiteIDE
October 5th, 2026. 
© Jean-Michel Batto

AgentPlurAI is a LiteIDE plugin. It is the Windows desk for one Cline companion per open Go project, and for several local Ollama models.

## What the plugin does

LiteIDE stays a Windows process. Each AI engine stays in its own container. The plugin only names those engines in `agentplurai.yaml`.

For the open Go project there is one active Cline. Several Ollama endpoints may be up at the same time. The default engine is chosen in the AgentPlurAI menu.

The Prompt window freezes the scope when it opens:

- current selection
- current file (the editor visible at invocation, including unsaved text)
- project root

The message sent is the global system prompt, the engine system prompt, the chosen scope, then the user text.

Ollama is an HTTP call. Cline is the official `cline` binary, started with `docker exec` from the host. The prompt file is `/tmp/agentplurai-prompt.txt` inside the companion. The Docker socket is not mounted into that container.

Both engines share one result window. `rendering_markdown` chooses Markdown or plain text. `thinking_progress` shows or hides the reasoning. Tool traces (`[thinking]`, `[run_commands]`, `[editor]`) are not the answer.

The plugin can show health, prompt history, the action journal, the local Cline quota, and reload files the journal marks `write`. It will not reload a dirty editor buffer.

## Layout

```text
agentplurai/
  docker-compose.yml
  Dockerfile
  dist/                      host bind, filled by export
    bin/liteide.exe
    lib/liteide/plugins/     AgentPlurAI DLL lands here
  plugin-src/liteideaf/     plugin sources, bind-mounted into plugin-dev
```

Compose paths are relative. Run every command from `liteide-windows-docker/`.

## Operating procedure

Two images, in this order. Export fills `dist/`. Plugin-dev compiles the DLL into that same `dist/`.

### 1. Export the Windows tree

```bat
cd agentplurai
docker compose up export -d
```

Wait until `dist\bin\liteide.exe` is present. Export only copies the tree onto the host. It is not the editor and not the plugin build. Stop it:

```bat
docker compose stop export
```

`dist\` stays on disk. `dist\bin\liteide.exe` is the program to double-click.

### 2. Start the plugin workshop

```bat
docker compose up plugin-dev -d
docker compose exec plugin-dev bash
```

`plugin-dev` bind-mounts `./plugin-src` and `./dist`. Building the plugin writes the DLL into `dist\lib\liteide\plugins\`, next to the tree export just produced.

On a first run of an image built before `libpcre2-16-0` was added to the dev stage, `qmake` fails with a missing `libpcre2-16.so.0`. Inside the container:

```bash
apt-get update
apt-get install -y --no-install-recommends libpcre2-16-0
```

The package name is `libpcre2-16-0`. A new `plugin-dev` image already installs it.

### 3. Build the plugin

Sources are edited on the host under `plugin-src/liteideaf/`. Inside the container that tree is `/work/plugin-src/liteideaf`.

```bash
rm -rf /work/plugin-src/liteideaf/build-win
build-plugin.sh
```

Restart `liteide.exe` on Windows so it reloads the DLL.

Place `agentplurai.yaml` in the working directory. `dryRun: true` exercises the menu without Docker and without Z.ai.

## Companion

# TBD
