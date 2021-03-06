/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qv4argumentsobject_p.h>
#include <qv4alloca_p.h>
#include <qv4scopedvalue_p.h>
#include <qv4string_p.h>
#include <qv4function_p.h>

using namespace QV4;

DEFINE_OBJECT_VTABLE(ArgumentsObject);

void Heap::ArgumentsObject::init(QV4::CallContext *context)
{
    ExecutionEngine *v4 = internalClass->engine;

    Object::init();
    fullyCreated = false;
    this->context.set(v4, context->d());
    Q_ASSERT(vtable() == QV4::ArgumentsObject::staticVTable());

    Scope scope(v4);
    Scoped<QV4::ArgumentsObject> args(scope, this);

    if (context->d()->strictMode) {
        Q_ASSERT(CalleePropertyIndex == args->internalClass()->find(v4->id_callee()));
        Q_ASSERT(CallerPropertyIndex == args->internalClass()->find(v4->id_caller()));
        args->setProperty(CalleePropertyIndex + QV4::Object::GetterOffset, *v4->thrower());
        args->setProperty(CalleePropertyIndex + QV4::Object::SetterOffset, *v4->thrower());
        args->setProperty(CallerPropertyIndex + QV4::Object::GetterOffset, *v4->thrower());
        args->setProperty(CallerPropertyIndex + QV4::Object::SetterOffset, *v4->thrower());

        args->arrayReserve(context->argc());
        args->arrayPut(0, context->args(), context->argc());
        args->d()->fullyCreated = true;
    } else {
        Q_ASSERT(CalleePropertyIndex == args->internalClass()->find(v4->id_callee()));
        args->setProperty(CalleePropertyIndex, context->d()->function);
    }
    Q_ASSERT(LengthPropertyIndex == args->internalClass()->find(v4->id_length()));
    args->setProperty(LengthPropertyIndex, Primitive::fromInt32(context->d()->callData->argc));
}

void ArgumentsObject::fullyCreate()
{
    if (fullyCreated())
        return;

    Scope scope(engine());

    uint argCount = context()->callData->argc;
    uint numAccessors = qMin(context()->formalParameterCount(), argCount);
    ArrayData::realloc(this, Heap::ArrayData::Sparse, argCount, true);
    scope.engine->requireArgumentsAccessors(numAccessors);

    Scoped<MemberData> md(scope, d()->mappedArguments);
    if (numAccessors) {
        d()->mappedArguments.set(scope.engine, md->allocate(scope.engine, numAccessors));
        for (uint i = 0; i < numAccessors; ++i) {
            d()->mappedArguments->values.set(scope.engine, i, context()->callData->args[i]);
            arraySet(i, scope.engine->argumentsAccessors + i, Attr_Accessor);
        }
    }
    arrayPut(numAccessors, context()->callData->args + numAccessors, argCount - numAccessors);
    for (uint i = numAccessors; i < argCount; ++i)
        setArrayAttributes(i, Attr_Data);

    d()->fullyCreated = true;
}

bool ArgumentsObject::defineOwnProperty(ExecutionEngine *engine, uint index, const Property *desc, PropertyAttributes attrs)
{
    fullyCreate();

    Scope scope(engine);
    ScopedProperty map(scope);
    PropertyAttributes mapAttrs;
    uint numAccessors = qMin(context()->formalParameterCount(), static_cast<uint>(context()->callData->argc));
    bool isMapped = false;
    if (arrayData() && index < numAccessors &&
        arrayData()->attributes(index).isAccessor() &&
        arrayData()->get(index) == scope.engine->argumentsAccessors[index].getter()->asReturnedValue())
        isMapped = true;

    if (isMapped) {
        Q_ASSERT(arrayData());
        mapAttrs = arrayData()->attributes(index);
        arrayData()->getProperty(index, map, &mapAttrs);
        setArrayAttributes(index, Attr_Data);
        ArrayData::Index arrayIndex{ arrayData(), arrayData()->mappedIndex(index) };
        arrayIndex.set(scope.engine, d()->mappedArguments->values[index]);
    }

    bool strict = engine->current->strictMode;
    engine->current->strictMode = false;
    bool result = Object::defineOwnProperty2(scope.engine, index, desc, attrs);
    engine->current->strictMode = strict;

    if (isMapped && attrs.isData()) {
        Q_ASSERT(arrayData());
        ScopedFunctionObject setter(scope, map->setter());
        ScopedCallData callData(scope, 1);
        callData->thisObject = this->asReturnedValue();
        callData->args[0] = desc->value;
        setter->call(scope, callData);

        if (attrs.isWritable()) {
            setArrayAttributes(index, mapAttrs);
            arrayData()->setProperty(engine, index, map);
        }
    }

    if (engine->current->strictMode && !result)
        return engine->throwTypeError();
    return result;
}

