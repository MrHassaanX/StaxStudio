#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QString>

#include "ui/controllers/AppController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QGuiApplication::setApplicationName(QStringLiteral("StaxStudio"));
    QGuiApplication::setOrganizationName(QStringLiteral("StaxStudio"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("staxstudio.app"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppController appController;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );
    engine.loadFromModule(QStringLiteral("StaxStudio"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
