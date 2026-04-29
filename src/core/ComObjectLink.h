#pragma once

#include "GroupAddress.h"
#include <QString>

// Links a device's communication object to a group address.
struct ComObjectLink
{
    enum class Direction { Send, Receive };

    QString      comObjectId;  // references KnxComObject::id
    GroupAddress ga;           // may be invalid if not yet linked
    Direction    direction = Direction::Send;
};
