#include "uploader.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

static const qint64 kCatboxMaxBytes = 200LL * 1000 * 1000;
static const qint64 kLitterboxMaxBytes = 1000LL * 1000 * 1000;
// No bytes moving for this long while sending = stalled connection.
static const int kStallTimeoutMs = 90 * 1000;
// After the last byte is sent the server still has to store the file.
static const int kServerReplyTimeoutMs = 5 * 60 * 1000;

static QUrl endpointFor(int hours)
{
#ifdef LINKDROP_TESTING
    // Test builds only: lets the test suite point uploads at a local server.
    const QByteArray testEndpoint = qgetenv("LINKDROP_TEST_ENDPOINT");
    if (!testEndpoint.isEmpty())
        return QUrl(QString::fromUtf8(testEndpoint));
#endif
    return QUrl(hours == 0
        ? QStringLiteral("https://catbox.moe/user/api.php")
        : QStringLiteral("https://litterbox.catbox.moe/resources/internals/api.php"));
}

Uploader::Uploader(QObject *parent)
    : QObject(parent), m_watchdog(new QTimer(this))
{
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(kStallTimeoutMs);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (!m_reply)
            return;
        m_timedOut = true;
        m_reply->abort();
    });
}

QString Uploader::readableBytes(qint64 size)
{
    if (size < 1024)
        return QString::number(size) + QStringLiteral(" B");
    if (size < 1024 * 1024)
        return QString::number(double(size) / 1024.0, 'f', 1) + QStringLiteral(" KiB");
    if (size < 1024LL * 1024 * 1024)
        return QString::number(double(size) / (1024.0 * 1024), 'f', 1) + QStringLiteral(" MiB");
    return QString::number(double(size) / (1024.0 * 1024 * 1024), 'f', 2) + QStringLiteral(" GiB");
}

