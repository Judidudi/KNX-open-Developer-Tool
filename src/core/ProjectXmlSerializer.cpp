#include "ProjectXmlSerializer.h"
#include "Project.h"
#include "TopologyNode.h"
#include "DeviceInstance.h"
#include "GroupAddress.h"

#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QVariant>

// ─── Save ────────────────────────────────────────────────────────────────────

static void writeDevice(QXmlStreamWriter &xml, const DeviceInstance &dev)
{
    xml.writeStartElement(QStringLiteral("Device"));
    xml.writeAttribute(QStringLiteral("id"),          dev.id());
    xml.writeAttribute(QStringLiteral("physAddr"),    dev.physicalAddress());
    xml.writeAttribute(QStringLiteral("catalogRef"),  dev.catalogRef());
    xml.writeAttribute(QStringLiteral("version"),     dev.manifestVersion());

    xml.writeStartElement(QStringLiteral("Parameters"));
    for (const auto &[key, val] : dev.parameters()) {
        xml.writeStartElement(QStringLiteral("Param"));
        xml.writeAttribute(QStringLiteral("id"),    key);
        xml.writeAttribute(QStringLiteral("value"), val.toString());
        xml.writeEndElement();
    }
    xml.writeEndElement(); // Parameters

    xml.writeStartElement(QStringLiteral("ComObjectLinks"));
    for (const ComObjectLink &link : dev.links()) {
        xml.writeStartElement(QStringLiteral("Link"));
        xml.writeAttribute(QStringLiteral("comObjectId"), link.comObjectId);
        xml.writeAttribute(QStringLiteral("ga"),          link.ga.toString());
        xml.writeEndElement();
    }
    xml.writeEndElement(); // ComObjectLinks

    xml.writeEndElement(); // Device
}

static void writeLine(QXmlStreamWriter &xml, const TopologyNode &line)
{
    xml.writeStartElement(QStringLiteral("Line"));
    xml.writeAttribute(QStringLiteral("id"),     QString::number(line.id()));
    xml.writeAttribute(QStringLiteral("name"),   line.name());
    xml.writeAttribute(QStringLiteral("medium"), QStringLiteral("TP"));

    for (int d = 0; d < line.deviceCount(); ++d)
        writeDevice(xml, *const_cast<TopologyNode &>(line).deviceAt(d));

    xml.writeEndElement(); // Line
}

static void writeArea(QXmlStreamWriter &xml, const TopologyNode &area)
{
    xml.writeStartElement(QStringLiteral("Area"));
    xml.writeAttribute(QStringLiteral("id"),   QString::number(area.id()));
    xml.writeAttribute(QStringLiteral("name"), area.name());

    for (int l = 0; l < area.childCount(); ++l)
        writeLine(xml, *area.childAt(l));

    xml.writeEndElement(); // Area
}

bool ProjectXmlSerializer::save(const Project &project, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();

    xml.writeStartElement(QStringLiteral("KodtProject"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));

    xml.writeStartElement(QStringLiteral("ProjectInfo"));
    xml.writeTextElement(QStringLiteral("Name"),    project.name());
    xml.writeTextElement(QStringLiteral("Created"), project.created().toString(Qt::ISODate));
    xml.writeEndElement(); // ProjectInfo

    xml.writeStartElement(QStringLiteral("Topology"));
    for (int a = 0; a < project.areaCount(); ++a)
        writeArea(xml, *const_cast<Project &>(project).areaAt(a));
    xml.writeEndElement(); // Topology

    // Group addresses – write in 3-level structure
    xml.writeStartElement(QStringLiteral("GroupAddresses"));
    QMap<int, QMap<int, QList<GroupAddress>>> sorted;
    for (const GroupAddress &ga : project.groupAddresses())
        sorted[ga.main()][ga.middle()].append(ga);

    for (auto mainIt = sorted.constBegin(); mainIt != sorted.constEnd(); ++mainIt) {
        xml.writeStartElement(QStringLiteral("MainGroup"));
        xml.writeAttribute(QStringLiteral("id"), QString::number(mainIt.key()));
        for (auto midIt = mainIt.value().constBegin(); midIt != mainIt.value().constEnd(); ++midIt) {
            xml.writeStartElement(QStringLiteral("MiddleGroup"));
            xml.writeAttribute(QStringLiteral("id"), QString::number(midIt.key()));
            for (const GroupAddress &ga : midIt.value()) {
                xml.writeStartElement(QStringLiteral("GroupAddress"));
                xml.writeAttribute(QStringLiteral("id"),   QString::number(ga.sub()));
                xml.writeAttribute(QStringLiteral("name"), ga.name());
                xml.writeAttribute(QStringLiteral("dpt"),  ga.dpt());
                xml.writeEndElement();
            }
            xml.writeEndElement(); // MiddleGroup
        }
        xml.writeEndElement(); // MainGroup
    }
    xml.writeEndElement(); // GroupAddresses

    xml.writeEndElement(); // KodtProject
    xml.writeEndDocument();
    return true;
}

