#include "YamlToKnxprod.h"
#include "Manifest.h"

#include <QBuffer>
#include <QFile>
#include <QXmlStreamWriter>
#include <QDate>

// ─── CRC-32 + ZIP (duplicated from KnxprojSerializer; extracted in Phase D) ──

static quint32 yCrc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char c : data) {
        crc ^= c;
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}

struct YZipEntry {
    QByteArray name;
    QByteArray data;
    quint32    crc    = 0;
    quint32    offset = 0;
};

static void yu16(QByteArray &b, quint16 v)
{
    b += static_cast<char>(v & 0xFF);
    b += static_cast<char>((v >> 8) & 0xFF);
}
static void yu32(QByteArray &b, quint32 v)
{
    b += static_cast<char>(v & 0xFF);
    b += static_cast<char>((v >> 8) & 0xFF);
    b += static_cast<char>((v >> 16) & 0xFF);
    b += static_cast<char>((v >> 24) & 0xFF);
}

static QByteArray yBuildZip(QList<YZipEntry> &files)
{
    QByteArray out;
    for (YZipEntry &f : files) {
        f.offset = static_cast<quint32>(out.size());
        f.crc    = yCrc32(f.data);
        const auto sz = static_cast<quint32>(f.data.size());
        const auto nl = static_cast<quint16>(f.name.size());
        yu32(out, 0x04034b50u); yu16(out, 20); yu16(out, 0); yu16(out, 0);
        yu16(out, 0); yu16(out, 0);
        yu32(out, f.crc); yu32(out, sz); yu32(out, sz);
        yu16(out, nl); yu16(out, 0);
        out += f.name; out += f.data;
    }
    const quint32 cdOff = static_cast<quint32>(out.size());
    quint32 cdSz = 0;
    for (const YZipEntry &f : files) {
        const int before = out.size();
        const auto sz = static_cast<quint32>(f.data.size());
        const auto nl = static_cast<quint16>(f.name.size());
        yu32(out, 0x02014b50u); yu16(out, 0); yu16(out, 20); yu16(out, 0); yu16(out, 0);
        yu16(out, 0); yu16(out, 0);
        yu32(out, f.crc); yu32(out, sz); yu32(out, sz);
        yu16(out, nl); yu16(out, 0); yu16(out, 0);
        yu16(out, 0); yu16(out, 0); yu32(out, 0); yu32(out, f.offset);
        out += f.name;
        cdSz += static_cast<quint32>(out.size() - before);
    }
    const auto cnt = static_cast<quint16>(files.size());
    yu32(out, 0x06054b50u); yu16(out, 0); yu16(out, 0);
    yu16(out, cnt); yu16(out, cnt);
    yu32(out, cdSz); yu32(out, cdOff); yu16(out, 0);
    return out;
}

// ─── ID helpers ──────────────────────────────────────────────────────────────

QString YamlToKnxprod::hash4(const QString &s)
{
    quint16 h = 0x5A5A;
    for (QChar c : s)
        h = static_cast<quint16>(h * 31u + c.unicode());
    return QString::asprintf("%04X", h);
}

QString YamlToKnxprod::versionHex(const QString &version)
{
    const QStringList parts = version.split(QLatin1Char('.'));
    const int major = parts.isEmpty() ? 1 : parts[0].toInt();
    return QString::asprintf("%04X", major);
}

QString YamlToKnxprod::productRefId(const Manifest &m)
{
    const QString h = hash4(m.id);
    return QStringLiteral("M-00FA_H-%1_HP-%1").arg(h);
}

QString YamlToKnxprod::appProgramRefId(const Manifest &m)
{
    const QString h   = hash4(m.id);
    const QString ver = versionHex(m.version);
    return QStringLiteral("M-00FA_A-%1-%2").arg(h, ver);
}

// ─── Hardware XML ─────────────────────────────────────────────────────────────

