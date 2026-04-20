#pragma once

#include "GroupAddress.h"
#include <QString>

// Links a device's communication object to a group address.
struct ComObjectLink
{
    QString      comObjectId;  // references Manifest::ComObject::id
    GroupAddress ga;           // may be invalid if not yet linked
};
