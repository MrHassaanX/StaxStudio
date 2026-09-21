#pragma once

#include "StudioProject.h"

#include <QString>

class StudioRepository final
{
public:
    explicit StudioRepository(QString storageDirectory = {});

    [[nodiscard]] StudioProject load() const;
    [[nodiscard]] bool save(const StudioProject &project, QString *errorMessage = nullptr) const;
    [[nodiscard]] QString filePath() const;

private:
    QString storageDirectory_;
};
