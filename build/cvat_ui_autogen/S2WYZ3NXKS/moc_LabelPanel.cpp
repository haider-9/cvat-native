/****************************************************************************
** Meta object code from reading C++ file 'LabelPanel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/cvatnative/ui/LabelPanel.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LabelPanel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
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
struct qt_meta_tag_ZN4cvat10LabelPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto cvat::LabelPanel::qt_create_metaobjectdata<qt_meta_tag_ZN4cvat10LabelPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "cvat::LabelPanel",
        "labelActivated",
        "",
        "labelId",
        "attributesChanged",
        "QList<AttributeValue>",
        "attributes",
        "occludedChanged",
        "occluded",
        "deleteRequested"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'labelActivated'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'attributesChanged'
        QtMocHelpers::SignalData<void(const QVector<AttributeValue> &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'occludedChanged'
        QtMocHelpers::SignalData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Signal 'deleteRequested'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LabelPanel, qt_meta_tag_ZN4cvat10LabelPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject cvat::LabelPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4cvat10LabelPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4cvat10LabelPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN4cvat10LabelPanelE_t>.metaTypes,
    nullptr
} };

void cvat::LabelPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LabelPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->labelActivated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->attributesChanged((*reinterpret_cast<std::add_pointer_t<QList<AttributeValue>>>(_a[1]))); break;
        case 2: _t->occludedChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->deleteRequested(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LabelPanel::*)(int )>(_a, &LabelPanel::labelActivated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LabelPanel::*)(const QVector<AttributeValue> & )>(_a, &LabelPanel::attributesChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LabelPanel::*)(bool )>(_a, &LabelPanel::occludedChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LabelPanel::*)()>(_a, &LabelPanel::deleteRequested, 3))
            return;
    }
}

const QMetaObject *cvat::LabelPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *cvat::LabelPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4cvat10LabelPanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int cvat::LabelPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void cvat::LabelPanel::labelActivated(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void cvat::LabelPanel::attributesChanged(const QVector<AttributeValue> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void cvat::LabelPanel::occludedChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void cvat::LabelPanel::deleteRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
