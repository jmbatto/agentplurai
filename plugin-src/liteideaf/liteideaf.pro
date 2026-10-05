# Plugin LiteIDE-agentplurai — hors arbre officiel, compilé contre liteidex/src/api
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

# IPlugin / PluginFactoryT sont compilés dans liteapi, pas dans le plugin.
LIBS += -L$$IDE_SOURCE_TREE/liteide/lib/liteide
LIBS += -L/opt/liteide-dist/bin
LIBS += -L/opt/liteide-dist/lib/liteide
LIBS += -lliteapi

DEFINES += LITEIDEAF_LIBRARY

HEADERS += liteideafplugin.h
SOURCES += liteideafplugin.cpp
OTHER_FILES += README.md