// ─── Load ────────────────────────────────────────────────────────────────────

std::unique_ptr<Project> ProjectXmlSerializer::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return nullptr;

    QXmlStreamReader xml(&file);
    auto project = std::make_unique<Project>();

    TopologyNode *currentArea = nullptr;
    TopologyNode *currentLine = nullptr;
    DeviceInstance *currentDevice = nullptr;
    int currentMain = -1;
    int currentMiddle = -1;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement)
            continue;

        const QStringView name = xml.name();

        if (name == QLatin1String("Name") && !currentDevice) {
            project->setName(xml.readElementText());
        } else if (name == QLatin1String("Created")) {
            project->setCreated(QDate::fromString(xml.readElementText(), Qt::ISODate));
        } else if (name == QLatin1String("Area")) {
            auto attrs = xml.attributes();
            auto area = std::make_unique<TopologyNode>(
                TopologyNode::Type::Area,
                attrs.value(QLatin1String("id")).toInt(),
                attrs.value(QLatin1String("name")).toString());
            currentArea = area.get();
            project->addArea(std::move(area));
        } else if (name == QLatin1String("Line") && currentArea) {
            auto attrs = xml.attributes();
            auto line = std::make_unique<TopologyNode>(
                TopologyNode::Type::Line,
                attrs.value(QLatin1String("id")).toInt(),
                attrs.value(QLatin1String("name")).toString());
            currentLine = line.get();
            currentArea->addChild(std::move(line));
        } else if (name == QLatin1String("Device") && currentLine) {
            auto attrs = xml.attributes();
            auto dev = std::make_unique<DeviceInstance>(
                attrs.value(QLatin1String("id")).toString(),
                attrs.value(QLatin1String("catalogRef")).toString(),
                attrs.value(QLatin1String("version")).toString());
            dev->setPhysicalAddress(attrs.value(QLatin1String("physAddr")).toString());
            currentDevice = dev.get();
            currentLine->addDevice(std::move(dev));
        } else if (name == QLatin1String("Param") && currentDevice) {
            auto attrs = xml.attributes();
            currentDevice->parameters()[attrs.value(QLatin1String("id")).toString()]
                = attrs.value(QLatin1String("value")).toString();
        } else if (name == QLatin1String("Link") && currentDevice) {
            auto attrs = xml.attributes();
            ComObjectLink link;
            link.comObjectId = attrs.value(QLatin1String("comObjectId")).toString();
            link.ga = GroupAddress::fromString(attrs.value(QLatin1String("ga")).toString());
            currentDevice->addLink(link);
        } else if (name == QLatin1String("MainGroup")) {
            currentMain   = xml.attributes().value(QLatin1String("id")).toInt();
            currentMiddle = -1;
        } else if (name == QLatin1String("MiddleGroup")) {
            currentMiddle = xml.attributes().value(QLatin1String("id")).toInt();
        } else if (name == QLatin1String("GroupAddress") && currentMain >= 0 && currentMiddle >= 0) {
            auto attrs = xml.attributes();
            GroupAddress ga(
                currentMain, currentMiddle,
                attrs.value(QLatin1String("id")).toInt(),
                attrs.value(QLatin1String("name")).toString(),
                attrs.value(QLatin1String("dpt")).toString());
            project->addGroupAddress(ga);
        }
    }

    if (xml.hasError())
        return nullptr;

    return project;
}
