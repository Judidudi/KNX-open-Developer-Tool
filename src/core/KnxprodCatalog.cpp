#include "KnxprodCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QMap>

// ─── Minimal ZIP reader (STORE only) ─────────────────────────────────────────
// Duplicated from KnxprojSerializer; will be extracted in Phase D.

static quint16 zru16(const QByteArray &d, int i)
{
    return static_cast<quint16>(
        static_cast<unsigned char>(d[i]) |
        (static_cast<unsigned char>(d[i + 1]) << 8));
}

static quint32 zru32(const QByteArray &d, int i)
{
    return static_cast<quint32>(
        static_cast<unsigned char>(d[i]) |
        (static_cast<unsigned char>(d[i + 1]) << 8) |
        (static_cast<unsigned char>(d[i + 2]) << 16) |
        (static_cast<unsigned char>(d[i + 3]) << 24));
}

static QMap<QString, QByteArray> zipEntries(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray all = f.readAll();
    f.close();

    if (all.size() < 22)
        return {};

    int eocd = -1;
    for (int i = all.size() - 22; i >= 0; --i) {
        if (zru32(all, i) == 0x06054b50u) { eocd = i; break; }
    }
    if (eocd < 0)
        return {};

    const quint32 cdOff   = zru32(all, eocd + 16);
    const quint16 cdCount = zru16(all, eocd + 10);
    int pos = static_cast<int>(cdOff);

    QMap<QString, QByteArray> entries;
    for (int i = 0; i < cdCount; ++i) {
        if (pos + 46 > all.size() || zru32(all, pos) != 0x02014b50u)
            break;
        const quint16 comp       = zru16(all, pos + 10);
        const quint32 compSize   = zru32(all, pos + 20);
        const quint16 nameLen    = zru16(all, pos + 28);
        const quint16 extraLen   = zru16(all, pos + 30);
        const quint16 commentLen = zru16(all, pos + 32);
        const quint32 localOff   = zru32(all, pos + 42);
        const QString name       = QString::fromUtf8(all.mid(pos + 46, nameLen));
        pos += 46 + nameLen + extraLen + commentLen;

        if (comp != 0) continue;

        const int lp = static_cast<int>(localOff);
        if (lp + 30 > all.size() || zru32(all, lp) != 0x04034b50u)
            continue;
        const int dataStart = lp + 30 + zru16(all, lp + 26) + zru16(all, lp + 28);
        if (dataStart + static_cast<int>(compSize) > all.size())
            continue;
        entries[name] = all.mid(dataStart, static_cast<int>(compSize));
    }
    return entries;
}

// ─── Hardware XML parser ──────────────────────────────────────────────────────

struct HwInfo {
    QString productId;
    QString productName;
    QString manufacturer;
    QString appProgramRefId;
};

static HwInfo parseHardwareXml(const QByteArray &xml)
{
    HwInfo info;
    QXmlStreamReader rd(xml);

    while (!rd.atEnd()) {
        if (rd.readNext() != QXmlStreamReader::StartElement)
            continue;
        const QStringView n = rd.name();

        if (n == QLatin1String("Manufacturer")) {
            info.manufacturer = rd.attributes().value(QLatin1String("RefId")).toString();

        } else if (n == QLatin1String("Hardware")) {
            // The inner <Hardware> (not the wrapper)
            const QString hwId = rd.attributes().value(QLatin1String("Id")).toString();
            if (!hwId.isEmpty() && info.productName.isEmpty())
                info.productName = rd.attributes().value(QLatin1String("Name")).toString();

        } else if (n == QLatin1String("Product")) {
            info.productId   = rd.attributes().value(QLatin1String("Id")).toString();
            if (info.productName.isEmpty())
                info.productName = rd.attributes().value(QLatin1String("Text")).toString();

        } else if (n == QLatin1String("ApplicationProgramRef")) {
            info.appProgramRefId = rd.attributes().value(QLatin1String("RefId")).toString();
        }
    }
    return info;
}

// ─── ApplicationProgram XML parser ───────────────────────────────────────────

