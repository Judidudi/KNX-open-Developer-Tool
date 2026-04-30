#include "ZipUtils.h"

#include <QFile>
#include <miniz.h>

namespace ZipUtils {

static QMap<QString, QByteArray> readEntriesFromMemory(const void *buf, size_t size)
{
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, buf, size, 0))
        return {};

    QMap<QString, QByteArray> result;
    const int n = static_cast<int>(mz_zip_reader_get_num_files(&zip));
    for (int i = 0; i < n; ++i) {
        if (mz_zip_reader_is_file_a_directory(&zip, i))
            continue;
        char name[512] = {};
        mz_zip_reader_get_filename(&zip, i, name, sizeof(name));
        size_t outSize = 0;
        void *outBuf = mz_zip_reader_extract_to_heap(&zip, i, &outSize, 0);
        if (outBuf) {
            result[QString::fromUtf8(name)] = QByteArray(static_cast<const char *>(outBuf),
                                                         static_cast<int>(outSize));
            mz_free(outBuf);
        }
    }
    mz_zip_reader_end(&zip);
    return result;
}

QMap<QString, QByteArray> readEntries(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = f.readAll();
    return readEntriesFromMemory(data.constData(), static_cast<size_t>(data.size()));
}

QMap<QString, QByteArray> readEntries(const QByteArray &data)
{
    return readEntriesFromMemory(data.constData(), static_cast<size_t>(data.size()));
}

QByteArray buildZip(const QList<QPair<QString, QByteArray>> &entries)
{
    mz_zip_archive zip = {};
    if (!mz_zip_writer_init_heap(&zip, 0, 0))
        return {};

    for (const auto &[name, data] : entries) {
        const int level = (data.size() > 32) ? MZ_DEFAULT_LEVEL : MZ_NO_COMPRESSION;
        if (!mz_zip_writer_add_mem(&zip, name.toUtf8().constData(),
                                   data.constData(), static_cast<size_t>(data.size()),
                                   static_cast<mz_uint>(level))) {
            mz_zip_writer_end(&zip);
            return {};
        }
    }

    void *pBuf   = nullptr;
    size_t bufSz = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &pBuf, &bufSz)) {
        mz_zip_writer_end(&zip);
        return {};
    }

    QByteArray result(static_cast<const char *>(pBuf), static_cast<int>(bufSz));
    mz_free(pBuf);
    mz_zip_writer_end(&zip);
    return result;
}

} // namespace ZipUtils
