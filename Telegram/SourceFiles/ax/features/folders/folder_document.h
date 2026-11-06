#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>
#include <optional>

namespace AxFolders {

[[nodiscard]] std::optional<QByteArray> CanonicalJson(const QJsonObject &value, int maxBytes);
[[nodiscard]] std::optional<QJsonObject> ParseCanonicalJson(const QByteArray &bytes, int maxBytes);
[[nodiscard]] std::optional<QByteArray> Encode(QJsonObject document);
[[nodiscard]] std::optional<QJsonObject> Decode(const QByteArray &bytes, qint64 account);
[[nodiscard]] std::optional<QJsonObject> Merge(const QJsonObject &a, const QJsonObject &b);
[[nodiscard]] QStringList VisibleOrder(const QJsonObject &document);

}
