#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

struct AudioRoute final {
    QString sourceId;
    QString endpointKey;
    bool global = false;
};

// One capture session can have several logical references. Only one active
// reference may feed the program mix for a physical endpoint at a time.
inline QHash<QString, QString> selectAudioRouteOwners(const QVector<AudioRoute> &routes,
                                                       const QSet<QString> &activeSourceIds)
{
    QHash<QString, QString> owners;
    QHash<QString, bool> ownerIsGlobal;
    for (const AudioRoute &route : routes) {
        if (!activeSourceIds.contains(route.sourceId)) continue;
        const auto current = owners.constFind(route.endpointKey);
        if (current == owners.cend() || (route.global && !ownerIsGlobal.value(route.endpointKey))
            || (route.global == ownerIsGlobal.value(route.endpointKey) && route.sourceId < current.value())) {
            owners.insert(route.endpointKey, route.sourceId);
            ownerIsGlobal.insert(route.endpointKey, route.global);
        }
    }
    return owners;
}
