#include "KnxdManager.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QFile>
#include <QDebug>

bool KnxdManager::isInstalled()
{
    return !binaryPath().isEmpty();
}

QString KnxdManager::binaryPath()
{
    // QStandardPaths searches PATH (covers /usr/bin, /usr/local/bin, etc.)
    const QString path = QStandardPaths::findExecutable(QStringLiteral("knxd"));
    if (!path.isEmpty()) return path;

    // Common sysadmin locations not always in user PATH
    static const QStringList extra = {
        QStringLiteral("/usr/sbin/knxd"),
        QStringLiteral("/usr/local/sbin/knxd"),
        QStringLiteral("/sbin/knxd"),
    };
    for (const QString &p : extra)
        if (QFile::exists(p)) return p;

    return {};
}

KnxdManager::KnxdManager(QObject *parent) : QObject(parent) {}

KnxdManager::~KnxdManager()
{
    stop();
}

bool KnxdManager::start(const QString &usbDevPath)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "[KnxdMgr] already running";
        return false;
    }

    const QString bin = binaryPath();
    if (bin.isEmpty()) {
        emit errorOccurred(tr("knxd nicht gefunden. Installation: sudo apt install knxd"));
        return false;
    }

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            const QString line =
                QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
            if (!line.isEmpty())
                qDebug().nospace() << "[knxd] " << line;
        });
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            const QString line =
                QString::fromLocal8Bit(m_process->readAllStandardOutput()).trimmed();
            if (!line.isEmpty())
                qDebug().nospace() << "[knxd] " << line;
        });
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &KnxdManager::onProcessFinished);
    }

    // -e  = gateway individual address
    // -E  = pool of 8 tunneling-client individual addresses
    // -T  = enable KNXnet/IP Tunnelling server on UDP port 3671
    // -S  = enable knxd server (EIBd compat)
    const QStringList args = {
        QStringLiteral("-e"), QStringLiteral("1.0.255"),
        QStringLiteral("-E"), QStringLiteral("1.0.1:8"),
        QStringLiteral("-T"),
        QStringLiteral("-S"),
        QStringLiteral("usb:") + usbDevPath,
    };
    qDebug().nospace() << "[KnxdMgr] starting: " << bin
                       << QLatin1Char(' ') << args.join(QLatin1Char(' '));

    m_process->start(bin, args);
    if (!m_process->waitForStarted(3000)) {
        emit errorOccurred(tr("knxd konnte nicht gestartet werden: %1")
                               .arg(m_process->errorString()));
        return false;
    }

    // Declare ready after 2 s — only if still running
    QTimer::singleShot(2000, this, [this]() {
        if (m_process && m_process->state() == QProcess::Running) {
            qDebug() << "[KnxdMgr] ready";
            emit ready();
        }
        // If already exited, onProcessFinished() has already emitted stopped()
    });
    return true;
}

void KnxdManager::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    qDebug() << "[KnxdMgr] stopping";
    m_process->terminate();
    if (!m_process->waitForFinished(3000))
        m_process->kill();
}

bool KnxdManager::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void KnxdManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    qDebug().nospace() << "[KnxdMgr] finished exitCode=" << exitCode
                       << " status=" << static_cast<int>(status);
    if (exitCode != 0 || status == QProcess::CrashExit) {
        const QString stderr =
            m_process ? QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed()
                      : QString{};
        emit errorOccurred(
            tr("knxd beendet (Code %1)%2")
                .arg(exitCode)
                .arg(stderr.isEmpty() ? QString{} : QStringLiteral(": ") + stderr));
    }
    emit stopped();
}
