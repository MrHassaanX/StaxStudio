#include "DockLayout.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QSizeF>
#include <QRectF>
#include <cmath>

namespace {
constexpr double Handle=6;
const QSet<QString> panels={"preview","scenes","sources","mixer","transition","controls"};
QJsonObject leaf(const QString &id) { return {{"panel",id}}; }
QJsonObject split(QJsonObject a,QJsonObject b,bool horizontal,double ratio=0.5) {
    return {{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"orientation",horizontal ? "horizontal" : "vertical"},
            {"ratio",ratio},{"first",a},{"second",b}};
}
QSizeF minimum(const QJsonObject &node) {
    const QString panel=node.value("panel").toString();
    if(!panel.isEmpty()) {
        if(panel=="preview") return {260,220};
        if(panel=="sources") return {210,180};
        if(panel=="mixer") return {205,180};
        if(panel=="controls") return {205,150};
        return {170,180};
    }
    auto a=minimum(node.value("first").toObject()), b=minimum(node.value("second").toObject());
    return node.value("orientation")=="horizontal" ? QSizeF(a.width()+b.width()+Handle,qMax(a.height(),b.height()))
        : QSizeF(qMax(a.width(),b.width()),a.height()+b.height()+Handle);
}
bool remove(QJsonObject &node,const QString &panel) {
    if(node.value("panel")==panel) { node={}; return true; }
    if(node.contains("panel")) return false;
    auto a=node.value("first").toObject(),b=node.value("second").toObject();
    if(!remove(a,panel) && !remove(b,panel)) return false;
    if(a.isEmpty()) node=b; else if(b.isEmpty()) node=a;
    else { node["first"]=a; node["second"]=b; }
    return true;
}
bool insert(QJsonObject &node,const QString &target,const QString &panel,const QString &edge) {
    if(node.value("panel")==target) {
        const bool before=edge=="left" || edge=="top";
        node=split(before ? leaf(panel) : node,before ? node : leaf(panel),edge=="left" || edge=="right"); return true;
    }
    if(node.contains("panel")) return false;
    for(const QString &key:{QStringLiteral("first"),QStringLiteral("second")}) {
        auto child=node.value(key).toObject();
        if(insert(child,target,panel,edge)) { node[key]=child; return true; }
    }
    return false;
}
bool resize(QJsonObject &node,const QString &id,double ratio) {
    if(node.value("id")==id) { node["ratio"]=qBound(0.05,ratio,0.95); return true; }
    if(node.contains("panel")) return false;
    for(const QString &key:{QStringLiteral("first"),QStringLiteral("second")}) {
        auto child=node.value(key).toObject();
        if(resize(child,id,ratio)) { node[key]=child; return true; }
    }
    return false;
}
bool valid(const QJsonObject &node,QSet<QString> &seen,QSet<QString> &ids,int depth=0) {
    if(depth>12) return false;
    if(node.contains("panel")) {
        const QString id=node.value("panel").toString();
        if(!panels.contains(id) || seen.contains(id)) return false;
        seen.insert(id); return true;
    }
    const auto orientation=node.value("orientation").toString();
    const QString id=node.value("id").toString();
    const double ratio=node.value("ratio").toDouble(-1);
    if(id.isEmpty() || ids.contains(id) || (orientation!="horizontal" && orientation!="vertical") || !std::isfinite(ratio) || ratio<0.05 || ratio>0.95) return false;
    ids.insert(id);
    return valid(node.value("first").toObject(),seen,ids,depth+1) && valid(node.value("second").toObject(),seen,ids,depth+1);
}
QVariantMap rectangle(QRectF r) { return {{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()}}; }
void arrangeNode(const QJsonObject &node,QRectF rect,QVariantMap &placements,QVariantList &handles) {
    if(node.contains("panel")) { placements[node.value("panel").toString()]=rectangle(rect); return; }
    const auto a=node.value("first").toObject(), b=node.value("second").toObject();
    const bool horizontal=node.value("orientation")=="horizontal";
    const double extent=(horizontal ? rect.width() : rect.height())-Handle;
    const auto minA=minimum(a),minB=minimum(b);
    const double start=qBound(horizontal ? minA.width() : minA.height(),extent*node.value("ratio").toDouble(),
                              extent-(horizontal ? minB.width() : minB.height()));
    QRectF first=rect,second=rect,handle=rect;
    if(horizontal) { first.setWidth(start); handle.setX(rect.x()+start); handle.setWidth(Handle); second.setX(rect.x()+start+Handle); }
    else { first.setHeight(start); handle.setY(rect.y()+start); handle.setHeight(Handle); second.setY(rect.y()+start+Handle); }
    QVariantMap h=rectangle(handle); h["id"]=node.value("id").toString(); h["horizontal"]=horizontal;
    h["origin"]=horizontal ? rect.x() : rect.y(); h["extent"]=extent;
    handles.append(h);
    arrangeNode(a,first,placements,handles); arrangeNode(b,second,placements,handles);
}
}
DockLayout::DockLayout(QString directory,QObject *parent):QObject(parent) {
    if(directory.isEmpty()) directory=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    path_=QDir(directory).filePath("workspace-layout.json");
    saveTimer_.setSingleShot(true); saveTimer_.setInterval(400);
    connect(&saveTimer_,&QTimer::timeout,this,&DockLayout::save);
    QFile file(path_);
    if(file.open(QIODevice::ReadOnly) && restore(QJsonDocument::fromJson(file.readAll()).object().toVariantMap())) { saveTimer_.stop(); return; }
    reset(); saveTimer_.stop();
}
DockLayout::~DockLayout() { if(saveTimer_.isActive()) save(); }
void DockLayout::dirty() { saveTimer_.start(); emit changed(); }
void DockLayout::setLocked(bool value) { if(locked_!=value) {locked_=value;dirty();} }
QVariantMap DockLayout::state() const { return {{"version",1},{"locked",locked_},{"hidden",hidden_},{"tree",tree_.toVariantMap()}}; }
void DockLayout::setPanelVisible(const QString &panel,bool visible) {
    if(locked_ || panel=="preview" || !panels.contains(panel)) return;
    if(visible) hidden_.removeAll(panel); else if(!hidden_.contains(panel)) hidden_.append(panel);
    dirty();
}
QVariantMap DockLayout::arrange(double width,double height) const {
    auto visible=tree_;
    for(const auto &id:hidden_) remove(visible,id);
    const QSizeF min=minimum(visible);
    QVariantMap placements; QVariantList handles;
    arrangeNode(visible,{0,0,qMax(width,min.width()),qMax(height,min.height())},placements,handles);
    return {{"panels",placements},{"handles",handles},{"minimumWidth",min.width()},{"minimumHeight",min.height()}};
}
bool DockLayout::movePanel(const QString &panel,const QString &target,const QString &edge) {
    if(locked_ || panel=="preview" || panel==target || !panels.contains(panel) || !panels.contains(target)
       || !QStringList{"left","right","top","bottom"}.contains(edge)) return false;
    auto candidate=tree_;
    if(!remove(candidate,panel) || !insert(candidate,target,panel,edge)) return false;
    tree_=candidate; dirty(); return true;
}
bool DockLayout::resizeSplit(const QString &id,double ratio) {
    if(locked_ || !std::isfinite(ratio) || !resize(tree_,id,ratio)) return false;
    dirty(); return true;
}
void DockLayout::reset() {
    auto bottom=split(leaf("transition"),leaf("controls"),true,0.45);
    bottom=split(leaf("mixer"),bottom,true,0.34);
    bottom=split(leaf("sources"),bottom,true,0.27);
    bottom=split(leaf("scenes"),bottom,true,0.18);
    tree_=split(leaf("preview"),bottom,false,0.56); locked_=false; hidden_.clear(); dirty();
}
bool DockLayout::restore(const QVariantMap &state) {
    const auto tree=QJsonObject::fromVariantMap(state.value("tree").toMap());
    QSet<QString> seen,ids;
    if(state.value("version").toInt()!=1 || !valid(tree,seen,ids) || seen!=panels) return false;
    const auto hidden=state.value("hidden").toStringList();
    for(const auto &id:hidden) if(id=="preview" || !panels.contains(id)) return false;
    tree_=tree; hidden_=hidden; locked_=state.value("locked").toBool(); dirty(); return true;
}
void DockLayout::save() {
    saveTimer_.stop(); QDir().mkpath(QFileInfo(path_).absolutePath());
    QSaveFile file(path_);
    if(file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument::fromVariant(state()).toJson()); file.commit(); }
}
