#include "KnxprojSerializer.h"
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "GroupAddress.h"
#include "ComObjectLink.h"

#include <QBuffer>
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QUuid>
#include <QDate>
#include <QMap>

static constexpr const char *kNs = "http://knx.org/xml/project/21";

// ─── CRC-32 (ZIP standard polynomial) ────────────────────────────────────────

static quint32 crc32Compute(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char c : data) {
        crc ^= c;
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ─── Minimal ZIP writer (STORE / no compression) ─────────────────────────────

struct ZipEntry {
    QByteArray name;
    QByteArray data;
    quint32    crc    = 0;
    quint32    offset = 0;
};

static void u16(QByteArray &b, quint16 v)
{
    b += static_cast<char>(v & 0xFF);
    b += static_cast<char>((v >> 8) & 0xFF);
}

static void u32(QByteArray &b, quint32 v)
{
    b += static_cast<char>(v & 0xFF);
    b += static_cast<char>((v >> 8) & 0xFF);
    b += static_cast<char>((v >> 16) & 0xFF);
    b += static_cast<char>((v >> 24) & 0xFF);
}

static QByteArray buildZip(QList<ZipEntry> &files)
{
    QByteArray out;

    for (ZipEntry &f : files) {
        f.offset = static_cast<quint32>(out.size());
        f.crc    = crc32Compute(f.data);
        const auto sz = static_cast<quint32>(f.data.size());
        const auto nl = static_cast<quint16>(f.name.size());

        u32(out, 0x04034b50u);  // local file header sig
        u16(out, 20);            // version needed
        u16(out, 0);             // flags
        u16(out, 0);             // compression: STORE
        u16(out, 0);             // mod time
        u16(out, 0);             // mod date
        u32(out, f.crc);
        u32(out, sz);            // compressed size
        u32(out, sz);            // uncompressed size
        u16(out, nl);            // name length
        u16(out, 0);             // extra length
        out += f.name;
        out += f.data;
    }

    const quint32 cdOffset = static_cast<quint32>(out.size());
    quint32 cdSize = 0;

    for (const ZipEntry &f : files) {
        const int before = out.size();
        const auto sz = static_cast<quint32>(f.data.size());
        const auto nl = static_cast<quint16>(f.name.size());

        u32(out, 0x02014b50u);  // central directory sig
        u16(out, 0);             // version made by
        u16(out, 20);            // version needed
        u16(out, 0);             // flags
        u16(out, 0);             // compression
        u16(out, 0);             // mod time
        u16(out, 0);             // mod date
        u32(out, f.crc);
        u32(out, sz);            // compressed size
        u32(out, sz);            // uncompressed size
        u16(out, nl);            // name length
        u16(out, 0);             // extra length
        u16(out, 0);             // comment length
        u16(out, 0);             // disk start
        u16(out, 0);             // internal attrs
        u32(out, 0);             // external attrs
        u32(out, f.offset);      // local header offset
        out += f.name;
        cdSize += static_cast<quint32>(out.size() - before);
    }

    const auto count = static_cast<quint16>(files.size());
    u32(out, 0x06054b50u);      // EOCD sig
    u16(out, 0);                 // disk number
    u16(out, 0);                 // CD start disk
    u16(out, count);             // entries this disk
    u16(out, count);             // total entries
    u32(out, cdSize);            // CD size
    u32(out, cdOffset);          // CD offset
    u16(out, 0);                 // comment length

    return out;
}

// ─── Minimal ZIP reader (STORE only) ─────────────────────────────────────────

static quint16 ru16(const QByteArray &d, int i)
{
    return static_cast<quint16>(
        static_cast<unsigned char>(d[i]) |
        (static_cast<unsigned char>(d[i + 1]) << 8));
}

static quint32 ru32(const QByteArray &d, int i)
{
    return static_cast<quint32>(
        static_cast<unsigned char>(d[i]) |
        (static_cast<unsigned char>(d[i + 1]) << 8) |
        (static_cast<unsigned char>(d[i + 2]) << 16) |
        (static_cast<unsigned char>(d[i + 3]) << 24));
}

// Returns a map: entry name → raw bytes (only STORE entries).
static QMap<QString, QByteArray> readZip(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray all = f.readAll();
    f.close();

    if (all.size() < 22)
        return {};

    // Scan backwards for EOCD signature
    int eocd = -1;
    for (int i = all.size() - 22; i >= 0; --i) {
        if (ru32(all, i) == 0x06054b50u) { eocd = i; break; }
    }
    if (eocd < 0)
        return {};

    const quint32 cdOff   = ru32(all, eocd + 16);
    const quint16 cdCount = ru16(all, eocd + 10);
    int pos = static_cast<int>(cdOff);

    QMap<QString, QByteArray> entries;
    for (int i = 0; i < cdCount; ++i) {
        if (pos + 46 > all.size() || ru32(all, pos) != 0x02014b50u)
            break;

        const quint16 comp       = ru16(all, pos + 10);
        const quint32 compSize   = ru32(all, pos + 20);
        const quint16 nameLen    = ru16(all, pos + 28);
        const quint16 extraLen   = ru16(all, pos + 30);
        const quint16 commentLen = ru16(all, pos + 32);
        const quint32 localOff   = ru32(all, pos + 42);
        const QString name       = QString::fromUtf8(all.mid(pos + 46, nameLen));
        pos += 46 + nameLen + extraLen + commentLen;

        if (comp != 0)  // skip compressed entries
            continue;

        const int lp = static_cast<int>(localOff);
        if (lp + 30 > all.size() || ru32(all, lp) != 0x04034b50u)
            continue;
        const int dataStart = lp + 30 + ru16(all, lp + 26) + ru16(all, lp + 28);
        if (dataStart + static_cast<int>(compSize) > all.size())
            continue;

        entries[name] = all.mid(dataStart, static_cast<int>(compSize));
    }
    return entries;
}

// ─── ID helpers ──────────────────────────────────────────────────────────────

static QString newProjectId()
{
    // Take the first 8 uppercase hex chars from a UUID (no dashes)
    const QString uuid = QUuid::createUuid().toString(QUuid::Id128);
    return uuid.left(8).toUpper();
}

// ─── XML: build root 0.xml ───────────────────────────────────────────────────

static QByteArray buildRoot0xml(const Project &project)
{
    QByteArray buf;
    QXmlStreamWriter xml(&buf);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QLatin1String(kNs), QStringLiteral("KNX"));
    xml.writeDefaultNamespace(QLatin1String(kNs));
    xml.writeAttribute(QStringLiteral("CreatedBy"),   QStringLiteral("KNX open Developer Tool"));
    xml.writeAttribute(QStringLiteral("ToolVersion"), QStringLiteral("0.1.0"));

    xml.writeStartElement(QStringLiteral("Project"));
    xml.writeAttribute(QStringLiteral("Id"), QStringLiteral("P-") + project.knxprojId());

    xml.writeStartElement(QStringLiteral("ProjectInformation"));
    xml.writeAttribute(QStringLiteral("Name"),         project.name());
    xml.writeAttribute(QStringLiteral("LastModified"), QDate::currentDate().toString(Qt::ISODate));
    xml.writeEndElement(); // ProjectInformation

    xml.writeEndElement(); // Project
    xml.writeEndElement(); // KNX
    xml.writeEndDocument();
    return buf;
}

