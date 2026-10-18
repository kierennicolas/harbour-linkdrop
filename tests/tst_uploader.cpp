// Tests for Uploader (Litterbox only). The offline cases always run. The online cases upload a
// small random file to Litterbox with a 1 hour expiry, download it again and
// compare the bytes; they only run when LINKDROP_ONLINE_TEST=1.

#include <QtTest>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "uploader.h"

// Minimal one-shot HTTP server: records the request, sends a canned reply.
class MockServer : public QObject
{
    Q_OBJECT
public:
    int status = 200;
    QByteArray body;
    QByteArray request;   // raw bytes of the last complete request
    int requests = 0;

    bool start()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &MockServer::onConnection);
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    QByteArray url() const
    {
        return "http://127.0.0.1:" + QByteArray::number(m_server.serverPort()) + "/api.php";
    }
    QByteArray header(const QByteArray &name) const
    {
        const int end = request.indexOf("\r\n\r\n");
        for (const QByteArray &line : request.left(end).split('\n')) {
            const int colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).trimmed().toLower() == name.toLower())
                return line.mid(colon + 1).trimmed();
        }
        return QByteArray();
    }

private slots:
    void onConnection()
    {
        while (QTcpSocket *sock = m_server.nextPendingConnection()) {
            auto buffer = QSharedPointer<QByteArray>::create();
            connect(sock, &QTcpSocket::readyRead, this, [this, sock, buffer]() {
                buffer->append(sock->readAll());
                const int end = buffer->indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                request = *buffer;
                const qint64 length = header("Content-Length").toLongLong();
                if (buffer->size() - (end + 4) < length)
                    return;
                ++requests;
                QByteArray reply = "HTTP/1.1 " + QByteArray::number(status) + " Mock\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                        "Connection: close\r\n\r\n" + body;
                sock->write(reply);
                sock->disconnectFromHost();
            });
            connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        }
    }

private:
    QTcpServer m_server;
};

