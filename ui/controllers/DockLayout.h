#pragma once
#include <QObject>
#include <QVariantMap>
#include <QJsonObject>
#include <QTimer>

// Editor-only split tree, persisted separately from scene/project content.
class DockLayout final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool locked READ locked WRITE setLocked NOTIFY changed)
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QStringList hiddenPanels READ hiddenPanels NOTIFY changed)
public:
    explicit DockLayout(QString directory, QObject *parent=nullptr);
    ~DockLayout() override;
    bool locked() const { return locked_; }
    void setLocked(bool value);
    QVariantMap state() const;
    QStringList hiddenPanels() const { return hidden_; }
    Q_INVOKABLE void setPanelVisible(const QString &panel,bool visible);
    Q_INVOKABLE QVariantMap arrange(double width,double height) const;
    Q_INVOKABLE bool movePanel(const QString &panel,const QString &target,const QString &edge);
    Q_INVOKABLE bool resizeSplit(const QString &id,double ratio);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void save();
    Q_INVOKABLE bool restore(const QVariantMap &state);
signals:
    void changed();
private:
    QJsonObject tree_;
    QString path_;
    bool locked_=false;
    QStringList hidden_;
    QTimer saveTimer_;
    void dirty();
};