// ─── XML: build P-XXXX/0.xml ────────────────────────────────────────────────

static QByteArray buildProject0xml(const Project &project)
{
    const QString pid = QStringLiteral("P-") + project.knxprojId();

    // Build GA id map: ga.toString() → element id
    QMap<QString, QString> gaIdMap;
    for (const GroupAddress &ga : project.groupAddresses()) {
        const int addr = ga.main() * 2048 + ga.middle() * 256 + ga.sub();
        gaIdMap[ga.toString()] = QStringLiteral("%1_GA-%2").arg(pid).arg(addr);
    }

    QByteArray buf;
    QXmlStreamWriter xml(&buf);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QLatin1String(kNs), QStringLiteral("KNX"));
    xml.writeDefaultNamespace(QLatin1String(kNs));

    xml.writeStartElement(QStringLiteral("Project"));
    xml.writeAttribute(QStringLiteral("Id"), pid);

    xml.writeStartElement(QStringLiteral("Installations"));
    xml.writeStartElement(QStringLiteral("Installation"));
    xml.writeAttribute(QStringLiteral("Name"), QStringLiteral("Default"));

    // ── Topology ─────────────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("Topology"));
    for (int a = 0; a < project.areaCount(); ++a) {
        TopologyNode *area = const_cast<Project &>(project).areaAt(a);
        xml.writeStartElement(QStringLiteral("Area"));
        xml.writeAttribute(QStringLiteral("Id"),      QStringLiteral("%1_A-%2").arg(pid).arg(area->id()));
        xml.writeAttribute(QStringLiteral("Address"), QString::number(area->id()));
        xml.writeAttribute(QStringLiteral("Name"),    area->name());

        for (int l = 0; l < area->childCount(); ++l) {
            TopologyNode *line = area->childAt(l);
            xml.writeStartElement(QStringLiteral("Line"));
            xml.writeAttribute(QStringLiteral("Id"),             QStringLiteral("%1_L-%2").arg(pid).arg(line->id()));
            xml.writeAttribute(QStringLiteral("Address"),        QString::number(line->id()));
            xml.writeAttribute(QStringLiteral("Name"),           line->name());
            xml.writeAttribute(QStringLiteral("MediumTypeRefId"),QStringLiteral("TP"));

            for (int d = 0; d < line->deviceCount(); ++d) {
                DeviceInstance *dev = line->deviceAt(d);

                // Individual address last component (e.g. "1.1.3" → "3")
                const QStringList parts = dev->physicalAddress().split(QLatin1Char('.'));
                const QString devAddr = parts.isEmpty() ? QStringLiteral("0") : parts.last();

                xml.writeStartElement(QStringLiteral("DeviceInstance"));
                xml.writeAttribute(QStringLiteral("Id"),             QStringLiteral("%1_DI-%2").arg(pid, dev->id()));
                xml.writeAttribute(QStringLiteral("Address"),        devAddr);
                xml.writeAttribute(QStringLiteral("Name"),           QString());
                xml.writeAttribute(QStringLiteral("ProductRefId"),   dev->productRefId());
                xml.writeAttribute(QStringLiteral("AppProgramRefId"),dev->appProgramRefId());
                xml.writeAttribute(QStringLiteral("LastModified"),   QDate::currentDate().toString(Qt::ISODate));

                // Parameters
                if (!dev->parameters().empty()) {
                    xml.writeStartElement(QStringLiteral("ParameterInstanceRefs"));
                    for (const auto &[key, val] : dev->parameters()) {
                        xml.writeStartElement(QStringLiteral("ParameterInstanceRef"));
                        xml.writeAttribute(QStringLiteral("RefId"), key);
                        xml.writeAttribute(QStringLiteral("Value"), val.toString());
                        xml.writeEndElement();
                    }
                    xml.writeEndElement(); // ParameterInstanceRefs
                }

                // ComObject links
                const QList<ComObjectLink> &links = dev->links();
                const bool hasLinks = std::any_of(links.begin(), links.end(),
                    [](const ComObjectLink &lnk){ return lnk.ga.isValid(); });
                if (hasLinks || !links.isEmpty()) {
                    xml.writeStartElement(QStringLiteral("ComObjectInstanceRefs"));
                    for (const ComObjectLink &lnk : links) {
                        xml.writeStartElement(QStringLiteral("ComObjectInstanceRef"));
                        xml.writeAttribute(QStringLiteral("RefId"), lnk.comObjectId);
                        if (lnk.ga.isValid()) {
                            xml.writeStartElement(QStringLiteral("Connectors"));
                            xml.writeStartElement(QStringLiteral("Send"));
                            xml.writeAttribute(QStringLiteral("GroupAddressRefId"),
                                               gaIdMap.value(lnk.ga.toString()));
                            xml.writeEndElement(); // Send
                            xml.writeEndElement(); // Connectors
                        }
                        xml.writeEndElement(); // ComObjectInstanceRef
                    }
                    xml.writeEndElement(); // ComObjectInstanceRefs
                }

                xml.writeEndElement(); // DeviceInstance
            }
            xml.writeEndElement(); // Line
        }
        xml.writeEndElement(); // Area
    }
    xml.writeEndElement(); // Topology

    // ── GroupAddresses ───────────────────────────────────────────────────────
    xml.writeStartElement(QStringLiteral("GroupAddresses"));
    xml.writeStartElement(QStringLiteral("GroupRanges"));

    QMap<int, QMap<int, QList<const GroupAddress *>>> sorted;
    for (const GroupAddress &ga : project.groupAddresses())
        sorted[ga.main()][ga.middle()].append(&ga);

    for (auto mIt = sorted.constBegin(); mIt != sorted.constEnd(); ++mIt) {
        const int m = mIt.key();
        xml.writeStartElement(QStringLiteral("GroupRange"));
        xml.writeAttribute(QStringLiteral("Id"),
                           QStringLiteral("%1_GR-%2").arg(pid).arg(m));
        xml.writeAttribute(QStringLiteral("RangeStart"), QString::number(m * 2048));
        xml.writeAttribute(QStringLiteral("RangeEnd"),   QString::number(m * 2048 + 2047));
        xml.writeAttribute(QStringLiteral("Name"),       QString::number(m));

        for (auto midIt = mIt.value().constBegin(); midIt != mIt.value().constEnd(); ++midIt) {
            const int mid = midIt.key();
            xml.writeStartElement(QStringLiteral("GroupRange"));
            xml.writeAttribute(QStringLiteral("Id"),
                               QStringLiteral("%1_GR-%2-%3").arg(pid).arg(m).arg(mid));
            xml.writeAttribute(QStringLiteral("RangeStart"), QString::number(m * 2048 + mid * 256));
            xml.writeAttribute(QStringLiteral("RangeEnd"),   QString::number(m * 2048 + mid * 256 + 255));
            xml.writeAttribute(QStringLiteral("Name"),       QString::number(mid));

            for (const GroupAddress *ga : midIt.value()) {
                const int addr = m * 2048 + mid * 256 + ga->sub();
                xml.writeStartElement(QStringLiteral("GroupAddress"));
                xml.writeAttribute(QStringLiteral("Id"),      QStringLiteral("%1_GA-%2").arg(pid).arg(addr));
                xml.writeAttribute(QStringLiteral("Address"), QString::number(addr));
                xml.writeAttribute(QStringLiteral("Name"),    ga->name());
                xml.writeAttribute(QStringLiteral("DPTs"),    ga->dpt());
                xml.writeEndElement();
            }
            xml.writeEndElement(); // GroupRange (mid)
        }
        xml.writeEndElement(); // GroupRange (main)
    }

    xml.writeEndElement(); // GroupRanges
    xml.writeEndElement(); // GroupAddresses

    xml.writeEndElement(); // Installation
    xml.writeEndElement(); // Installations
    xml.writeEndElement(); // Project
    xml.writeEndElement(); // KNX
    xml.writeEndDocument();
    return buf;
}