QByteArray YamlToKnxprod::buildHardwareXml(const Manifest &m,
                                             const QString &h4,
                                             const QString &ver)
{
    const QString mid   = QStringLiteral("M-00FA");
    const QString hwId  = QStringLiteral("M-00FA_H-%1").arg(h4);
    const QString hpId  = QStringLiteral("M-00FA_H-%1_HP-%1").arg(h4);
    const QString h2pId = QStringLiteral("M-00FA_H-%1_HP-%1-0_A-%1-%2").arg(h4, ver);
    const QString appId = QStringLiteral("M-00FA_A-%1-%2").arg(h4, ver);
    const QString devName = m.name.get(QStringLiteral("en"));

    QByteArray buf;
    QXmlStreamWriter xml(&buf);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("KNX"));
    xml.writeAttribute(QStringLiteral("xmlns"),      QStringLiteral("http://knx.org/xml/product/20"));
    xml.writeAttribute(QStringLiteral("CreatedBy"),  QStringLiteral("KNX open Developer Tool"));
    xml.writeAttribute(QStringLiteral("CreatedAt"),  QDate::currentDate().toString(Qt::ISODate));

    xml.writeStartElement(QStringLiteral("ManufacturerData"));
    xml.writeStartElement(QStringLiteral("Manufacturer"));
    xml.writeAttribute(QStringLiteral("RefId"), mid);

    xml.writeStartElement(QStringLiteral("Hardware"));
    xml.writeStartElement(QStringLiteral("Hardware"));
    xml.writeAttribute(QStringLiteral("Id"),                    hwId);
    xml.writeAttribute(QStringLiteral("Name"),                  devName);
    xml.writeAttribute(QStringLiteral("Version"),               QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("BusCurrent"),            QStringLiteral("10"));
    xml.writeAttribute(QStringLiteral("HasIndividualAddress"),   QStringLiteral("true"));
    xml.writeAttribute(QStringLiteral("HasApplicationProgram"), QStringLiteral("true"));

    xml.writeStartElement(QStringLiteral("Products"));
    xml.writeStartElement(QStringLiteral("Product"));
    xml.writeAttribute(QStringLiteral("Id"),              hpId);
    xml.writeAttribute(QStringLiteral("Text"),            devName);
    xml.writeAttribute(QStringLiteral("OrderNumber"),     m.id);
    xml.writeAttribute(QStringLiteral("DefaultLanguage"), QStringLiteral("de-DE"));
    xml.writeEndElement(); // Product
    xml.writeEndElement(); // Products

    xml.writeStartElement(QStringLiteral("Hardware2Programs"));
    xml.writeStartElement(QStringLiteral("Hardware2Program"));
    xml.writeAttribute(QStringLiteral("Id"),          h2pId);
    xml.writeAttribute(QStringLiteral("MediumTypes"), QStringLiteral("TP"));
    xml.writeStartElement(QStringLiteral("ApplicationProgramRef"));
    xml.writeAttribute(QStringLiteral("RefId"), appId);
    xml.writeEndElement(); // ApplicationProgramRef
    xml.writeEndElement(); // Hardware2Program
    xml.writeEndElement(); // Hardware2Programs

    xml.writeEndElement(); // Hardware (inner)
    xml.writeEndElement(); // Hardware (outer)
    xml.writeEndElement(); // Manufacturer
    xml.writeEndElement(); // ManufacturerData
    xml.writeEndElement(); // KNX
    xml.writeEndDocument();
    return buf;
}

// ─── ApplicationProgram XML ───────────────────────────────────────────────────

