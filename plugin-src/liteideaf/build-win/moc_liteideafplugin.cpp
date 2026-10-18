/****************************************************************************
** Meta object code from reading C++ file 'liteideafplugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../liteideafplugin.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'liteideafplugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN15LiteIdeAfPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto LiteIdeAfPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN15LiteIdeAfPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "LiteIdeAfPlugin",
        "health",
        "",
        "showScope",
        "sendPrompt",
        "showHistory",
        "editSystemPrompt",
        "showJournal",
        "showQuota",
        "reloadWrites",
        "setDefaultAI",
        "alignProject",
        "toggleHideThinking",
        "onOllamaFinished",
        "onClineFinished",
        "code",
        "status"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'health'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showScope'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'sendPrompt'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showHistory'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'editSystemPrompt'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showJournal'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showQuota'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'reloadWrites'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'setDefaultAI'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'alignProject'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'toggleHideThinking'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onOllamaFinished'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClineFinished'
        QtMocHelpers::SlotData<void(int, int)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { QMetaType::Int, 16 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LiteIdeAfPlugin, qt_meta_tag_ZN15LiteIdeAfPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject LiteIdeAfPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<LiteApi::IPlugin::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LiteIdeAfPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LiteIdeAfPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15LiteIdeAfPluginE_t>.metaTypes,
    nullptr
} };

void LiteIdeAfPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LiteIdeAfPlugin *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->health(); break;
        case 1: _t->showScope(); break;
        case 2: _t->sendPrompt(); break;
        case 3: _t->showHistory(); break;
        case 4: _t->editSystemPrompt(); break;
        case 5: _t->showJournal(); break;
        case 6: _t->showQuota(); break;
        case 7: _t->reloadWrites(); break;
        case 8: _t->setDefaultAI(); break;
        case 9: _t->alignProject(); break;
        case 10: _t->toggleHideThinking(); break;
        case 11: _t->onOllamaFinished(); break;
        case 12: _t->onClineFinished((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject *LiteIdeAfPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LiteIdeAfPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LiteIdeAfPluginE_t>.strings))
        return static_cast<void*>(this);
    return LiteApi::IPlugin::qt_metacast(_clname);
}

int LiteIdeAfPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = LiteApi::IPlugin::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}
namespace {
struct qt_meta_tag_ZN13PluginFactoryE_t {};
} // unnamed namespace

template <> constexpr inline auto PluginFactory::qt_create_metaobjectdata<qt_meta_tag_ZN13PluginFactoryE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PluginFactory"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PluginFactory, qt_meta_tag_ZN13PluginFactoryE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PluginFactory::staticMetaObject = { {
    QMetaObject::SuperData::link<LiteApi::PluginFactoryT<LiteIdeAfPlugin>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13PluginFactoryE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13PluginFactoryE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13PluginFactoryE_t>.metaTypes,
    nullptr
} };

void PluginFactory::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PluginFactory *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *PluginFactory::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PluginFactory::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13PluginFactoryE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "LiteApi.IPluginFactory.X37"))
        return static_cast< LiteApi::IPluginFactory*>(this);
    return LiteApi::PluginFactoryT<LiteIdeAfPlugin>::qt_metacast(_clname);
}

int PluginFactory::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = LiteApi::PluginFactoryT<LiteIdeAfPlugin>::qt_metacall(_c, _id, _a);
    return _id;
}

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_PluginFactory[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x18,  'l',  'i',  't',  'e',  'i', 
    'd',  'e',  'x',  '.',  'L',  'i',  't',  'e', 
    'I',  'd',  'e',  'A',  'f',  'P',  'l',  'u', 
    'g',  'i',  'n', 
    // "className"
    0x03,  0x6d,  'P',  'l',  'u',  'g',  'i',  'n', 
    'F',  'a',  'c',  't',  'o',  'r',  'y', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(PluginFactory, PluginFactory, qt_pluginMetaDataV2_PluginFactory)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_PluginFactory[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x18,  'l',  'i',  't',  'e',  'i', 
    'd',  'e',  'x',  '.',  'L',  'i',  't',  'e', 
    'I',  'd',  'e',  'A',  'f',  'P',  'l',  'u', 
    'g',  'i',  'n', 
    // "className"
    0x03,  0x6d,  'P',  'l',  'u',  'g',  'i',  'n', 
    'F',  'a',  'c',  't',  'o',  'r',  'y', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(PluginFactory, PluginFactory)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
