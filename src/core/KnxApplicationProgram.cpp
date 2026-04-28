#include "KnxApplicationProgram.h"

const KnxParameter *KnxApplicationProgram::findParameter(const QString &id) const
{
    for (const KnxParameter &p : parameters)
        if (p.id == id) return &p;
    return nullptr;
}

const KnxComObject *KnxApplicationProgram::findComObject(const QString &id) const
{
    for (const KnxComObject &co : comObjects)
        if (co.id == id) return &co;
    return nullptr;
}

const KnxParameterType *KnxApplicationProgram::findType(const QString &typeId) const
{
    auto it = paramTypes.constFind(typeId);
    return (it != paramTypes.constEnd()) ? &it.value() : nullptr;
}

uint32_t KnxApplicationProgram::effectiveSize(const KnxParameter &param) const
{
    const KnxParameterType *t = findType(param.typeId);
    if (!t)
        return 1;
    switch (t->kind) {
    case KnxParameterType::Kind::Bool:
    case KnxParameterType::Kind::Enum:
        return t->size > 0 ? t->size : 1;
    case KnxParameterType::Kind::UInt:
        return t->size > 0 ? t->size : 1;
    }
    return 1;
}
