// Loads every QML file of the app the same way the app does (QQuickView +
// context properties) and counts errors/warnings that point at app files.
// Messages coming from inside Silica itself are printed but not counted.

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTimer>
#include <QUrl>
#include <cstdio>
#include <cstdlib>

#include "uploader.h"

static QString g_appQmlUrl;
static QString g_appQmlPath;
static int g_problems = 0;

static void handler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const bool ours = msg.contains(g_appQmlUrl) || msg.contains(g_appQmlPath);
    const bool counts = ours && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg);
    if (counts)
        ++g_problems;
    fprintf(stderr, "%s%s\n", counts ? "[APP-QML] " : "          ", qPrintable(msg));
    if (type == QtFatalMsg)
        abort();
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "usage: qmlcheck <app qml dir>\n");
        return 2;
    }
    const QDir root(QString::fromLocal8Bit(argv[1]));
    g_appQmlPath = root.absolutePath();
    g_appQmlUrl = QUrl::fromLocalFile(g_appQmlPath).toString();
    qInstallMessageHandler(handler);

    Uploader uploader;
    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("uploader"), &uploader);
    view.rootContext()->setContextProperty(QStringLiteral("appVersion"), QStringLiteral("qmlcheck"));

    view.setSource(QUrl::fromLocalFile(root.filePath(QStringLiteral("harbour-linkdrop.qml"))));
    if (view.status() != QQuickView::Ready || !view.rootObject()) {
        for (const QQmlError &e : view.errors())
            qWarning("%s", qPrintable(e.toString()));
        ++g_problems;
    } else {
        fprintf(stderr, "OK  harbour-linkdrop.qml (root: %s)\n", view.rootObject()->metaObject()->className());
    }

    const char *files[] = { "pages/MainPage.qml", "pages/AboutPage.qml", "cover/CoverPage.qml" };
    QList<QObject *> keep;
    for (const char *f : files) {
        QQmlComponent c(view.engine(), QUrl::fromLocalFile(root.filePath(QString::fromLatin1(f))));
        QObject *o = c.create(view.rootContext());
        if (!o) {
            for (const QQmlError &e : c.errors())
                qWarning("%s", qPrintable(e.toString()));
            ++g_problems;
        } else {
            fprintf(stderr, "OK  %s (%s)\n", f, o->metaObject()->className());
            keep << o;
        }
    }

    // Drive the state machine so state-dependent bindings get evaluated.
    QTimer::singleShot(200, &app, [&]() {
        uploader.chooseFile(QCoreApplication::applicationFilePath());   // a readable file
        fprintf(stderr, "state: %s / %s\n", qPrintable(uploader.phase()), qPrintable(uploader.selectedFileName()));
    });
    QTimer::singleShot(500, &app, [&]() {
        uploader.chooseFile(QStringLiteral("/nonexistent/file"));       // error state
        fprintf(stderr, "state: %s / %s\n", qPrintable(uploader.phase()), qPrintable(uploader.status()));
    });
    QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    app.exec();

    qDeleteAll(keep);
    fprintf(stderr, "qmlcheck: %d problem(s) in app QML\n", g_problems);
    return g_problems ? 1 : 0;
}
