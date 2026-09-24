#include "VisualSourceManager.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QPixmap>
#include <QScreen>
#include <QVideoFrame>
#include <QVideoSink>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QString cameraId(const QCameraDevice &device)
{
    return QString::fromLatin1(device.id().toBase64(QByteArray::Base64UrlEncoding));
}

#ifdef Q_OS_WIN
struct WindowEntry final {
    QString id;
    QString title;
};

BOOL CALLBACK collectWindow(HWND window, LPARAM parameter)
{
    auto *entries = reinterpret_cast<QVector<WindowEntry> *>(parameter);
    if (!IsWindowVisible(window) || IsIconic(window) || GetWindow(window, GW_OWNER)) return TRUE;
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) return TRUE;
    QString title(length + 1, Qt::Uninitialized);
    GetWindowTextW(window, reinterpret_cast<wchar_t *>(title.data()), length + 1);
    title.truncate(length);
    if (!title.trimmed().isEmpty()) entries->append({QString::number(reinterpret_cast<quintptr>(window)), title});
    return TRUE;
}
#endif
}

VisualSourceManager::VisualSourceManager(QObject *parent) : QObject(parent)
{
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, &VisualSourceManager::captureDesktopFrames);
}

VisualSourceManager::~VisualSourceManager()
{
    for (auto it = cameras_.cbegin(); it != cameras_.cend(); ++it) if (it->camera) it->camera->stop();
}

bool VisualSourceManager::isVisual(const SourceType type)
{
    return type == SourceType::DisplayCapture || type == SourceType::WindowCapture || type == SourceType::GameCapture || type == SourceType::Webcam;
}

void VisualSourceManager::synchronizeSources(const QVector<Source> &sources)
{
    QHash<QString, Source> next;
    for (const Source &source : sources) if (isVisual(source.type)) next.insert(source.id, source);
    QStringList camerasToStop;
    for (auto it = cameras_.cbegin(); it != cameras_.cend(); ++it) {
        if (!next.contains(it.key()) || next.value(it.key()).configuration.value("targetId").toString() != it->targetId)
            camerasToStop.append(it.key());
    }
    for (const QString &id : camerasToStop) stopCamera(id);
    sources_ = std::move(next);
    for (const Source &source : sources_) if (source.type == SourceType::Webcam) startCamera(source);
    if (!timer_.isActive() && !sources_.isEmpty()) timer_.start();
    if (sources_.isEmpty()) timer_.stop();
}

CapturedVideoFrame VisualSourceManager::frame(const QString &sourceId) const
{
    return frames_.value(sourceId, CapturedVideoFrame{{}, {}, 0, CaptureState::Stopped, QStringLiteral("Not started")});
}

QVariantList VisualSourceManager::targets(const SourceType type) const
{
    QVariantList result;
    if (type == SourceType::DisplayCapture) {
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            const QSize size = screen->size();
            result.append(CaptureTarget{screen->name(), screen->name(), size, screen == QGuiApplication::primaryScreen(), QStringLiteral("%1 x %2").arg(size.width()).arg(size.height())}.toVariantMap());
        }
    } else if (type == SourceType::WindowCapture || type == SourceType::GameCapture) {
#ifdef Q_OS_WIN
        QVector<WindowEntry> windows;
        EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&windows));
        for (const WindowEntry &window : windows) result.append(CaptureTarget{window.id, window.title, {}, false, type == SourceType::GameCapture ? QStringLiteral("Windowed or borderless game") : QStringLiteral("Application window")}.toVariantMap());
#endif
    } else if (type == SourceType::Webcam) {
        for (const QCameraDevice &device : QMediaDevices::videoInputs()) result.append(CaptureTarget{cameraId(device), device.description(), {}, device == QMediaDevices::defaultVideoInput(), QStringLiteral("Camera")}.toVariantMap());
    }
    return result;
}

QVariantList VisualSourceManager::cameraFormats(const QString &id) const
{
    QVariantList result;
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) {
        if (cameraId(device) != id) continue;
        for (const QCameraFormat &format : device.videoFormats()) {
            const QSize resolution = format.resolution();
            result.append(QVariantMap{{"id", QStringLiteral("%1x%2@%3").arg(resolution.width()).arg(resolution.height()).arg(qRound(format.maxFrameRate()))}, {"name", QStringLiteral("%1 x %2 @ %3 FPS").arg(resolution.width()).arg(resolution.height()).arg(qRound(format.maxFrameRate()))}, {"width", resolution.width()}, {"height", resolution.height()}, {"fps", qRound(format.maxFrameRate())}});
        }
        break;
    }
    return result;
}

