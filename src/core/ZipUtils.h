#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

// Shared ZIP utility backed by miniz.
// Supports STORE (method 0) and DEFLATE (method 8) for both reading and writing.
namespace ZipUtils {

// Read all entries from a ZIP file or in-memory buffer.
// Returns entry name → decompressed data. Skips entries that cannot be decoded.
QMap<QString, QByteArray> readEntries(const QString &path);
QMap<QString, QByteArray> readEntries(const QByteArray &data);

// Build a ZIP archive from a list of (name, data) pairs.
// Entries larger than 32 bytes are compressed with DEFLATE; smaller ones use STORE.
QByteArray buildZip(const QList<QPair<QString, QByteArray>> &entries);

} // namespace ZipUtils
