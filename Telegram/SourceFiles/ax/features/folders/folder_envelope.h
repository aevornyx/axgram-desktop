#pragma once

#include "ax/features/folders/folder_document.h"

namespace AxFolders {

[[nodiscard]] std::optional<QByteArray> Seal(const QJsonObject &document, const QByteArray &key, const QString &workspace);
[[nodiscard]] std::optional<QJsonObject> Open(const QByteArray &envelope, const QByteArray &key, const QString &workspace, qint64 account);

}
