#include "KnxprodCatalog.h"
#include "ZipUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QXmlStreamReader>

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

// Normalise DPT strings from different manufacturer conventions to our "M.SSS" format.
// "DPST-1-1" → "1.001", "DPT-5" → "DPT-5" (kept), "1.001" → "1.001" (kept).
static QString normalizeDpt(const QString &raw)
{
    // DPST-<main>-<sub>  →  <main>.<sub padded to 3 digits>
    if (raw.startsWith(QLatin1String("DPST-"))) {
        const QStringList parts = raw.mid(5).split(QLatin1Char('-'));
        if (parts.size() >= 2) {
            bool ok1 = false, ok2 = false;
            const int main = parts[0].toInt(&ok1);
            const int sub  = parts[1].toInt(&ok2);
            if (ok1 && ok2)
                return QString::number(main) + QLatin1Char('.') + QString::number(sub).rightJustified(3, QLatin1Char('0'));
        }
    }
    return raw;
}

static std::shared_ptr<KnxApplicationProgram> parseApplicationXml(const QByteArray &xml)
{
    auto prog = std::make_shared<KnxApplicationProgram>();
    QXmlStreamReader rd(xml);

    // Depth counter: incremented for <Static>, <ParameterBlock>, <ChannelIndependentBlock>;
    // parameters and ComObjects are parsed whenever insideParamContext > 0.
    int insideParamContext = 0;
    KnxParameterType currentPT;
    bool inPT = false;
    // Track the largest AbsoluteSegment seen (some manufacturers emit several small ones).
    quint32 bestSegSize = 0;

    while (!rd.atEnd()) {
        const auto token = rd.readNext();
        if (token == QXmlStreamReader::EndElement) {
            const QStringView n = rd.name();
            if (n == QLatin1String("Static") ||
                n == QLatin1String("ParameterBlock") ||
                n == QLatin1String("ChannelIndependentBlock")) {
                if (insideParamContext > 0) --insideParamContext;
            }
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

        } else if (n == QLatin1String("Static") ||
                   n == QLatin1String("ParameterBlock") ||
                   n == QLatin1String("ChannelIndependentBlock")) {
            ++insideParamContext;

        } else if (n == QLatin1String("AbsoluteSegment") && insideParamContext > 0) {
            bool addrOk = false, szOk = false;
            const quint32 addr = rd.attributes().value(QLatin1String("Address")).toString().toUInt(&addrOk, 0);
            const quint32 sz   = rd.attributes().value(QLatin1String("Size")).toString().toUInt(&szOk, 0);
            if (szOk && sz > bestSegSize) {
                bestSegSize = sz;
                if (addrOk) prog->memoryLayout.parameterBase = addr;
                prog->memoryLayout.parameterSize = sz;
            }

        } else if (n == QLatin1String("ParameterType") && insideParamContext > 0) {
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

        } else if (inPT &&
                   (n == QLatin1String("TypeFloat") ||
                    n == QLatin1String("TypeIPAddress") ||
                    n == QLatin1String("TypeDate") ||
                    n == QLatin1String("TypeTime"))) {
            // Unknown type children — safe fallback to 4-byte UInt
            currentPT.kind = KnxParameterType::Kind::UInt;
            currentPT.size = 4;

        } else if (n == QLatin1String("Parameter") && insideParamContext > 0) {
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

        } else if (n == QLatin1String("ComObject") && insideParamContext > 0) {
            KnxComObject co;
            const QString origId = rd.attributes().value(QLatin1String("OriginalId")).toString();
            co.id     = origId.isEmpty() ? rd.attributes().value(QLatin1String("Id")).toString() : origId;
            co.number = rd.attributes().value(QLatin1String("Number")).toInt();
            co.name   = rd.attributes().value(QLatin1String("Name")).toString();
            co.dpt    = normalizeDpt(rd.attributes().value(QLatin1String("DatapointType")).toString());
            // Support both combined "C,W,T" and individual flag attributes (real manufacturer files)
            const QString flagStr = rd.attributes().value(QLatin1String("CommunicationFlag")).toString();
            if (!flagStr.isEmpty()) {
                co.flags = flagStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
            } else {
                const auto enabled = QLatin1String("Enabled");
                if (rd.attributes().value(QLatin1String("CommunicationObjectEnable")).toString() == enabled)
                    co.flags.append(QStringLiteral("C"));
                if (rd.attributes().value(QLatin1String("ReadFlag")).toString() == enabled)
                    co.flags.append(QStringLiteral("R"));
                if (rd.attributes().value(QLatin1String("WriteFlag")).toString() == enabled)
                    co.flags.append(QStringLiteral("W"));
                if (rd.attributes().value(QLatin1String("TransmitFlag")).toString() == enabled)
                    co.flags.append(QStringLiteral("T"));
                if (rd.attributes().value(QLatin1String("UpdateFlag")).toString() == enabled)
                    co.flags.append(QStringLiteral("U"));
            }
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
    const auto entries = ZipUtils::readEntries(path);
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