QString VisualSourceManager::defaultTarget(const SourceType type) const
{
    const QVariantList list = targets(type);
    return list.isEmpty() ? QString{} : list.first().toMap().value("id").toString();
}

void VisualSourceManager::updateFrame(const QString &sourceId, CapturedVideoFrame frame)
{
    const CapturedVideoFrame old = frames_.value(sourceId);
    const bool stateChanged = old.state != frame.state || old.message != frame.message;
    frames_.insert(sourceId, std::move(frame));
    emit framesChanged();
    if (stateChanged) emit sourceStateChanged(sourceId);
}

void VisualSourceManager::captureDesktopFrames()
{
    for (const Source &source : sources_) {
        if (source.type == SourceType::Webcam) continue;
        captureDesktopSource(source);
    }
}

void VisualSourceManager::captureDesktopSource(const Source &source)
{
    const QString target = source.configuration.value("targetId").toString();
    if (target.isEmpty()) {
        updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Unavailable, QStringLiteral("Choose a capture target")});
        return;
    }
#ifdef Q_OS_WIN
    // Display/window pixels are acquired by the D3D11 compositor through DXGI
    // Desktop Duplication. Keep this manager limited to lifecycle/state work.
    if (source.type == SourceType::DisplayCapture) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->name() == target) {
                updateFrame(source.id, {{}, screen->size(), mediaTimestampNs(), CaptureState::Active, {}});
                return;
            }
        }
        updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Unavailable, QStringLiteral("Display disconnected")});
        return;
    }
    bool valid = false;
    const WId window = static_cast<WId>(target.toULongLong(&valid));
    if (!valid || !IsWindow(reinterpret_cast<HWND>(window)) || IsIconic(reinterpret_cast<HWND>(window))) {
        updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Unavailable, QStringLiteral("Window unavailable")});
        return;
    }
    updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Active, {}});
    return;
#else
    QPixmap pixmap;
    if (source.type == SourceType::DisplayCapture) {
        QScreen *screen = nullptr;
        for (QScreen *candidate : QGuiApplication::screens()) if (candidate->name() == target) { screen = candidate; break; }
        if (!screen) { updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Unavailable, QStringLiteral("Display disconnected")}); return; }
        pixmap = screen->grabWindow(0);
    }
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull()) { updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Error, QStringLiteral("Capture returned no frame")}); return; }
    updateFrame(source.id, {image, image.size(), mediaTimestampNs(), CaptureState::Active, {}});
#endif
}
void VisualSourceManager::startCamera(const Source &source)
{
    if (cameras_.contains(source.id)) return;
    const QString target = source.configuration.value("targetId").toString();
    QCameraDevice selected;
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) if (cameraId(device) == target) { selected = device; break; }
    if (selected.isNull()) { updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Unavailable, QStringLiteral("Camera unavailable")}); return; }
    auto *camera = new QCamera(selected, this);
    const QString wantedFormat = source.configuration.value("formatId").toString();
    for (const QCameraFormat &format : selected.videoFormats()) {
        const QSize size = format.resolution();
        const QString id = QStringLiteral("%1x%2@%3").arg(size.width()).arg(size.height()).arg(qRound(format.maxFrameRate()));
        if (id == wantedFormat) { camera->setCameraFormat(format); break; }
    }
    auto *session = new QMediaCaptureSession(camera);
    auto *sink = new QVideoSink(camera);
    session->setCamera(camera);
    session->setVideoSink(sink);
    connect(sink, &QVideoSink::videoFrameChanged, this, [this, sourceId = source.id](const QVideoFrame &frame) {
        const QImage image = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
        if (!image.isNull()) updateFrame(sourceId, {image, image.size(), mediaTimestampNs(), CaptureState::Active, {}});
    });
    connect(camera, &QCamera::errorOccurred, this, [this, sourceId = source.id](QCamera::Error, const QString &message) {
        updateFrame(sourceId, {{}, {}, mediaTimestampNs(), CaptureState::Error, message});
    });
    cameras_.insert(source.id, {camera, session, sink, target});
    updateFrame(source.id, {{}, {}, mediaTimestampNs(), CaptureState::Starting, {}});
    camera->start();
}

void VisualSourceManager::stopCamera(const QString &sourceId)
{
    const CameraSession session = cameras_.take(sourceId);
    if (session.camera) { session.camera->stop(); session.camera->deleteLater(); }
    if (session.session) session.session->deleteLater();
    if (session.sink) session.sink->deleteLater();
    frames_.remove(sourceId);
}