static std::shared_ptr<KnxApplicationProgram> parseApplicationXml(const QByteArray &xml)
{
    auto prog = std::make_shared<KnxApplicationProgram>();
    QXmlStreamReader rd(xml);

    // Tracking state
    bool insideStatic = false;
    KnxParameterType currentPT;
    bool inPT = false;

    while (!rd.atEnd()) {
        const auto token = rd.readNext();
        if (token == QXmlStreamReader::EndElement) {
            const QStringView n = rd.name();
            if (n == QLatin1String("Static")) insideStatic = false;
            if (n == QLatin1String("ParameterType") && inPT) {
                prog->paramTypes.insert(currentPT.id, currentPT);
                inPT = false;
            }
            continue;
        }
        if (token != QXmlStreamReader::StartElement)
            continue;

        const QStringView n = rd.name();

        if (n == QLatin1String("ApplicationProgram")) {
            prog->id   = rd.attributes().value(QLatin1String("Id")).toString();
            prog->name = rd.attributes().value(QLatin1String("Name")).toString();

        } else if (n == QLatin1String("Manufacturer")) {
            prog->manufacturer = rd.attributes().value(QLatin1String("RefId")).toString();

        } else if (n == QLatin1String("Static")) {
            insideStatic = true;

        } else if (n == QLatin1String("AbsoluteSegment") && insideStatic) {
            bool ok = false;
            const quint32 addr = rd.attributes().value(QLatin1String("Address")).toString().toUInt(&ok, 0);
            if (ok) prog->memoryLayout.parameterBase = addr;
            const quint32 sz = rd.attributes().value(QLatin1String("Size")).toString().toUInt(&ok, 0);
            if (ok) prog->memoryLayout.parameterSize = sz;

        } else if (n == QLatin1String("ParameterType") && insideStatic) {
            currentPT = {};
            currentPT.id = rd.attributes().value(QLatin1String("Id")).toString();
            inPT = true;

        } else if (n == QLatin1String("TypeNumber") && inPT) {
            const QString type = rd.attributes().value(QLatin1String("Type")).toString();
            if (type == QLatin1String("unsignedInt"))
                currentPT.kind = KnxParameterType::Kind::UInt;
            currentPT.minValue = rd.attributes().value(QLatin1String("minInclusive")).toInt();
            currentPT.maxValue = rd.attributes().value(QLatin1String("maxInclusive")).toInt();
            // SizeByte is a custom attribute we write; standard readers ignore it
            const QString sb = rd.attributes().value(QLatin1String("SizeByte")).toString();
            if (!sb.isEmpty())
                currentPT.size = static_cast<uint8_t>(sb.toUInt());
            else if (currentPT.maxValue <= 1)       currentPT.size = 1;
            else if (currentPT.maxValue <= 255)     currentPT.size = 1;
            else if (currentPT.maxValue <= 65535)   currentPT.size = 2;
            else                                    currentPT.size = 4;

        } else if (n == QLatin1String("TypeRestriction") && inPT) {
            currentPT.kind = KnxParameterType::Kind::Enum;
            const QString sb = rd.attributes().value(QLatin1String("SizeByte")).toString();
            currentPT.size = sb.isEmpty() ? 1 : static_cast<uint8_t>(sb.toUInt());

        } else if (n == QLatin1String("Enumeration") && inPT) {
            KnxEnumValue ev;
            ev.value = rd.attributes().value(QLatin1String("Value")).toInt();
            ev.text  = rd.attributes().value(QLatin1String("Text")).toString();
            currentPT.enumValues.append(ev);

        } else if (n == QLatin1String("Parameter") && insideStatic) {
            KnxParameter p;
            const QString origId = rd.attributes().value(QLatin1String("OriginalId")).toString();
            p.id     = origId.isEmpty() ? rd.attributes().value(QLatin1String("Id")).toString() : origId;
            p.name   = rd.attributes().value(QLatin1String("Name")).toString();
            p.typeId = rd.attributes().value(QLatin1String("ParameterType")).toString();
            bool ok  = false;
            p.offset = rd.attributes().value(QLatin1String("Offset")).toString().toUInt(&ok);
            // Default value is in the <Value> child text – read it now
            while (!rd.atEnd()) {
                rd.readNext();
                if (rd.isEndElement() && rd.name() == QLatin1String("Parameter"))
                    break;
                if (rd.isStartElement() && rd.name() == QLatin1String("Value"))
                    p.defaultValue = rd.readElementText();
            }
            prog->parameters.append(p);

        } else if (n == QLatin1String("ComObject") && insideStatic) {
            KnxComObject co;
            const QString origId = rd.attributes().value(QLatin1String("OriginalId")).toString();
            co.id     = origId.isEmpty() ? rd.attributes().value(QLatin1String("Id")).toString() : origId;
            co.number = rd.attributes().value(QLatin1String("Number")).toInt();
            co.name   = rd.attributes().value(QLatin1String("Name")).toString();
            co.dpt    = rd.attributes().value(QLatin1String("DatapointType")).toString();
            const QString flagStr = rd.attributes().value(QLatin1String("CommunicationFlag")).toString();
            // Flags are stored as a combined string "CW" or individual; split by comma if present
            if (!flagStr.isEmpty())
                co.flags = flagStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
            prog->comObjects.append(co);
        }
    }

    return prog->isValid() ? prog : nullptr;
}

