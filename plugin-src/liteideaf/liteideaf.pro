# Plugin LiteIDE-AgentField — hors arbre officiel, compilé contre liteidex/src/api
TARGET = liteideaf
TEMPLATE = lib
QT += widgets printsupport network
CONFIG += plugin plugin_with_soname c++17
CONFIG -= debug debug_and_release

isEmpty(IDE_SOURCE_TREE): IDE_SOURCE_TREE = $$(LITEIDEX)
isEmpty(IDE_SOURCE_TREE): IDE_SOURCE_TREE = /src/liteide/liteidex
isEmpty(IDE_PLUGIN_PATH): IDE_PLUGIN_PATH = /dist/lib/liteide/plugins

INCLUDEPATH += \
    $$IDE_SOURCE_TREE/src/api \
    $$IDE_SOURCE_TREE/src/api/liteapi \
    $$IDE_SOURCE_TREE/src/utils

DESTDIR = $$IDE_PLUGIN_PATH

# Deux chemins, pas de factorisation.
# Windows : MinGW lie la staticlib telle quelle.
# Linux   : liteapi.a est reconstruite en -fPIC par build-linux.sh,
#           puis liée normalement dans le .so.
win32 {
    LIBS += -L$$IDE_SOURCE_TREE/liteide/lib/liteide
    LIBS += -L/opt/liteide-dist-win/bin
    LIBS += -L/opt/liteide-dist-win/lib/liteide
    LIBS += -lliteapi
} else:macx {
    LIBS += -L$$IDE_SOURCE_TREE/liteide/lib/liteide
    LIBS += -L$$IDE_SOURCE_TREE/liteide/bin/LiteIDE.app/Contents/PlugIns
    LIBS += -lliteapi
} else {
    # Plugin ELF. PIC, durcissement, et échec immédiat si un symbole manque.
    CONFIG += pic
    QMAKE_CFLAGS += -fPIC -pthread -D_REENTRANT -fno-semantic-interposition
    QMAKE_CXXFLAGS += -fPIC -pthread -D_REENTRANT -fno-semantic-interposition
    QMAKE_LFLAGS += -fPIC -pthread \
        -Wl,-z,relro \
        -Wl,-z,now \
        -Wl,-z,noexecstack \
        -Wl,--as-needed \
        -Wl,--no-undefined
    LIBS += -L$$IDE_SOURCE_TREE/liteide/lib/liteide
    LIBS += -L/opt/liteide-dist-linux/lib/liteide
    LIBS += -lliteapi
}

DEFINES += LITEIDEAF_LIBRARY

HEADERS += liteideafplugin.h
SOURCES += liteideafplugin.cpp
OTHER_FILES += README.md