// ─── Save ────────────────────────────────────────────────────────────────────

bool KnxprojSerializer::save(Project &project, const QString &filePath)
{
    if (project.knxprojId().isEmpty())
        project.setKnxprojId(newProjectId());

    const QString pid = project.knxprojId();

    QList<ZipEntry> entries;

    ZipEntry root;
    root.name = QByteArrayLiteral("0.xml");
    root.data = buildRoot0xml(project);
    entries.append(root);

    ZipEntry proj;
    proj.name = (QStringLiteral("P-%1/0.xml").arg(pid)).toUtf8();
    proj.data = buildProject0xml(project);
    entries.append(proj);

    const QByteArray zip = buildZip(entries);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(zip) == zip.size();
}

// ─── Load ────────────────────────────────────────────────────────────────────

std::unique_ptr<Project> KnxprojSerializer::load(const QString &filePath)
{
    const auto entries = readZip(filePath);
    if (entries.isEmpty())
        return nullptr;

    // 1. Read project ID from root 0.xml
    const QByteArray rootXml = entries.value(QStringLiteral("0.xml"));
    if (rootXml.isEmpty())
        return nullptr;

    QString projectId;
    QString projName;
    {
        QXmlStreamReader xml(rootXml);
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement)
                continue;
            const QStringView ename = xml.name();
            if (ename == QLatin1String("Project") && projectId.isEmpty()) {
                const QString fullId = xml.attributes().value(QLatin1String("Id")).toString();
                projectId = fullId.startsWith(QLatin1String("P-")) ? fullId.mid(2) : fullId;
            } else if (ename == QLatin1String("ProjectInformation")) {
                projName = xml.attributes().value(QLatin1String("Name")).toString();
                break; // ProjectInformation is the only thing we need here
            }
        }
    }
    if (projectId.isEmpty())
        return nullptr;

    // 2. Read main project XML
    const QByteArray projXml = entries.value(QStringLiteral("P-%1/0.xml").arg(projectId));
    if (projXml.isEmpty())
        return nullptr;

    auto project = std::make_unique<Project>();
    project->setKnxprojId(projectId);

    // We buffer unresolved GA refs so they can be resolved after GA section
    struct PendingLink { DeviceInstance *dev; QString comObjectId; QString gaRefId; };
    QList<PendingLink> pending;
    QMap<QString, GroupAddress> gaById; // gaId → GroupAddress

    QXmlStreamReader xml(projXml);

    // Tracking state
    TopologyNode  *currentArea   = nullptr;
    TopologyNode  *currentLine   = nullptr;
    DeviceInstance *currentDev   = nullptr;
    int            areaAddr      = 0;
    int            lineAddr      = 0;

    // Parsing
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;

        const QStringView name = xml.name();

        if (name == QLatin1String("Area")) {
            const auto attrs = xml.attributes();
            areaAddr = attrs.value(QLatin1String("Address")).toInt();
            auto node = std::make_unique<TopologyNode>(
                TopologyNode::Type::Area,
                areaAddr,
                attrs.value(QLatin1String("Name")).toString());
            currentArea = node.get();
            currentLine = nullptr;
            currentDev  = nullptr;
            project->addArea(std::move(node));

        } else if (name == QLatin1String("Line") && currentArea) {
            const auto attrs = xml.attributes();
            lineAddr = attrs.value(QLatin1String("Address")).toInt();
            auto node = std::make_unique<TopologyNode>(
                TopologyNode::Type::Line,
                lineAddr,
                attrs.value(QLatin1String("Name")).toString());
            currentLine = node.get();
            currentDev  = nullptr;
            currentArea->addChild(std::move(node));

        } else if (name == QLatin1String("DeviceInstance") && currentLine) {
            const auto attrs       = xml.attributes();
            const QString fullId   = attrs.value(QLatin1String("Id")).toString();
            // Extract device id: everything after "_DI-"
            const int diIdx = fullId.lastIndexOf(QLatin1String("_DI-"));
            const QString devId = (diIdx >= 0) ? fullId.mid(diIdx + 4) : fullId;

            const int devAddr = attrs.value(QLatin1String("Address")).toInt();
            const QString physAddr = QStringLiteral("%1.%2.%3").arg(areaAddr).arg(lineAddr).arg(devAddr);

            auto dev = std::make_unique<DeviceInstance>(
                devId,
                attrs.value(QLatin1String("ProductRefId")).toString(),
                attrs.value(QLatin1String("AppProgramRefId")).toString());
            dev->setPhysicalAddress(physAddr);
            currentDev = dev.get();
            currentLine->addDevice(std::move(dev));

        } else if (name == QLatin1String("ParameterInstanceRef") && currentDev) {
            const auto attrs = xml.attributes();
            currentDev->parameters()[attrs.value(QLatin1String("RefId")).toString()]
                = attrs.value(QLatin1String("Value")).toString();

        } else if (name == QLatin1String("ComObjectInstanceRef") && currentDev) {
            const QString coId = xml.attributes().value(QLatin1String("RefId")).toString();
            ComObjectLink lnk;
            lnk.comObjectId = coId;
            // GA ref resolved below; add a placeholder link now
            currentDev->addLink(lnk);

        } else if (name == QLatin1String("Send") && currentDev) {
            const QString gaRef = xml.attributes().value(QLatin1String("GroupAddressRefId")).toString();
            if (!gaRef.isEmpty() && !currentDev->links().isEmpty()) {
                pending.append({currentDev, currentDev->links().last().comObjectId, gaRef});
            }

        } else if (name == QLatin1String("GroupAddress")) {
            const auto attrs = xml.attributes();
            const QString gaId   = attrs.value(QLatin1String("Id")).toString();
            const int     addr   = attrs.value(QLatin1String("Address")).toInt();
            const int     main   = addr / 2048;
            const int     middle = (addr % 2048) / 256;
            const int     sub    = addr % 256;
            GroupAddress ga(main, middle, sub,
                            attrs.value(QLatin1String("Name")).toString(),
                            attrs.value(QLatin1String("DPTs")).toString());
            gaById[gaId] = ga;
            project->addGroupAddress(ga);
        }
    }

    if (xml.hasError())
        return nullptr;

    project->setName(projName);

    // Resolve pending ComObject→GA links
    for (const PendingLink &p : pending) {
        const GroupAddress *ga = gaById.contains(p.gaRefId) ? &gaById[p.gaRefId] : nullptr;
        if (!ga)
            continue;
        // Find the link with matching comObjectId and set its GA
        for (ComObjectLink &lnk : p.dev->links()) {
            if (lnk.comObjectId == p.comObjectId && !lnk.ga.isValid()) {
                lnk.ga = *ga;
                break;
            }
        }
    }

    return project;
}