class TestUploader : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString writeRandomFile(const QString &name, int bytes)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return QString();
        QByteArray data;
        data.reserve(bytes);
        quint32 x = 0x9e3779b9u ^ quint32(bytes);
        for (int i = 0; i < bytes; ++i) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;   // xorshift32
            data.append(char(x & 0xff));
        }
        f.write(data);
        return path;
    }

    static bool online() { return qgetenv("LINKDROP_ONLINE_TEST") == "1"; }

    static bool waitForPhaseNot(Uploader &u, const QString &phase, int timeoutMs)
    {
        QElapsedTimer t;
        t.start();
        while (u.phase() == phase && t.elapsed() < timeoutMs)
            QTest::qWait(100);
        return u.phase() != phase;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void initialState()
    {
        Uploader u;
        QCOMPARE(u.phase(), QStringLiteral("idle"));
        QVERIFY(!u.hasFile());
        QVERIFY(!u.busy());
        QCOMPARE(u.progress(), 0.0);
    }

    void missingFileIsRejected()
    {
        Uploader u;
        QSignalSpy spy(&u, &Uploader::changed);
        u.chooseFile(m_dir.filePath("does-not-exist.bin"));
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QCOMPARE(u.status(), QStringLiteral("Cannot use that file"));
        QVERIFY(!u.hasFile());
        QVERIFY(spy.count() >= 1);
    }

    void relativePathIsRejected()
    {
        Uploader u;
        u.chooseFile(QStringLiteral("relative/file.txt"));
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY(!u.hasFile());
    }

    void directoryIsRejected()
    {
        Uploader u;
        u.chooseFile(m_dir.path());
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY(!u.hasFile());
    }

    void plainPathIsAccepted()
    {
        const QString path = writeRandomFile(QStringLiteral("plain.txt"), 1500);
        Uploader u;
        u.chooseFile(path);
        QCOMPARE(u.phase(), QStringLiteral("selected"));
        QVERIFY(u.hasFile());
        QCOMPARE(u.selectedFileName(), QStringLiteral("plain.txt"));
        QCOMPARE(u.selectedFileSize(), QStringLiteral("1.5 KiB"));
    }

    void fileUrlWithSpacesAndUnicodeIsAccepted()
    {
        const QString name = QString::fromUtf8("Fête d'été ✓ notes.txt");
        const QString path = writeRandomFile(name, 10);
        Uploader u;
        u.chooseFile(QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded));
        QCOMPARE(u.phase(), QStringLiteral("selected"));
        QCOMPARE(u.selectedFileName(), name);
        QCOMPARE(u.selectedFileSize(), QStringLiteral("10 B"));
    }

    void uploadWithoutFileFails()
    {
        Uploader u;
        u.upload(24);
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QCOMPARE(u.errorMessage(), QStringLiteral("Choose a file first."));
    }

    void unsupportedExpiryFails()
    {
        const QString path = writeRandomFile(QStringLiteral("expiry.bin"), 16);
        Uploader u;
        u.chooseFile(path);
        u.upload(5);
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QCOMPARE(u.errorMessage(), QStringLiteral("Unsupported expiry setting."));
        QVERIFY(!u.busy());
    }

    void permanentCatboxOptionIsGone()
    {
        // 0 used to mean a permanent Catbox upload; only Litterbox is supported now.
        const QString path = writeRandomFile(QStringLiteral("keep.bin"), 16);
        Uploader u;
        u.chooseFile(path);
        u.upload(0);
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QCOMPARE(u.errorMessage(), QStringLiteral("Unsupported expiry setting."));
    }

    void fileRemovedAfterSelectionFails()
    {
        const QString path = writeRandomFile(QStringLiteral("vanishing.bin"), 16);
        Uploader u;
        u.chooseFile(path);
        QVERIFY(QFile::remove(path));
        u.upload(1);
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY(!u.busy());
    }

    void cancelWhenIdleIsHarmless()
    {
        Uploader u;
        u.cancel();
        QCOMPARE(u.phase(), QStringLiteral("idle"));
    }

    // ---- against a local mock server ----------------------------------

    void mockLitterboxSuccess()
    {
        MockServer server;
        QVERIFY(server.start());
        server.body = "https://litter.catbox.moe/abc123.bin\n";
        qputenv("LINKDROP_TEST_ENDPOINT", server.url());

        const QString name = QString::fromUtf8("Fête \"quoted\" ✓.bin");
        const QString path = writeRandomFile(name, 70000);
        QFile src(path);
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QByteArray original = src.readAll();

        Uploader u;
        u.chooseFile(path);
        u.upload(12);
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        qunsetenv("LINKDROP_TEST_ENDPOINT");

        QVERIFY2(u.phase() == QStringLiteral("done"), qPrintable(u.errorMessage()));
        QCOMPARE(u.shareUrl(), QStringLiteral("https://litter.catbox.moe/abc123.bin"));
        QCOMPARE(u.progress(), 1.0);

        QCOMPARE(server.requests, 1);
        QVERIFY(server.request.startsWith("POST /api.php HTTP/1.1"));
        QCOMPARE(server.header("User-Agent"), QByteArray("LinkDrop-Sailfish/test"));
        QVERIFY(server.header("Content-Type").startsWith("multipart/form-data; boundary="));
        QVERIFY(server.request.contains("name=\"reqtype\"\r\n\r\nfileupload\r\n"));
        QVERIFY(server.request.contains("name=\"time\"\r\n\r\n12h\r\n"));
        // UTF-8 filename survives; double quotes are neutralised.
        QVERIFY(server.request.contains(
            QString::fromUtf8("name=\"fileToUpload\"; filename=\"Fête _quoted_ ✓.bin\"").toUtf8()));
        QVERIFY(server.request.contains(original));
    }

    void mockRejectsLinkFromWrongHost()
    {
        MockServer server;
        QVERIFY(server.start());
        server.body = "https://files.catbox.moe/abc.bin";   // Litterbox must answer litter.catbox.moe
        qputenv("LINKDROP_TEST_ENDPOINT", server.url());
        Uploader u;
        u.chooseFile(writeRandomFile(QStringLiteral("host.bin"), 64));
        u.upload(1);
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        qunsetenv("LINKDROP_TEST_ENDPOINT");
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY(u.shareUrl().isEmpty());
    }

    void mockHttpErrorShowsServerMessage()
    {
        MockServer server;
        QVERIFY(server.start());
        server.status = 412;
        server.body = "File is too large.";
        qputenv("LINKDROP_TEST_ENDPOINT", server.url());
        Uploader u;
        u.chooseFile(writeRandomFile(QStringLiteral("big.bin"), 64));
        u.upload(24);
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        qunsetenv("LINKDROP_TEST_ENDPOINT");
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY2(u.errorMessage().contains(QStringLiteral("HTTP 412")), qPrintable(u.errorMessage()));
        QVERIFY(u.errorMessage().contains(QStringLiteral("File is too large.")));
    }

    void mockPlainTextErrorWithOk()
    {
        MockServer server;
        QVERIFY(server.start());
        server.body = "Something went wrong";
        qputenv("LINKDROP_TEST_ENDPOINT", server.url());
        Uploader u;
        u.chooseFile(writeRandomFile(QStringLiteral("x.bin"), 64));
        u.upload(72);
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        qunsetenv("LINKDROP_TEST_ENDPOINT");
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY(server.request.contains("name=\"time\"\r\n\r\n72h\r\n"));
        QVERIFY2(u.errorMessage().endsWith(QStringLiteral("Server said: Something went wrong")),
                 qPrintable(u.errorMessage()));
    }

    void mockHtmlPageIsNotShown()
    {
        MockServer server;
        QVERIFY(server.start());
        server.body = "<html><body>Cloudflare says no</body></html>";
        qputenv("LINKDROP_TEST_ENDPOINT", server.url());
        Uploader u;
        u.chooseFile(writeRandomFile(QStringLiteral("y.bin"), 64));
        u.upload(1);
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        qunsetenv("LINKDROP_TEST_ENDPOINT");
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QCOMPARE(u.errorMessage(), QStringLiteral("The server did not return a valid download link."));
    }

    void mockServerDown()
    {
        quint16 port;
        {
            QTcpServer s;
            QVERIFY(s.listen(QHostAddress::LocalHost, 0));
            port = s.serverPort();
        }
        qputenv("LINKDROP_TEST_ENDPOINT", "http://127.0.0.1:" + QByteArray::number(port) + "/api.php");
        Uploader u;
        u.chooseFile(writeRandomFile(QStringLiteral("z.bin"), 64));
        u.upload(1);
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        qunsetenv("LINKDROP_TEST_ENDPOINT");
        QCOMPARE(u.phase(), QStringLiteral("error"));
        QVERIFY(!u.errorMessage().isEmpty());
    }

    // ---- online -------------------------------------------------------

    void onlineLitterboxRoundTrip()
    {
        if (!online())
            QSKIP("Set LINKDROP_ONLINE_TEST=1 to upload to Litterbox");

        const QString name = QString::fromUtf8("LinkDrop CI test ✓.bin");
        const QString path = writeRandomFile(name, 256 * 1024);
        QFile src(path);
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QByteArray original = src.readAll();

        Uploader u;
        QSignalSpy progressSpy(&u, &Uploader::progressChanged);
        u.chooseFile(path);
        u.upload(1);
        QCOMPARE(u.phase(), QStringLiteral("uploading"));
        QVERIFY(u.busy());
        QVERIFY2(waitForPhaseNot(u, QStringLiteral("uploading"), 180000), "upload timed out");
        QVERIFY2(u.phase() == QStringLiteral("done"),
                 qPrintable(u.status() + QStringLiteral(": ") + u.errorMessage()));
        QCOMPARE(u.progress(), 1.0);
        QVERIFY(progressSpy.count() >= 2);

        const QUrl url(u.shareUrl());
        qInfo("Litterbox returned %s", qPrintable(url.toString()));
        QCOMPARE(url.scheme(), QStringLiteral("https"));
        QCOMPARE(url.host(), QStringLiteral("litter.catbox.moe"));
        QVERIFY(url.path().endsWith(QStringLiteral(".bin")));

        QNetworkAccessManager nam;
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *reply = nam.get(req);
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(120000));
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        const QByteArray downloaded = reply->readAll();
        reply->deleteLater();
        QCOMPARE(downloaded.size(), original.size());
        QCOMPARE(QCryptographicHash::hash(downloaded, QCryptographicHash::Sha256),
                 QCryptographicHash::hash(original, QCryptographicHash::Sha256));
    }

    void onlineCancel()
    {
        if (!online())
            QSKIP("Set LINKDROP_ONLINE_TEST=1 to upload to Litterbox");

        const QString path = writeRandomFile(QStringLiteral("cancel-me.bin"), 24 * 1024 * 1024);
        Uploader u;
        u.chooseFile(path);
        u.upload(1);
        QVERIFY(u.busy());
        QTRY_VERIFY_WITH_TIMEOUT(u.progress() > 0.0 || u.phase() != QStringLiteral("uploading"), 60000);
        u.cancel();
        QVERIFY(waitForPhaseNot(u, QStringLiteral("uploading"), 10000));
        QCOMPARE(u.phase(), QStringLiteral("cancelled"));
        QCOMPARE(u.status(), QStringLiteral("Upload cancelled"));
        QVERIFY(u.shareUrl().isEmpty());
        QVERIFY(!u.busy());
    }
};

QTEST_GUILESS_MAIN(TestUploader)
#include "tst_uploader.moc"