QByteArray YamlToKnxprod::buildApplicationXml(const Manifest &m,
                                                const QString &h4,
                                                const QString &ver)
{
    const QString mid   = QStringLiteral("M-00FA");
    const QString appId = QStringLiteral("M-00FA_A-%1-%2").arg(h4, ver);
    const QString segId = QStringLiteral("M-00FA_A-%1_AS-00").arg(h4);
    const QString ptPfx = QStringLiteral("M-00FA_A-%1_PT-").arg(h4);
    const QString pPfx  = QStringLiteral("M-00FA_A-%1_P-").arg(h4);
    const QString oPfx  = QStringLiteral("M-00FA_A-%1_O-").arg(h4);
    const QString devName = m.name.get(QStringLiteral("en"));

    QByteArray buf;
    QXmlStreamWriter xml(&buf);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("KNX"));
    xml.writeAttribute(QStringLiteral("xmlns"),      QStringLiteral("http://knx.org/xml/product/20"));
    xml.writeAttribute(QStringLiteral("CreatedBy"),  QStringLiteral("KNX open Developer Tool"));

    xml.writeStartElement(QStringLiteral("ManufacturerData"));
    xml.writeStartElement(QStringLiteral("Manufacturer"));
    xml.writeAttribute(QStringLiteral("RefId"), mid);

    xml.writeStartElement(QStringLiteral("ApplicationPrograms"));
    xml.writeStartElement(QStringLiteral("ApplicationProgram"));
    xml.writeAttribute(QStringLiteral("Id"),                 appId);
    xml.writeAttribute(QStringLiteral("Name"),               devName);
    xml.writeAttribute(QStringLiteral("ApplicationNumber"),  QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("ApplicationVersion"), QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("ProgramType"),        QStringLiteral("ApplicationProgram"));
    xml.writeAttribute(QStringLiteral("MaskVersion"),        QStringLiteral("MV-0701"));

    xml.writeStartElement(QStringLiteral("Static"));

    // Code segment (parameter memory block)
    xml.writeStartElement(QStringLiteral("Code"));
    xml.writeStartElement(QStringLiteral("AbsoluteSegment"));
    xml.writeAttribute(QStringLiteral("Id"),      segId);
    xml.writeAttribute(QStringLiteral("Address"), QString::asprintf("0x%04X", m.memoryLayout.parameterBase));
    xml.writeAttribute(QStringLiteral("Size"),    QString::number(m.memoryLayout.parameterSize));
    xml.writeEndElement();
    xml.writeEndElement(); // Code

    xml.writeEmptyElement(QStringLiteral("AddressTable"));
    xml.writeAttribute(QStringLiteral("CodeSegment"), segId);
    xml.writeAttribute(QStringLiteral("Offset"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("MaxEntries"), QStringLiteral("255"));

    xml.writeEmptyElement(QStringLiteral("AssociationTable"));
    xml.writeAttribute(QStringLiteral("CodeSegment"), segId);
    xml.writeAttribute(QStringLiteral("Offset"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("MaxEntries"), QStringLiteral("255"));

    xml.writeEmptyElement(QStringLiteral("ComObjectTable"));
    xml.writeAttribute(QStringLiteral("CodeSegment"), segId);
    xml.writeAttribute(QStringLiteral("Offset"), QStringLiteral("0"));

    xml.writeEmptyElement(QStringLiteral("ParameterMemory"));
    xml.writeAttribute(QStringLiteral("CodeSegment"), segId);
    xml.writeAttribute(QStringLiteral("Offset"), QStringLiteral("0"));

    // ── ParameterTypes ────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("ParameterTypes"));
    for (const ManifestParameter &p : m.parameters) {
        xml.writeStartElement(QStringLiteral("ParameterType"));
        xml.writeAttribute(QStringLiteral("Id"), ptPfx + p.id);

        if (p.type == QLatin1String("bool")) {
            xml.writeStartElement(QStringLiteral("TypeNumber"));
            xml.writeAttribute(QStringLiteral("Type"),         QStringLiteral("unsignedInt"));
            xml.writeAttribute(QStringLiteral("minInclusive"), QStringLiteral("0"));
            xml.writeAttribute(QStringLiteral("maxInclusive"), QStringLiteral("1"));
            xml.writeAttribute(QStringLiteral("SizeByte"),     QStringLiteral("1"));
            xml.writeEndElement();
        } else if (p.type == QLatin1String("enum")) {
            xml.writeStartElement(QStringLiteral("TypeRestriction"));
            xml.writeAttribute(QStringLiteral("Base"),     QStringLiteral("Value"));
            xml.writeAttribute(QStringLiteral("SizeByte"), QString::number(p.effectiveSize()));
            for (const ManifestParameterEnumValue &ev : p.enumValues) {
                xml.writeEmptyElement(QStringLiteral("Enumeration"));
                xml.writeAttribute(QStringLiteral("Text"),  ev.label.get(QStringLiteral("en")));
                xml.writeAttribute(QStringLiteral("Value"), QString::number(ev.value));
            }
            xml.writeEndElement();
        } else {
            // uint8, uint16, uint32
            xml.writeStartElement(QStringLiteral("TypeNumber"));
            xml.writeAttribute(QStringLiteral("Type"),         QStringLiteral("unsignedInt"));
            xml.writeAttribute(QStringLiteral("minInclusive"), p.rangeMin.isValid() ? p.rangeMin.toString() : QStringLiteral("0"));
            xml.writeAttribute(QStringLiteral("maxInclusive"), p.rangeMax.isValid() ? p.rangeMax.toString() : QStringLiteral("255"));
            xml.writeAttribute(QStringLiteral("SizeByte"),     QString::number(p.effectiveSize()));
            xml.writeEndElement();
        }
        xml.writeEndElement(); // ParameterType
    }
    xml.writeEndElement(); // ParameterTypes

    // ── Parameters ────────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("Parameters"));
    for (const ManifestParameter &p : m.parameters) {
        xml.writeStartElement(QStringLiteral("Parameter"));
        xml.writeAttribute(QStringLiteral("Id"),            pPfx + p.id);
        xml.writeAttribute(QStringLiteral("OriginalId"),    p.id);
        xml.writeAttribute(QStringLiteral("Name"),          p.name.get(QStringLiteral("en")));
        xml.writeAttribute(QStringLiteral("ParameterType"), ptPfx + p.id);
        xml.writeAttribute(QStringLiteral("Offset"),        QString::number(p.memoryOffset));
        xml.writeTextElement(QStringLiteral("Value"),       p.defaultValue.toString());
        xml.writeEndElement(); // Parameter
    }
    xml.writeEndElement(); // Parameters

    // ── ParameterRefs ─────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("ParameterRefs"));
    for (const ManifestParameter &p : m.parameters) {
        xml.writeEmptyElement(QStringLiteral("ParameterRef"));
        xml.writeAttribute(QStringLiteral("Id"),    pPfx + p.id + QStringLiteral("-R"));
        xml.writeAttribute(QStringLiteral("RefId"), pPfx + p.id);
        xml.writeAttribute(QStringLiteral("Name"),  p.name.get(QStringLiteral("en")));
    }
    xml.writeEndElement(); // ParameterRefs

    // ── ComObjects ────────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("ComObjects"));
    for (const ManifestComObject &co : m.comObjects) {
        xml.writeEmptyElement(QStringLiteral("ComObject"));
        xml.writeAttribute(QStringLiteral("Id"),            oPfx + QString::number(co.number));
        xml.writeAttribute(QStringLiteral("OriginalId"),    co.id);
        xml.writeAttribute(QStringLiteral("Name"),          co.name.get(QStringLiteral("en")));
        xml.writeAttribute(QStringLiteral("Number"),        QString::number(co.number));
        xml.writeAttribute(QStringLiteral("Text"),          co.channel);
        xml.writeAttribute(QStringLiteral("DatapointType"), QStringLiteral("DPT-") + co.dpt.section(QLatin1Char('.'), 0, 0));
        xml.writeAttribute(QStringLiteral("CommunicationFlag"), co.flags.join(QLatin1Char(',')));
    }
    xml.writeEndElement(); // ComObjects

    // ── ComObjectRefs ─────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("ComObjectRefs"));
    for (const ManifestComObject &co : m.comObjects) {
        xml.writeEmptyElement(QStringLiteral("ComObjectRef"));
        xml.writeAttribute(QStringLiteral("Id"),    oPfx + QString::number(co.number) + QStringLiteral("-R"));
        xml.writeAttribute(QStringLiteral("RefId"), oPfx + QString::number(co.number));
    }
    xml.writeEndElement(); // ComObjectRefs

    xml.writeEndElement(); // Static
    xml.writeEndElement(); // ApplicationProgram
    xml.writeEndElement(); // ApplicationPrograms
    xml.writeEndElement(); // Manufacturer
    xml.writeEndElement(); // ManufacturerData
    xml.writeEndElement(); // KNX
    xml.writeEndDocument();
    return buf;
}

// ─── Public API ──────────────────────────────────────────────────────────────

QByteArray YamlToKnxprod::toZip(const Manifest &m)
{
    const QString h4  = hash4(m.id);
    const QString ver = versionHex(m.version);

    const QString mfDir  = QStringLiteral("M-00FA/");
    const QString hwFile = QStringLiteral("M-00FA_H-%1_HP-%1.xml").arg(h4);
    const QString apFile = QStringLiteral("M-00FA_A-%1-%2.xml").arg(h4, ver);

    QList<YZipEntry> entries;

    YZipEntry hw;
    hw.name = (mfDir + hwFile).toUtf8();
    hw.data = buildHardwareXml(m, h4, ver);
    entries.append(hw);

    YZipEntry ap;
    ap.name = (mfDir + apFile).toUtf8();
    ap.data = buildApplicationXml(m, h4, ver);
    entries.append(ap);

    return yBuildZip(entries);
}

bool YamlToKnxprod::writeFile(const Manifest &m, const QString &outputPath)
{
    const QByteArray zip = toZip(m);
    if (zip.isEmpty())
        return false;
    QFile f(outputPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(zip) == zip.size();
}