ReturnedValue ArgumentsObject::getIndexed(const Managed *m, uint index, bool *hasProperty)
{
    const ArgumentsObject *args = static_cast<const ArgumentsObject *>(m);
    if (args->fullyCreated())
        return Object::getIndexed(m, index, hasProperty);

    if (index < static_cast<uint>(args->context()->callData->argc)) {
        if (hasProperty)
            *hasProperty = true;
        return args->context()->callData->args[index].asReturnedValue();
    }
    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

bool ArgumentsObject::putIndexed(Managed *m, uint index, const Value &value)
{
    ArgumentsObject *args = static_cast<ArgumentsObject *>(m);
    if (!args->fullyCreated() && index >= static_cast<uint>(args->context()->callData->argc))
        args->fullyCreate();

    if (args->fullyCreated())
        return Object::putIndexed(m, index, value);

    args->context()->callData->args[index] = value;
    return true;
}

bool ArgumentsObject::deleteIndexedProperty(Managed *m, uint index)
{
    ArgumentsObject *args = static_cast<ArgumentsObject *>(m);
    if (!args->fullyCreated())
        args->fullyCreate();
    return Object::deleteIndexedProperty(m, index);
}

PropertyAttributes ArgumentsObject::queryIndexed(const Managed *m, uint index)
{
    const ArgumentsObject *args = static_cast<const ArgumentsObject *>(m);
    if (args->fullyCreated())
        return Object::queryIndexed(m, index);

    uint numAccessors = qMin((int)args->context()->formalParameterCount(), args->context()->callData->argc);
    uint argCount = args->context()->callData->argc;
    if (index >= argCount)
        return PropertyAttributes();
    if (index >= numAccessors)
        return Attr_Data;
    return Attr_Accessor;
}

DEFINE_OBJECT_VTABLE(ArgumentsGetterFunction);

void ArgumentsGetterFunction::call(const Managed *getter, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = static_cast<const ArgumentsGetterFunction *>(getter)->engine();
    Scoped<ArgumentsGetterFunction> g(scope, static_cast<const ArgumentsGetterFunction *>(getter));
    Scoped<ArgumentsObject> o(scope, callData->thisObject.as<ArgumentsObject>());
    if (!o) {
        scope.result = v4->throwTypeError();
        return;
    }

    Q_ASSERT(g->index() < static_cast<unsigned>(o->context()->callData->argc));
    scope.result = o->context()->callData->args[g->index()];
}

DEFINE_OBJECT_VTABLE(ArgumentsSetterFunction);

void ArgumentsSetterFunction::call(const Managed *setter, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = static_cast<const ArgumentsSetterFunction *>(setter)->engine();
    Scoped<ArgumentsSetterFunction> s(scope, static_cast<const ArgumentsSetterFunction *>(setter));
    Scoped<ArgumentsObject> o(scope, callData->thisObject.as<ArgumentsObject>());
    if (!o) {
        scope.result = v4->throwTypeError();
        return;
    }

    Q_ASSERT(s->index() < static_cast<unsigned>(o->context()->callData->argc));
    o->context()->callData->args[s->index()] = callData->argc ? callData->args[0].asReturnedValue() : Encode::undefined();
    scope.result = Encode::undefined();
}

uint ArgumentsObject::getLength(const Managed *m)
{
    const ArgumentsObject *a = static_cast<const ArgumentsObject *>(m);
    if (a->propertyData(Heap::ArgumentsObject::LengthPropertyIndex)->isInteger())
        return a->propertyData(Heap::ArgumentsObject::LengthPropertyIndex)->integerValue();
    return Primitive::toUInt32(a->propertyData(Heap::ArgumentsObject::LengthPropertyIndex)->doubleValue());
}
