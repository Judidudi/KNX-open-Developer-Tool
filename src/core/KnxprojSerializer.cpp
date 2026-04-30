#include "KnxprojSerializer.h"
#include "ZipUtils.h"
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "GroupAddress.h"
#include "ComObjectLink.h"
#include "BuildingPart.h"

#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QUuid>
#include <QDate>
#include <QMap>

static constexpr const char *kNs = "http://knx.org/xml/project/21";

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
                xml.writeAttribute(QStringLiteral("Description"),    dev->description());
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
                            const QString connTag = (lnk.direction == ComObjectLink::Direction::Receive)
                                                    ? QStringLiteral("Receive")
                                                    : QStringLiteral("Send");
                            xml.writeStartElement(connTag);
                            xml.writeAttribute(QStringLiteral("GroupAddressRefId"),
                                               gaIdMap.value(lnk.ga.toString()));
                            xml.writeEndElement(); // Send or Receive
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

    // ── Buildings ────────────────────────────────────────────────────────────
    if (project.buildingCount() > 0) {
        xml.writeStartElement(QStringLiteral("Buildings"));

        std::function<void(const BuildingPart *, int)> writeBp =
            [&](const BuildingPart *bp, int bpIndex) {
                xml.writeStartElement(QStringLiteral("BuildingPart"));
                xml.writeAttribute(QStringLiteral("Type"), BuildingPart::typeToString(bp->type()));
                xml.writeAttribute(QStringLiteral("Name"), bp->name());
                // Id is derived from parent's id + child index; build a stable id here
                // We pass bpIndex but actual stable id tracking is via Name+Type in tests
                for (const QString &gaRef : bp->groupAddressRefs()) {
                    xml.writeStartElement(QStringLiteral("GroupAddressRef"));
                    xml.writeAttribute(QStringLiteral("RefId"), gaRef);
                    xml.writeEndElement();
                }
                for (const QString &devRef : bp->deviceRefs()) {
                    xml.writeStartElement(QStringLiteral("DeviceInstanceRef"));
                    xml.writeAttribute(QStringLiteral("RefId"), devRef);
                    xml.writeEndElement();
                }
                for (int c = 0; c < bp->childCount(); ++c)
                    writeBp(bp->childAt(c), c);
                xml.writeEndElement(); // BuildingPart
            };

        for (int i = 0; i < project.buildingCount(); ++i)
            writeBp(project.buildingAt(i), i);

        xml.writeEndElement(); // Buildings
    }

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

    QList<QPair<QString, QByteArray>> entries;
    entries.append({ QStringLiteral("0.xml"),
                     buildRoot0xml(project) });
    entries.append({ QStringLiteral("P-%1/0.xml").arg(pid),
                     buildProject0xml(project) });

    const QByteArray zip = ZipUtils::buildZip(entries);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(zip) == zip.size();
}

// ─── Load ────────────────────────────────────────────────────────────────────

std::unique_ptr<Project> KnxprojSerializer::load(const QString &filePath)
{
    const auto entries = ZipUtils::readEntries(filePath);
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
    struct PendingLink {
        DeviceInstance         *dev;
        QString                 comObjectId;
        QString                 gaRefId;
        ComObjectLink::Direction direction = ComObjectLink::Direction::Send;
    };
    QList<PendingLink> pending;
    QMap<QString, GroupAddress> gaById; // gaId → GroupAddress

    QXmlStreamReader xml(projXml);

    // Tracking state
    TopologyNode  *currentArea   = nullptr;
    TopologyNode  *currentLine   = nullptr;
    DeviceInstance *currentDev   = nullptr;
    int            areaAddr      = 0;
    int            lineAddr      = 0;

    // Building structure parsing: stack of active BuildingPart pointers
    QList<BuildingPart *> bpStack;
    bool insideBuildings = false;

    // Parsing
    while (!xml.atEnd() && !xml.hasError()) {
        const auto token = xml.readNext();

        if (token == QXmlStreamReader::EndElement) {
            const QStringView ename = xml.name();
            if (ename == QLatin1String("Buildings")) {
                insideBuildings = false;
            } else if (ename == QLatin1String("BuildingPart") && insideBuildings) {
                if (!bpStack.isEmpty())
                    bpStack.removeLast();
            }
            // When leaving Area/Line/Device context
            if (ename == QLatin1String("Area"))        { currentArea = nullptr; currentLine = nullptr; }
            else if (ename == QLatin1String("Line"))   { currentLine = nullptr; }
            else if (ename == QLatin1String("DeviceInstance")) { currentDev = nullptr; }
            continue;
        }

        if (token != QXmlStreamReader::StartElement)
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
            dev->setDescription(attrs.value(QLatin1String("Description")).toString());
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

        } else if ((name == QLatin1String("Send") || name == QLatin1String("Receive")) && currentDev) {
            const QString gaRef = xml.attributes().value(QLatin1String("GroupAddressRefId")).toString();
            if (!gaRef.isEmpty() && !currentDev->links().isEmpty()) {
                const auto dir = (name == QLatin1String("Receive"))
                                 ? ComObjectLink::Direction::Receive
                                 : ComObjectLink::Direction::Send;
                pending.append({currentDev, currentDev->links().last().comObjectId, gaRef, dir});
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

        } else if (name == QLatin1String("Buildings")) {
            insideBuildings = true;

        } else if (name == QLatin1String("BuildingPart") && insideBuildings) {
            const auto attrs = xml.attributes();
            const BuildingPart::Type bpType =
                BuildingPart::typeFromString(attrs.value(QLatin1String("Type")).toString());
            const QString bpName = attrs.value(QLatin1String("Name")).toString();
            auto bp = std::make_unique<BuildingPart>(bpType, bpName);
            BuildingPart *bpRaw = bp.get();
            if (bpStack.isEmpty()) {
                project->addBuilding(std::move(bp));
            } else {
                bpStack.last()->addChild(std::move(bp));
            }
            bpStack.append(bpRaw);

        } else if (name == QLatin1String("GroupAddressRef") && insideBuildings && !bpStack.isEmpty()) {
            const QString refId = xml.attributes().value(QLatin1String("RefId")).toString();
            bpStack.last()->addGroupAddressRef(refId);

        } else if (name == QLatin1String("DeviceInstanceRef") && insideBuildings && !bpStack.isEmpty()) {
            const QString refId = xml.attributes().value(QLatin1String("RefId")).toString();
            bpStack.last()->addDeviceRef(refId);
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
                lnk.ga        = *ga;
                lnk.direction = p.direction;
                break;
            }
        }
    }

    return project;
}