// ─── KnxprodCatalog ──────────────────────────────────────────────────────────

KnxprodCatalog::KnxprodCatalog() = default;

void KnxprodCatalog::addSearchPath(const QString &path)
{
    if (!m_paths.contains(path))
        m_paths.append(path);
}

void KnxprodCatalog::reload()
{
    m_products.clear();
    m_appPrograms.clear();

    for (const QString &searchPath : m_paths) {
        const QDir dir(searchPath);
        if (!dir.exists())
            continue;

        // Auto-convert .yaml → .knxprod is handled externally (MainWindow::loadCatalog).

        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.knxprod")}, QDir::Files);
        for (const QFileInfo &fi : files)
            loadKnxprod(fi.absoluteFilePath());
    }
}

bool KnxprodCatalog::loadKnxprod(const QString &path)
{
    const auto entries = zipEntries(path);
    if (entries.isEmpty())
        return false;

    // Find Hardware XML: entry name contains "_HP-"
    // Find Application XML: entry name contains "_A-" but not "_AS-", "_AT-" etc.
    QByteArray hwXml;
    QByteArray appXml;

    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QString &name = it.key();
        if (name.endsWith(QLatin1String(".xml"))) {
            if (name.contains(QLatin1String("_HP-")))
                hwXml = it.value();
            else if (name.contains(QLatin1String("_A-")) &&
                     !name.contains(QLatin1String("_AS-")))
                appXml = it.value();
        }
    }

    if (hwXml.isEmpty() || appXml.isEmpty())
        return false;

    const HwInfo hw = parseHardwareXml(hwXml);
    if (hw.productId.isEmpty() || hw.appProgramRefId.isEmpty())
        return false;

    auto appProg = parseApplicationXml(appXml);
    if (!appProg)
        return false;

    m_appPrograms.insert(appProg->id, appProg);

    KnxHardwareProduct product;
    product.productId       = hw.productId;
    product.productName     = hw.productName;
    product.manufacturer    = hw.manufacturer;
    product.appProgramRefId = hw.appProgramRefId;
    product.appProgram      = appProg;
    m_products.append(product);

    return true;
}

const KnxHardwareProduct *KnxprodCatalog::at(int index) const
{
    if (index < 0 || index >= m_products.size())
        return nullptr;
    return &m_products[index];
}

std::shared_ptr<KnxApplicationProgram>
KnxprodCatalog::sharedByProductRef(const QString &productId) const
{
    for (const KnxHardwareProduct &p : m_products)
        if (p.productId == productId) return p.appProgram;
    return nullptr;
}

const KnxHardwareProduct *KnxprodCatalog::findProduct(const QString &productId) const
{
    for (const KnxHardwareProduct &p : m_products)
        if (p.productId == productId) return &p;
    return nullptr;
}
