#pragma once

#include "ax/features/folders/folder_document.h"
#include <QtCore/QMap>
#include <vector>

namespace AxFolders {

struct LocalFolder {
	QString uuid;
	int id = 0;
	QJsonObject values;
};

struct Replica {
	qint64 account = 0;
	QString device;
	QJsonObject document;
	QMap<QString, int> ids;
};

[[nodiscard]] std::optional<Replica> Fresh(qint64 account);
[[nodiscard]] std::optional<QByteArray> EncodeState(const Replica &state);
[[nodiscard]] std::optional<Replica> DecodeState(const QByteArray &bytes, qint64 account);
[[nodiscard]] std::optional<Replica> Capture(const Replica &state, const std::vector<LocalFolder> &folders);
[[nodiscard]] std::optional<Replica> Combine(const Replica &state, const QJsonObject &incoming);
[[nodiscard]] std::vector<LocalFolder> LocalFolders(const Replica &state);
[[nodiscard]] std::optional<int> NextLocalId(
	const Replica &state,
	const std::vector<int> &existing);
[[nodiscard]] std::optional<LocalFolder> RebaseEdit(
	const Replica &current,
	const QString &uuid,
	const QJsonObject &baseline,
	const QJsonObject &edited);
[[nodiscard]] std::optional<std::vector<int>> NativeOrder(const Replica &state, const std::vector<int> &existing);
[[nodiscard]] std::optional<std::vector<int>> ServerOrder(const std::vector<int> &existing, const std::vector<int> &incoming);
[[nodiscard]] std::optional<Replica> Load(const QString &root, qint64 account);
[[nodiscard]] bool Save(const QString &root, const Replica &state);

}
