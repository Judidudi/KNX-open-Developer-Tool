#include "KnxprodCatalog.h"
#include "ZipUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QDebug>

// ─── Hardware XML parser ──────────────────────────────────────────────────────

struct HwInfo {
    QString productId;
    QString productName;
    QString manufacturer;
    QString appProgramRefId;
};

// Parse a hardware XML file and return all products it describes.
// A single <Hardware> block may contain multiple <Product> entries (variants that share
// one application program). We collect every product and emit one HwInfo per product.
static QList<HwInfo> parseHardwareXml(const QByteArray &xml)
{
    QList<HwInfo> results;
    QString manufacturer;
    QString currentHwName;
    QString currentAppRef;
    QList<QPair<QString, QString>> currentProducts; // (Id, Text) within current <Hardware>

    QXmlStreamReader rd(xml);
    while (!rd.atEnd()) {
        const auto token = rd.readNext();

        if (token == QXmlStreamReader::EndElement) {
            if (rd.name() == QLatin1String("Hardware")) {
                // Commit one HwInfo per product in this hardware block.
                for (const auto &[productId, productText] : currentProducts) {
                    if (productId.isEmpty() || currentAppRef.isEmpty())
                        continue;
                    HwInfo hw;
                    hw.productId       = productId;
                    hw.productName     = productText.isEmpty() ? currentHwName : productText;
                    hw.manufacturer    = manufacturer;
                    hw.appProgramRefId = currentAppRef;
                    results.append(hw);
                }
                currentProducts.clear();
                currentAppRef.clear();
                currentHwName.clear();
            }
            continue;
        }
        if (token != QXmlStreamReader::StartElement)
            continue;

        const QStringView n = rd.name();

        if (n == QLatin1String("Manufacturer")) {
            manufacturer = rd.attributes().value(QLatin1String("RefId")).toString();

        } else if (n == QLatin1String("Hardware")) {
            currentHwName = rd.attributes().value(QLatin1String("Name")).toString();

        } else if (n == QLatin1String("Product")) {
            currentProducts.append({
                rd.attributes().value(QLatin1String("Id")).toString(),
                rd.attributes().value(QLatin1String("Text")).toString()
            });

        } else if (n == QLatin1String("ApplicationProgramRef")) {
            const QString ref = rd.attributes().value(QLatin1String("RefId")).toString();
            if (!ref.isEmpty())
                currentAppRef = ref;

        } else if (n == QLatin1String("Hardware2ProgrammeVersion")) {
            // ETS6 standard format (nested inside Hardware2Program)
            const QString ref = rd.attributes().value(QLatin1String("ApplicationProgramRefId")).toString();
            if (!ref.isEmpty())
                currentAppRef = ref;
        }
    }
    return results;
}

// ─── ApplicationProgram XML parser ───────────────────────────────────────────

