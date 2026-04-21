#pragma once

#include <QString>
#include <cstdint>

// 3-level KNX group address: main (5 bit) / middle (3 bit) / sub (8 bit)
// Wire format: 16-bit value = (main<<11) | (middle<<8) | sub
class GroupAddress
{
public:
    GroupAddress() = default;
    GroupAddress(int main, int middle, int sub,
                 const QString &name = {}, const QString &dpt = {});

    // Parse "0/0/1" string
    static GroupAddress fromString(const QString &s);
    QString toString() const;

    uint16_t toRaw() const;
    static GroupAddress fromRaw(uint16_t raw);

    int     main()   const { return m_main;   }
    int     middle() const { return m_middle; }
    int     sub()    const { return m_sub;    }
    QString name()   const { return m_name;   }
    QString dpt()    const { return m_dpt;    }
    int     id()     const { return m_id;     }

    void setName(const QString &name) { m_name = name; }
    void setDpt(const QString &dpt)   { m_dpt = dpt;   }
    void setId(int id)                { m_id = id;     }

    bool isValid() const;
    bool operator==(const GroupAddress &o) const;

private:
    int     m_main   = -1;  // -1 = invalid/default-constructed
    int     m_middle = 0;
    int     m_sub    = 0;
    QString m_name;
    QString m_dpt;
    int     m_id = 0;
};
