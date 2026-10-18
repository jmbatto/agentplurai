# LiteIDE, built and extended

One image, three deliveries. The image holds the MinGW toolchain and the native Qt 6 toolchain. Each OS has its own compose file, which fixes the plugin script and the output directory.

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
  plugin-src/liteideaf/        AgentPlurAI plugin sources
```

Run from this directory.

## Windows

```bash
docker compose -f docker-compose.windows.yml build
docker compose -f docker-compose.windows.yml run --rm plugin
docker compose -f docker-compose.windows.yml run --rm export
```

Result: `./dist-win/bin/liteide.exe` and `liteideaf.dll`.

## Linux

```bash
docker compose -f docker-compose.linux.yml build
docker compose -f docker-compose.linux.yml run --rm plugin
docker compose -f docker-compose.linux.yml run --rm export
```

Result: `./dist-linux/bin/liteide`. A wrapper sets `LD_LIBRARY_PATH` and `LITEIDE_RES_PATH`. Qt 6 is bundled.

## macOS

The Apple SDK is not cross-compilable. Run on a Mac, not in Docker.

```bash
brew install qt go
export PATH="$(brew --prefix qt)/bin:${PATH}"
./build-macos.sh
./build-plugin-macos.sh
```

Result: `./dist-mac/LiteIDE.app`.

`plugin` must run before `export`. The plugin binary is written into the bind-mounted `plugin-src` tree; `export` picks it up and places it next to the LiteIDE binary.

## Why the plugin

LiteIDE is a small Go IDE. It has no agent. AgentPlurAI adds one menu that talks to models without leaving the editor.

- One Cline per open Go project, inside a Docker companion. The Z.ai key stays in the container volume `cline-config`, never in the plugin.
- Several Ollama servers in parallel. Each is an HTTP endpoint in the YAML.
- The plugin sees the open folder, the current file, and the selection. It sends one of those to the active engine.
- Align recreates the container when the project changes. Only the project mount changes. The key is kept.

Menu, YAML, and multi-Ollama details: `plugin-src/liteideaf/README.md`.

| Entry | Role |
|---|---|
| Scope | Open folder, current file, selection. |
| Prompt | Input, radio selection / file / project, send to the active engine. |
| History | `.agentplurai/prompts.log`. |
| Journal | `.agentplurai/actions.log`. |
| Reload | Reopen files the agent wrote. |
| Default AI | Engine used by Prompt. |
| Align project | Recreate the container if it is down or mounts the wrong folder. Keeps the key. |
| System prompt | Global prompt and per-engine prompt. |
| Hide thinking | Check box. Hides or shows the model reasoning (`thinking_progress`). |
| Health | Plugin version, Cline state, Ollama reachability. |
| Cline quota | Remaining Z.ai credit, read from the key already in the container. |

## Building another plugin

This tree is a working example, not a closed product. Copy `plugin-src/liteideaf/` and change the names. The build path (compose, qmake, `distribute.sh`) stays the same.

The example below is called `scaffold`. It is the smallest plugin that loads, adds a menu, and does nothing else. Use it as the starting point for a real one.

### 1. Copy the skeleton

```text
plugin-src/scaffold/
  scaffold.pro
  scaffoldplugin.h
  scaffoldplugin.cpp
```

### 2. The contract

A LiteIDE plugin is a Qt plugin that implements `LiteApi::IPlugin`. The factory declares the id. The id is what LiteIDE loads.

`scaffoldplugin.h`:

```cpp
class ScaffoldPlugin : public LiteApi::IPlugin {
    Q_OBJECT
public:
    ScaffoldPlugin();
    bool load(LiteApi::IApplication *app) override;
private:
    LiteApi::IApplication *m_app = nullptr;
};

class PluginFactory : public LiteApi::PluginFactoryT<ScaffoldPlugin> {
    Q_OBJECT
    Q_INTERFACES(LiteApi::IPluginFactory)
    Q_PLUGIN_METADATA(IID "liteidex.ScaffoldPlugin")
public:
    PluginFactory() {
        m_info->setId("plugin/scaffold");
        m_info->setName("Scaffold");
        m_info->setVer("0.1");
        m_info->appendDepend("plugin/liteeditor");
    }
};
```

`scaffoldplugin.cpp`:

```cpp
bool ScaffoldPlugin::load(LiteApi::IApplication *app) {
    if (!app) return false;
    m_app = app;
    QMenu *menu = app->actionManager()->insertMenu("menu/scaffold", "Scaffold");
    if (!menu) return true;
    QAction *act = menu->addAction("Hello");
    connect(act, &QAction::triggered, this, [app]() {
        QMessageBox::information(app->mainWindow(), "Scaffold", "Plugin loaded.");
    });
    return true;
}
```

Rules that matter:

- `TARGET` in the `.pro` is the binary name (`scaffold.dll`, `libscaffold.so`).
- `CONFIG += plugin` makes it a Qt plugin, not a program.
- `IDE_SOURCE_TREE` points at `liteidex`. The headers live in `src/api`.
- Link `liteapi`. On Linux it must be built with `-fPIC` (already done by `build-linux.sh`).
- `DESTDIR` is the plugins directory. `distribute.sh` copies whatever lands there.
- Do not edit LiteIDE's own `plugins.pro`. The plugin is built outside the official tree.

### 3. The `.pro`

```qmake
TARGET = scaffold
TEMPLATE = lib
QT += widgets
CONFIG += plugin plugin_with_soname c++17
CONFIG -= debug debug_and_release

isEmpty(IDE_SOURCE_TREE): IDE_SOURCE_TREE = /src/liteide/liteidex
isEmpty(IDE_PLUGIN_PATH): IDE_PLUGIN_PATH = /dist/lib/liteide/plugins

INCLUDEPATH += $$IDE_SOURCE_TREE/src/api $$IDE_SOURCE_TREE/src/api/liteapi
DESTDIR = $$IDE_PLUGIN_PATH
LIBS += -L$$IDE_SOURCE_TREE/liteide/lib/liteide -lliteapi

HEADERS += scaffoldplugin.h
SOURCES += scaffoldplugin.cpp
```

On Linux, add `CONFIG += pic` and `-fPIC` to `QMAKE_CXXFLAGS` and `QMAKE_LFLAGS`, as `liteideaf.pro` does. A static `liteapi.a` without PIC will not link into a `.so`.

### 4. Build and ship

Point the compose plugin service at `plugin-src/scaffold`, or add a second script that runs `qmake && make` in that directory. Then:

```bash
docker compose -f docker-compose.linux.yml run --rm plugin
docker compose -f docker-compose.linux.yml run --rm export
```

The binary appears in `dist-linux/lib/liteide/plugins/`. Restart LiteIDE. The menu **Scaffold** is there if the id is unique and the dependency `plugin/liteeditor` is loaded.

### 5. What to change for a real plugin

| Piece | Where | What to set |
|---|---|---|
| Id | `setId` | `plugin/<name>`, unique. |
| Menu | `insertMenu` | Label the user sees. |
| Actions | `menu->addAction` | One slot per verb. |
| Settings | `app->settings()` | Persist small state. |
| Files | `app->fileManager()`, `app->editorManager()` | Current file, selection, open folder. |
| Version | a `#define` in the header | Shown in a Health entry. Bump it on each delivery. |

AgentPlurAI is the long form of this: a menu, a YAML contract, and calls to Docker or HTTP. A new plugin can stop at the menu, or grow the same way.
