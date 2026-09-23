#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QString>

#include "ui/controllers/AppController.h"
#include "ui/controllers/StudioController.h"
#include "ui/render/ProgramPreview.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    qmlRegisterType<ProgramPreview>("StaxStudio.Render", 1, 0, "ProgramPreview");

    QGuiApplication::setApplicationName(QStringLiteral("StaxStudio"));
    QGuiApplication::setOrganizationName(QStringLiteral("StaxStudio"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("staxstudio.app"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppController appController;
    StudioController studioController;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);
    engine.rootContext()->setContextProperty(QStringLiteral("studioController"), &studioController);

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