// Catbox/Litterbox report problems as a short plain-text body. Keep it short
// and drop HTML error pages, which are not useful on a phone screen.
QString Uploader::describeServerReply(const QByteArray &body)
{
    QString text = QString::fromUtf8(body).trimmed();
    if (text.isEmpty() || text.startsWith(QLatin1Char('<')))
        return QString();
    text.replace(QRegExp(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (text.length() > 160)
        text = text.left(157) + QStringLiteral("...");
    return text;
}

void Uploader::chooseFile(const QString &pathOrUrl)
{
    if (busy())
        return;

    QString path = pathOrUrl;
    if (pathOrUrl.startsWith(QStringLiteral("file:"))) {
        const QUrl url(pathOrUrl);
        if (!url.isLocalFile()) {
            fail(QStringLiteral("The selected file cannot be opened."),
                 QStringLiteral("Cannot use that file"));
            return;
        }
        path = url.toLocalFile();
    }

    const QFileInfo info(path);
    if (!info.isAbsolute() || !info.isFile() || !info.isReadable()) {
        m_path.clear();
        m_selectedFileName.clear();
        m_selectedFileSize.clear();
        fail(QStringLiteral("Cannot read that file. Check storage permissions."),
             QStringLiteral("Cannot use that file"));
        return;
    }

    m_path = info.absoluteFilePath();
    m_selectedFileName = info.fileName();
    m_selectedFileSize = readableBytes(info.size());
    m_phase = QStringLiteral("selected");
    m_status = QStringLiteral("Ready to upload");
    m_errorMessage.clear();
    m_shareUrl.clear();
    setProgress(0);
    emit changed();
}

void Uploader::fail(const QString &message, const QString &status)
{
    m_phase = QStringLiteral("error");
    m_status = status.isEmpty() ? QStringLiteral("Upload failed") : status;
    m_errorMessage = message;
    emit changed();
}

void Uploader::touchWatchdog()
{
    if (m_reply)
        m_watchdog->start(m_waitingForServer ? kServerReplyTimeoutMs : kStallTimeoutMs);
}

void Uploader::setProgress(double value)
{
    const double bounded = qBound(0.0, value, 1.0);
    if (!qFuzzyCompare(m_progress + 1.0, bounded + 1.0)) {
        m_progress = bounded;
        emit progressChanged();
    }
}

void Uploader::upload(int hours)
{
    if (busy())
        return;
    if (m_path.isEmpty()) {
        fail(QStringLiteral("Choose a file first."));
        return;
    }
    if (hours != 0 && hours != 1 && hours != 12 && hours != 24 && hours != 72) {
        fail(QStringLiteral("Unsupported expiry setting."));
        return;
    }
    const QFileInfo info(m_path);
    if (!info.isFile() || !info.isReadable()) {
        fail(QStringLiteral("The selected file is no longer readable."));
        return;
    }
    const qint64 limit = hours == 0 ? kCatboxMaxBytes : kLitterboxMaxBytes;
    if (info.size() > limit) {
        fail(hours == 0
             ? QStringLiteral("Catbox accepts files up to 200 MB. Choose a temporary link for files up to 1 GB.")
             : QStringLiteral("Litterbox accepts files up to 1 GB."),
             QStringLiteral("File too large"));
        return;
    }

    QFile *file = new QFile(m_path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        fail(QStringLiteral("Cannot open the selected file."));
        return;
    }

    QHttpMultiPart *body = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto addField = [body](const QByteArray &name, const QByteArray &value) {
        QHttpPart part;
        part.setRawHeader("Content-Disposition", "form-data; name=\"" + name + "\"");
        part.setBody(value);
        body->append(part);
    };
    addField("reqtype", "fileupload");
    if (hours != 0)
        addField("time", QByteArray::number(hours) + "h");

    QHttpPart uploadPart;
    // The name goes into a quoted header value, so strip characters that
    // could break out of it. Send it as UTF-8: QHttpPart's typed header
    // setter would turn every non-Latin-1 character into '?'.
    QString safeName = info.fileName();
    safeName.replace(QLatin1Char('"'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('\\'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('\r'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('\n'), QLatin1Char('_'));
    uploadPart.setRawHeader("Content-Disposition",
                            "form-data; name=\"fileToUpload\"; filename=\"" + safeName.toUtf8() + "\"");
    uploadPart.setRawHeader("Content-Type", "application/octet-stream");
    uploadPart.setBodyDevice(file);
    file->setParent(body);
    body->append(uploadPart);

    QNetworkRequest request(endpointFor(hours));
    // For privacy and predictability, do not forward uploads to arbitrary redirects.
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
    request.setRawHeader("User-Agent", "LinkDrop-Sailfish/" APP_VERSION);

    m_hours = hours;
    m_cancelRequested = false;
    m_timedOut = false;
    m_waitingForServer = false;
    m_shareUrl.clear();
    m_errorMessage.clear();
    m_phase = QStringLiteral("uploading");
    m_status = QStringLiteral("Uploading…");
    setProgress(0);
    emit changed();

    // Qt 5.6 bearer management can get stuck reporting "not accessible"
    // after the phone switches between Wi-Fi and mobile data.
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    m_network.setNetworkAccessible(QNetworkAccessManager::Accessible);
#endif

    QNetworkReply *reply = m_network.post(request, body);
    body->setParent(reply);
    m_reply = reply;
    touchWatchdog();

    connect(reply, &QNetworkReply::uploadProgress, this,
            [this, reply](qint64 sent, qint64 total) {
        if (m_reply != reply)
            return;
        if (total > 0) {
            // Completion is only declared after the server returns a valid URL.
            setProgress(qMin(0.99, double(sent) / double(total)));
            if (sent >= total && !m_waitingForServer) {
                m_waitingForServer = true;
                m_status = QStringLiteral("Waiting for the server…");
                emit changed();
            }
        }
        touchWatchdog();
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64, qint64) {
        if (m_reply == reply)
            touchWatchdog();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_reply == reply)
            finishRequest();
        reply->deleteLater();
    });
}

void Uploader::cancel()
{
    if (!m_reply)
        return;
    m_cancelRequested = true;
    m_reply->abort();
}

void Uploader::finishRequest()
{
    QNetworkReply *reply = m_reply.data();
    if (!reply)
        return;
    m_watchdog->stop();
    m_reply.clear();
    m_waitingForServer = false;

    if (m_cancelRequested) {
        m_phase = QStringLiteral("cancelled");
        m_status = QStringLiteral("Upload cancelled");
        m_errorMessage.clear();
        setProgress(0);
        emit changed();
        return;
    }
    if (m_timedOut) {
        fail(QStringLiteral("The upload stalled. Check your connection and try again."));
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString serverSays = describeServerReply(body);

    if (status != 0 && (status < 200 || status >= 300)) {
        QString message = QStringLiteral("The server rejected the upload (HTTP %1).").arg(status);
        if (!serverSays.isEmpty())
            message += QLatin1Char(' ') + serverSays;
        fail(message);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        fail(reply->errorString());
        return;
    }

    const QString urlText = QString::fromUtf8(body).trimmed();
    const QUrl url(urlText, QUrl::StrictMode);
    const QString expectedHost = m_hours == 0
            ? QStringLiteral("files.catbox.moe")
            : QStringLiteral("litter.catbox.moe");
    if (!url.isValid() ||
            url.scheme() != QStringLiteral("https") ||
            url.host().compare(expectedHost, Qt::CaseInsensitive) != 0 ||
            url.path().isEmpty() || url.path() == QStringLiteral("/") ||
            !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
            urlText.contains(QRegExp(QStringLiteral("\\s")))) {
        QString message = QStringLiteral("The server did not return a valid download link.");
        if (!serverSays.isEmpty())
            message += QStringLiteral(" Server said: ") + serverSays;
        fail(message);
        return;
    }

    m_shareUrl = url.toString(QUrl::FullyEncoded);
    m_phase = QStringLiteral("done");
    m_status = QStringLiteral("Upload complete");
    m_errorMessage.clear();
    setProgress(1);
    emit changed();
}
