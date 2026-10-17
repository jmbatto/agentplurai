# AgentPlurAI for LiteIDE

One image, two deliveries. The image holds the MinGW toolchain and the native Qt 6 toolchain. Each OS has its own compose file, which fixes the plugin script and the output directory.

```text
liteide-windows-docker/
  Dockerfile
  docker-compose.windows.yml   -> image liteide-dev-windows:qt6 -> ./dist-win
  docker-compose.linux.yml     -> image liteide-dev-linux:qt6  -> ./dist-linux
  build-windows.sh             LiteIDE cross-build
  build-linux.sh               LiteIDE native build
  build-macos.sh               LiteIDE native, run on a Mac
  build-plugin.sh              plugin DLL
  build-plugin-linux.sh        plugin .so
  build-plugin-macos.sh        plugin .dylib, run on a Mac
  distribute.sh                IDE + plugin into /dist
  plugin-src/liteideaf/        plugin sources
```

Run from this directory.

Windows:

```bash
docker compose -f docker-compose.windows.yml build
docker compose -f docker-compose.windows.yml run --rm plugin
docker compose -f docker-compose.windows.yml run --rm export
```

Linux:

```bash
docker compose -f docker-compose.linux.yml build
docker compose -f docker-compose.linux.yml run --rm plugin
docker compose -f docker-compose.linux.yml run --rm export
```

macOS (sur un Mac, pas dans Docker — le SDK Apple n'est pas cross-compilable) :

```bash
brew install qt go
export PATH="$(brew --prefix qt)/bin:${PATH}"
./build-macos.sh
./build-plugin-macos.sh
```

Résultat dans `./dist-mac/LiteIDE.app`.

`plugin` must run before `export`. The plugin binary is written into the bind-mounted `plugin-src` tree; `export` picks it up and places it next to the LiteIDE binary.