// Normalise DPT strings from different manufacturer conventions to our "M.SSS" format.
// "DPST-1-1" → "1.001", "DPT-5" → "DPT-5" (kept), "1.001" → "1.001" (kept).
static QString normalizeDpt(const QString &raw)
{
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

// Parse flag attributes from a ComObject or ComObjectRef element.
// Returns the flags list if any flag attribute is present; returns empty list if none found.
static QStringList parseFlagAttributes(const QXmlStreamAttributes &attrs, bool &hadFlagAttrs)
{
    const QString flagStr = attrs.value(QLatin1String("CommunicationFlag")).toString();
    if (!flagStr.isEmpty()) {
        hadFlagAttrs = true;
        return flagStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }
    const auto enabled = QLatin1String("Enabled");
    QStringList flags;
    hadFlagAttrs = false;
    const auto check = [&](const char *attrName, const char *letter) {
        const auto val = attrs.value(QLatin1String(attrName));
        if (!val.isEmpty()) {
            hadFlagAttrs = true;
            if (val == enabled)
                flags.append(QLatin1String(letter));
        }
    };
    check("CommunicationObjectEnable", "C");
    check("ReadFlag",     "R");
    check("WriteFlag",    "W");
    check("TransmitFlag", "T");
    check("UpdateFlag",   "U");
    return flags;
}

struct ParamRefInfo {
    QString paramId;
    KnxParameter::Access access = KnxParameter::Access::ReadWrite;
    QString name;                // display name override
    QString conditionRefId;
    QVariant conditionValue;
    KnxParameter::ConditionOp conditionOp = KnxParameter::ConditionOp::Equal;
};

static std::shared_ptr<KnxApplicationProgram> parseApplicationXml(const QByteArray &xml)
{
    auto prog = std::make_shared<KnxApplicationProgram>();
    QXmlStreamReader rd(xml);

    int insideParamContext = 0;
    KnxParameterType currentPT;
    bool inPT = false;
    quint32 bestSegSize = 0;

    QMap<QString, ParamRefInfo> paramRefs;    // key: ParameterRef.Id
    QMap<QString, KnxComObject> coBase;       // key: ComObject.Id — base definitions
    QList<KnxComObject> coFinal;              // merged ComObjectRef entries (ETS6 style)

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
            const QString sb = rd.attributes().value(QLatin1String("SizeByte")).toString();
            if (!sb.isEmpty())
                currentPT.size = static_cast<uint8_t>(sb.toUInt());
            else if (currentPT.maxValue <= 1)     currentPT.size = 1;
            else if (currentPT.maxValue <= 255)   currentPT.size = 1;
            else if (currentPT.maxValue <= 65535) currentPT.size = 2;
            else                                  currentPT.size = 4;

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
            while (!rd.atEnd()) {
                rd.readNext();
                if (rd.isEndElement() && rd.name() == QLatin1String("Parameter"))
                    break;
                if (rd.isStartElement() && rd.name() == QLatin1String("Value"))
                    p.defaultValue = rd.readElementText();
            }
            prog->parameters.append(p);

        } else if (n == QLatin1String("ParameterRef") && insideParamContext > 0) {
            const QString refId   = rd.attributes().value(QLatin1String("Id")).toString();
            const QString paramId = rd.attributes().value(QLatin1String("RefId")).toString();
            const QString access  = rd.attributes().value(QLatin1String("Access")).toString();

            ParamRefInfo info;
            info.paramId = paramId;
            info.name    = rd.attributes().value(QLatin1String("Name")).toString();
            if (access == QLatin1String("Hidden"))
                info.access = KnxParameter::Access::Hidden;
            else if (access == QLatin1String("ReadOnly"))
                info.access = KnxParameter::Access::ReadOnly;
            else
                info.access = KnxParameter::Access::ReadWrite;

            while (!rd.atEnd()) {
                rd.readNext();
                if (rd.isEndElement() && rd.name() == QLatin1String("ParameterRef"))
                    break;
                if (!rd.isStartElement())
                    continue;
                if (rd.name() == QLatin1String("ConditionValue")) {
                    info.conditionRefId = rd.attributes().value(QLatin1String("RefId")).toString();
                    info.conditionValue = rd.attributes().value(QLatin1String("Value")).toString();
                    const QString op = rd.attributes().value(QLatin1String("Operator")).toString();
                    info.conditionOp = (op == QLatin1String("notEqual"))
                                       ? KnxParameter::ConditionOp::NotEqual
                                       : KnxParameter::ConditionOp::Equal;
                }
            }

            if (!refId.isEmpty() && !paramId.isEmpty())
                paramRefs.insert(refId, info);

        } else if (n == QLatin1String("ComObject")) {
            // Collect base ComObject definitions. Not guarded by insideParamContext so that
            // <ComObject> elements inside <ComObjectTable> (which may be outside <Static>)
            // are also captured.
            KnxComObject co;
            const QString origId = rd.attributes().value(QLatin1String("OriginalId")).toString();
            co.id     = origId.isEmpty() ? rd.attributes().value(QLatin1String("Id")).toString() : origId;
            co.number = rd.attributes().value(QLatin1String("Number")).toInt();
            co.name   = rd.attributes().value(QLatin1String("Name")).toString();
            co.dpt    = normalizeDpt(rd.attributes().value(QLatin1String("DatapointType")).toString());
            bool hadFlags = false;
            co.flags = parseFlagAttributes(rd.attributes(), hadFlags);
            if (!co.id.isEmpty())
                coBase.insert(co.id, co);

        } else if (n == QLatin1String("ComObjectRef")) {
            // ETS6 style: ComObjectRef provides display name / DPT / flag overrides.
            const QString refId = rd.attributes().value(QLatin1String("RefId")).toString();

            // Start from the base ComObject; if not found, create a shell with refId as id.
            KnxComObject co = coBase.value(refId);
            if (co.id.isEmpty())
                co.id = refId;

            const QString nameOverride = rd.attributes().value(QLatin1String("Name")).toString();
            if (!nameOverride.isEmpty())
                co.name = nameOverride;

            const QString dptOverride = normalizeDpt(rd.attributes().value(QLatin1String("DatapointType")).toString());
            if (!dptOverride.isEmpty())
                co.dpt = dptOverride;

            bool hadFlags = false;
            const QStringList flagOverride = parseFlagAttributes(rd.attributes(), hadFlags);
            if (hadFlags)
                co.flags = flagOverride;

            coFinal.append(co);
        }
    }

    // Apply ParameterRef access, name override, and conditions to parameters.
    QMap<QString, ParamRefInfo> paramIdToRef;
    for (auto it = paramRefs.constBegin(); it != paramRefs.constEnd(); ++it) {
        if (!paramIdToRef.contains(it->paramId))
            paramIdToRef.insert(it->paramId, it.value());
    }
    for (KnxParameter &p : prog->parameters) {
        const auto infoIt = paramIdToRef.constFind(p.id);
        if (infoIt == paramIdToRef.constEnd())
            continue;
        const ParamRefInfo &info = *infoIt;
        p.access = info.access;
        if (!info.name.isEmpty())
            p.name = info.name;
        if (!info.conditionRefId.isEmpty()) {
            const auto condRefIt = paramRefs.constFind(info.conditionRefId);
            if (condRefIt != paramRefs.constEnd())
                p.conditionParamId = condRefIt->paramId;
            p.conditionValue = info.conditionValue;
            p.conditionOp    = info.conditionOp;
        }
    }

    // Finalize ComObjects:
    // - If ComObjectRef entries were found, use them (ETS6 manufacturer format).
    // - Otherwise use the base ComObject list sorted by number (our own format + some manufacturers).
    if (!coFinal.isEmpty()) {
        prog->comObjects = coFinal;
    } else {
        auto baseList = coBase.values();
        std::sort(baseList.begin(), baseList.end(), [](const KnxComObject &a, const KnxComObject &b) {
            return a.number < b.number;
        });
        prog->comObjects = baseList;
    }

    if (!prog->isValid())
        qWarning() << "KnxprodCatalog: application program" << prog->id << "is invalid (no id or name)";

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
    m_lastErrors.clear();

    for (const QString &searchPath : m_paths) {
        const QDir dir(searchPath);
        if (!dir.exists())
            continue;

        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.knxprod")}, QDir::Files);
        for (const QFileInfo &fi : files)
            loadKnxprod(fi.absoluteFilePath());

        qDebug() << "KnxprodCatalog: scanned" << searchPath
                 << "—" << files.size() << "file(s)," << m_products.size() << "product(s) total";
    }
}

