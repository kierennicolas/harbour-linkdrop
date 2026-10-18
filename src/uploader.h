#ifndef LINKDROP_UPLOADER_H
#define LINKDROP_UPLOADER_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QNetworkAccessManager>

class QNetworkReply;
class QTimer;

class Uploader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedFileName READ selectedFileName NOTIFY changed)
    Q_PROPERTY(QString selectedFileSize READ selectedFileSize NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(QString shareUrl READ shareUrl NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasFile READ hasFile NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit Uploader(QObject *parent = nullptr);

    QString selectedFileName() const { return m_selectedFileName; }
    QString selectedFileSize() const { return m_selectedFileSize; }
    QString phase() const { return m_phase; }
    QString status() const { return m_status; }
    QString errorMessage() const { return m_errorMessage; }
    QString shareUrl() const { return m_shareUrl; }
    bool busy() const { return m_phase == QStringLiteral("uploading"); }
    bool hasFile() const { return !m_path.isEmpty(); }
    double progress() const { return m_progress; }

    Q_INVOKABLE void chooseFile(const QString &pathOrUrl);
    // Hours: 1, 12, 24, 72 = temporary Litterbox; 0 = permanent Catbox.
    Q_INVOKABLE void upload(int hours);
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void progressChanged();

private:
    void fail(const QString &message, const QString &status = QString());
    void setProgress(double value);
    void finishRequest();
    void touchWatchdog();
    static QString readableBytes(qint64 bytes);
    static QString describeServerReply(const QByteArray &body);

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QString m_path;
    QString m_selectedFileName;
    QString m_selectedFileSize;
    QString m_phase = QStringLiteral("idle");
    QString m_status = QStringLiteral("Choose a file to share");
    QString m_errorMessage;
    QString m_shareUrl;
    double m_progress = 0.0;
    bool m_cancelRequested = false;
    bool m_timedOut = false;
    bool m_waitingForServer = false;
    int m_hours = 24;
    QTimer *m_watchdog;
};

#endif
