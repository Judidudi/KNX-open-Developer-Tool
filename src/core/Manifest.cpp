#include "Manifest.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <algorithm>
#include <optional>

namespace {

ManifestLocalizedString parseLocalizedString(const YAML::Node &node)
{
    ManifestLocalizedString s;
    if (node.IsScalar()) {
        s.de = s.en = QString::fromStdString(node.as<std::string>());
    } else if (node.IsMap()) {
        if (node["de"]) s.de = QString::fromStdString(node["de"].as<std::string>());
        if (node["en"]) s.en = QString::fromStdString(node["en"].as<std::string>());
        if (s.en.isEmpty()) s.en = s.de;
        if (s.de.isEmpty()) s.de = s.en;
    }
    return s;
}

// Reads a memory address which may be written as an integer or as a hex
// string like "0x4000".
bool parseAddress(const YAML::Node &node, uint32_t &out)
{
    if (!node)
        return false;
    if (node.IsScalar()) {
        const std::string s = node.as<std::string>();
        bool ok = false;
        const uint32_t v = QString::fromStdString(s).toUInt(&ok, 0);
        if (ok) { out = v; return true; }
    }
    return false;
}

} // namespace

uint32_t ManifestParameter::effectiveSize() const
{
    if (size > 0)
        return size;
    if (type == QLatin1String("uint16")) return 2;
    if (type == QLatin1String("uint32")) return 4;
    return 1;  // bool, uint8, enum
}

std::optional<Manifest> loadManifest(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;

    YAML::Node root;
    try {
        root = YAML::Load(f.readAll().toStdString());
    } catch (const YAML::Exception &) {
        return std::nullopt;
    }

    Manifest m;

    if (root["id"])           m.id           = QString::fromStdString(root["id"].as<std::string>());
    if (root["version"])      m.version      = QString::fromStdString(root["version"].as<std::string>());
    if (root["manufacturer"]) m.manufacturer = QString::fromStdString(root["manufacturer"].as<std::string>());
    if (root["name"])         m.name         = parseLocalizedString(root["name"]);

    if (root["hardware"]) {
        if (root["hardware"]["target"])      m.hardware.target      = QString::fromStdString(root["hardware"]["target"].as<std::string>());
        if (root["hardware"]["transceiver"]) m.hardware.transceiver = QString::fromStdString(root["hardware"]["transceiver"].as<std::string>());
    }

    if (root["channels"] && root["channels"].IsSequence()) {
        for (const auto &ch : root["channels"]) {
            ManifestChannel c;
            if (ch["id"])   c.id   = QString::fromStdString(ch["id"].as<std::string>());
            if (ch["name"]) c.name = parseLocalizedString(ch["name"]);
            m.channels.append(c);
        }
    }

    if (root["comObjects"] && root["comObjects"].IsSequence()) {
        int autoNumber = 0;
        for (const auto &co : root["comObjects"]) {
            ManifestComObject obj;
            if (co["id"])      obj.id      = QString::fromStdString(co["id"].as<std::string>());
            if (co["number"])  obj.number  = co["number"].as<int>();
            else               obj.number  = autoNumber;
            if (co["channel"]) obj.channel = QString::fromStdString(co["channel"].as<std::string>());
            if (co["name"])    obj.name    = parseLocalizedString(co["name"]);
            if (co["dpt"])     obj.dpt     = QString::fromStdString(co["dpt"].as<std::string>());
            if (co["flags"] && co["flags"].IsSequence()) {
                for (const auto &flag : co["flags"])
                    obj.flags.append(QString::fromStdString(flag.as<std::string>()));
            }
            m.comObjects.append(obj);
            ++autoNumber;
        }
    }

    if (root["parameters"] && root["parameters"].IsSequence()) {
        uint32_t autoOffset = 0;
        for (const auto &p : root["parameters"]) {
            ManifestParameter param;
            if (p["id"])   param.id   = QString::fromStdString(p["id"].as<std::string>());
            if (p["name"]) param.name = parseLocalizedString(p["name"]);
            if (p["type"]) param.type = QString::fromStdString(p["type"].as<std::string>());
            if (p["unit"]) param.unit = QString::fromStdString(p["unit"].as<std::string>());
            if (p["default"]) {
                const auto &def = p["default"];
                if (def.IsScalar()) {
                    try {
                        param.defaultValue = def.as<int>();
                    } catch (...) {
                        param.defaultValue = QString::fromStdString(def.as<std::string>());
                    }
                }
            }
            if (p["range"] && p["range"].IsSequence() && p["range"].size() == 2) {
                param.rangeMin = p["range"][0].as<int>();
                param.rangeMax = p["range"][1].as<int>();
            }
            if (p["values"] && p["values"].IsSequence()) {
                for (const auto &v : p["values"]) {
                    ManifestParameterEnumValue ev;
                    if (v["value"]) ev.value = v["value"].as<int>();
                    if (v["label"]) ev.label = parseLocalizedString(v["label"]);
                    param.enumValues.append(ev);
                }
            }
            if (p["visibilityCondition"])
                param.visibilityCondition = QString::fromStdString(p["visibilityCondition"].as<std::string>());
            if (p["size"])         param.size = p["size"].as<uint32_t>();
            if (p["memoryOffset"]) param.memoryOffset = p["memoryOffset"].as<uint32_t>();
            else                   param.memoryOffset = autoOffset;
            autoOffset = param.memoryOffset + param.effectiveSize();
            m.parameters.push_back(param);
        }
    }

    if (root["memoryLayout"]) {
        const auto &ml = root["memoryLayout"];
        uint32_t v = 0;
        if (parseAddress(ml["addressTable"],     v)) m.memoryLayout.addressTable     = v;
        if (parseAddress(ml["associationTable"], v)) m.memoryLayout.associationTable = v;
        if (parseAddress(ml["comObjectTable"],   v)) m.memoryLayout.comObjectTable   = v;
        if (parseAddress(ml["parameterBase"],    v)) m.memoryLayout.parameterBase    = v;
        // Backwards-compat: legacy "baseAddress" maps to parameterBase
        if (parseAddress(ml["baseAddress"],      v)) m.memoryLayout.parameterBase    = v;
        if (ml["parameterSize"])
            m.memoryLayout.parameterSize = ml["parameterSize"].as<uint32_t>();
    }

    // Auto-compute parameterSize if not specified: last offset + size
    if (m.memoryLayout.parameterSize == 0 && !m.parameters.empty()) {
        uint32_t end = 0;
        for (const auto &p : m.parameters)
            end = std::max(end, p.memoryOffset + p.effectiveSize());
        m.memoryLayout.parameterSize = end;
    }

    if (!m.isValid())
        return std::nullopt;

    return m;
}
