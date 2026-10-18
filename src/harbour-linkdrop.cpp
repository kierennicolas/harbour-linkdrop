#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include "uploader.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    // Declared before the view so it outlives the QML that binds to it.
    Uploader uploader;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("uploader"), &uploader);
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"), QStringLiteral(APP_VERSION));
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-linkdrop.qml")));
    view->show();
    return app->exec();
}