int KnxprodCatalog::importFile(const QString &path)
{
    m_lastErrors.clear();
    const int before = m_products.size();
    loadKnxprod(path);
    return m_products.size() - before;
}

bool KnxprodCatalog::loadKnxprod(const QString &path)
{
    const QString tag = QFileInfo(path).fileName();

    const auto entries = ZipUtils::readEntries(path);
    if (entries.isEmpty()) {
        const QString msg = QStringLiteral("[%1]: Datei kann nicht gelesen werden oder ist kein gültiges ZIP/.knxprod.").arg(tag);
        m_lastErrors.append(msg);
        qWarning().noquote() << "KnxprodCatalog:" << msg;
        return false;
    }

    // ETS6 manufacturer files use a variety of layouts:
    //  (a) Split: hardware in *_HP-*.xml, applications in *_A-*.xml
    //  (b) Combined: a single file contains <Hardware> and <ApplicationPrograms>
    //  (c) Different naming conventions per vendor.
    //
    // To be robust, scan EVERY .xml entry for both hardware blocks and
    // application program blocks based on XML content, not filename.
    QList<QByteArray> hwXmls;
    QMap<QString, QByteArray> appXmlsByName;    // key: entry name (for logging)

    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QString &name = it.key();
        if (!name.endsWith(QLatin1String(".xml")))
            continue;
        const QByteArray &xml = it.value();

        // A file is treated as hardware if it contains a <Product Id=...> or
        // <Hardware2ProgrammeVersion> element. Cheap pre-check via byte search.
        const bool looksLikeHardware =
            xml.contains("<Product ") || xml.contains("<Hardware2ProgrammeVersion");
        // Treat as application program source if it has <ApplicationProgram Id=...>.
        const bool looksLikeAppProg = xml.contains("<ApplicationProgram ");

        if (looksLikeHardware)
            hwXmls.append(xml);
        if (looksLikeAppProg)
            appXmlsByName.insert(name, xml);
    }

    if (hwXmls.isEmpty() || appXmlsByName.isEmpty()) {
        const QString msg = QStringLiteral(
            "[%1]: Keine Hardware-XML (*_HP-*.xml) oder Application-XML (*_A-*.xml) gefunden. "
            "Vorhandene Einträge: %2")
            .arg(tag, QStringList(entries.keys()).join(QStringLiteral(", ")));
        m_lastErrors.append(msg);
        qWarning().noquote() << "KnxprodCatalog:" << msg;
        return false;
    }

    // Parse all Application XMLs first, keyed by their ApplicationProgram id.
    QMap<QString, std::shared_ptr<KnxApplicationProgram>> parsedApps;
    for (auto it = appXmlsByName.constBegin(); it != appXmlsByName.constEnd(); ++it) {
        auto prog = parseApplicationXml(it.value());
        if (prog) {
            parsedApps.insert(prog->id, prog);
        } else {
            const QString msg = QStringLiteral("[%1]: Applikations-XML konnte nicht geparst werden: %2")
                                    .arg(tag, it.key());
            m_lastErrors.append(msg);
            qWarning().noquote() << "KnxprodCatalog:" << msg;
        }
    }

    if (parsedApps.isEmpty()) {
        const QString msg = QStringLiteral("[%1]: Keine gültigen Applikationsprogramme im Archiv.").arg(tag);
        m_lastErrors.append(msg);
        qWarning().noquote() << "KnxprodCatalog:" << msg;
        return false;
    }

    // Process each Hardware XML — each may describe multiple products.
    int loaded = 0;
    for (const QByteArray &hwXml : hwXmls) {
        const QList<HwInfo> hwList = parseHardwareXml(hwXml);
        if (hwList.isEmpty()) {
            const QString msg = QStringLiteral("[%1]: Hardware-XML enthält keine Produkte.").arg(tag);
            m_lastErrors.append(msg);
            qWarning().noquote() << "KnxprodCatalog:" << msg;
            continue;
        }
        for (const HwInfo &hw : hwList) {
            if (hw.appProgramRefId.isEmpty()) {
                const QString msg = QStringLiteral("[%1]: Produkt %2 hat keine ApplicationProgramRefId.")
                                        .arg(tag, hw.productId);
                m_lastErrors.append(msg);
                qWarning().noquote() << "KnxprodCatalog:" << msg;
                continue;
            }
            auto appProg = parsedApps.value(hw.appProgramRefId);
            if (!appProg) {
                const QString msg = QStringLiteral(
                    "[%1]: Produkt %2 referenziert Applikation %3, die nicht gefunden wurde. "
                    "Verfügbare Apps: %4")
                    .arg(tag, hw.productId, hw.appProgramRefId,
                         QStringList(parsedApps.keys()).join(QStringLiteral(", ")));
                m_lastErrors.append(msg);
                qWarning().noquote() << "KnxprodCatalog:" << msg;
                continue;
            }

            m_appPrograms.insert(appProg->id, appProg);

            KnxHardwareProduct product;
            product.productId       = hw.productId;
            product.productName     = hw.productName;
            product.manufacturer    = hw.manufacturer;
            product.appProgramRefId = hw.appProgramRefId;
            product.appProgram      = appProg;
            m_products.append(product);
            ++loaded;
        }
    }

    if (loaded == 0) {
        const QString msg = QStringLiteral("[%1]: Es konnten keine Produkte erfolgreich geladen werden.").arg(tag);
        m_lastErrors.append(msg);
        qWarning().noquote() << "KnxprodCatalog:" << msg;
        return false;
    }
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
