#include "ax/features/folders/folder_replica.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <algorithm>
#include <limits>

namespace AxFolders {
namespace {

constexpr auto StateBytes = 3 * 1024 * 1024;

QMap<QString, QJsonObject> FolderMap(const QJsonObject &document) {
	auto result = QMap<QString, QJsonObject>();
	for (const auto &value : document.value(QStringLiteral("folders")).toArray()) {
		const auto folder = value.toObject();
		result.insert(folder.value(QStringLiteral("id")).toString(), folder);
	}
	return result;
}

QJsonArray FolderArray(const QMap<QString, QJsonObject> &folders) {
	auto result = QJsonArray();
	for (const auto &folder : folders) {
		result.push_back(folder);
	}
	return result;
}

QJsonObject Register(qint64 clock, const QString &device, const QJsonValue &value) {
	return {
		{ QStringLiteral("clock"), QString::number(clock) },
		{ QStringLiteral("device"), device },
		{ QStringLiteral("value"), value },
	};
}

QStringList LiveIds(const QJsonObject &document) {
	auto result = QStringList();
	for (const auto &reference : VisibleOrder(document)) {
		if (reference.startsWith(QStringLiteral("local:"))) {
			result.push_back(reference.mid(6));
		}
	}
	return result;
}

bool Uuid(const QString &value) {
	return value == QUuid(value).toString(QUuid::WithoutBraces);
}

QString Path(const QString &root, qint64 account) {
	return QDir(root).filePath(QString::number(account) + QStringLiteral("/replica.json"));
}

}

std::optional<Replica> Fresh(qint64 account) {
	if (account <= 0) {
		return std::nullopt;
	}
	return Replica{
		account,
		QUuid::createUuid().toString(QUuid::WithoutBraces),
		{
			{ QStringLiteral("v"), 1 },
			{ QStringLiteral("account"), QString::number(account) },
			{ QStringLiteral("folders"), QJsonArray() },
		},
		{},
	};
}

std::optional<QByteArray> EncodeState(const Replica &state) {
	const auto document = Encode(state.document);
	if (!document || !Decode(*document, state.account) || !Uuid(state.device)) {
		return std::nullopt;
	}
	const auto live = LiveIds(state.document);
	if (live.size() != state.ids.size()) {
		return std::nullopt;
	}
	auto used = QSet<int>();
	auto mappings = QJsonObject();
	for (auto i = state.ids.begin(); i != state.ids.end(); ++i) {
		if (!live.contains(i.key()) || i.value() < 1000 || used.contains(i.value())) {
			return std::nullopt;
		}
		used.insert(i.value());
		mappings.insert(i.key(), i.value());
	}
	return CanonicalJson({
		{ QStringLiteral("v"), 1 },
		{ QStringLiteral("account"), QString::number(state.account) },
		{ QStringLiteral("device"), state.device },
		{ QStringLiteral("document"), QString::fromUtf8(*document) },
		{ QStringLiteral("ids"), mappings },
	}, StateBytes);
}

std::optional<Replica> DecodeState(const QByteArray &bytes, qint64 account) {
	const auto json = ParseCanonicalJson(bytes, StateBytes);
	if (!json || json->size() != 5 || json->value(QStringLiteral("v")) != QJsonValue(1)
		|| json->value(QStringLiteral("account")) != QJsonValue(QString::number(account))
		|| !json->value(QStringLiteral("device")).isString()
		|| !json->value(QStringLiteral("document")).isString()
		|| !json->value(QStringLiteral("ids")).isObject()) {
		return std::nullopt;
	}
	const auto document = Decode(json->value(QStringLiteral("document")).toString().toUtf8(), account);
	if (!document) {
		return std::nullopt;
	}
	auto result = Replica{ account, json->value(QStringLiteral("device")).toString(), *document, {} };
	const auto ids = json->value(QStringLiteral("ids")).toObject();
	for (auto i = ids.begin(); i != ids.end(); ++i) {
		if (!i.value().isDouble() || i.value().toDouble() < 1000 || i.value().toDouble() > std::numeric_limits<int>::max()
			|| i.value().toDouble() != i.value().toInt()) {
			return std::nullopt;
		}
		result.ids.insert(i.key(), i.value().toInt());
	}
	return EncodeState(result) ? std::make_optional(result) : std::nullopt;
}

std::optional<Replica> Capture(const Replica &state, const std::vector<LocalFolder> &source) {
	if (!EncodeState(state)) {
		return std::nullopt;
	}
	auto next = state;
	auto maximum = qint64(0);
	const auto scan = [&](const QJsonValue &value) {
		maximum = std::max(maximum, value.toObject().value(QStringLiteral("clock")).toString().toLongLong());
	};
	auto folders = FolderMap(state.document);
	scan(state.document.value(QStringLiteral("order")));
	for (const auto &folder : folders) {
		scan(folder.value(QStringLiteral("deleted")));
		const auto fields = folder.value(QStringLiteral("fields")).toObject();
		for (auto i = fields.begin(); i != fields.end(); ++i) {
			scan(i.value());
		}
	}
	auto mappings = QMap<QString, int>();
	auto seen = QSet<int>();
	auto uuids = QSet<QString>();
	auto order = QJsonArray();
	for (const auto &local : source) {
		if (local.id < 0 || seen.contains(local.id)) {
			return std::nullopt;
		}
		seen.insert(local.id);
		if (local.id < 1000) {
			order.push_back(QStringLiteral("remote:") + QString::number(local.id));
			continue;
		}
		if (!Uuid(local.uuid) || uuids.contains(local.uuid)) {
			return std::nullopt;
		}
		if (local.values.size() != 4 || !local.values.contains(QStringLiteral("title"))
			|| !local.values.contains(QStringLiteral("appearance")) || !local.values.contains(QStringLiteral("rules"))
			|| !local.values.contains(QStringLiteral("pinned"))) {
			return std::nullopt;
		}
		uuids.insert(local.uuid);
		auto folder = folders.value(local.uuid);
		if (folder.contains(QStringLiteral("deleted"))) {
			continue;
		}
		auto fields = folder.value(QStringLiteral("fields")).toObject();
		auto changed = false;
		for (auto i = local.values.begin(); i != local.values.end(); ++i) {
			if (fields.value(i.key()).toObject().value(QStringLiteral("value")) != i.value()) {
				if (maximum == std::numeric_limits<qint64>::max()) {
					return std::nullopt;
				}
				fields.insert(i.key(), Register(maximum + 1, state.device, i.value()));
				changed = true;
			}
		}
		if (changed) {
			++maximum;
			folder.insert(QStringLiteral("id"), local.uuid);
			folder.insert(QStringLiteral("fields"), fields);
			folders.insert(local.uuid, folder);
		}
		mappings.insert(local.uuid, local.id);
		order.push_back(QStringLiteral("local:") + local.uuid);
	}
	for (auto i = state.ids.begin(); i != state.ids.end(); ++i) {
		if (!mappings.contains(i.key())) {
			if (maximum == std::numeric_limits<qint64>::max()) {
				return std::nullopt;
			}
			auto folder = folders.value(i.key());
			folder.insert(QStringLiteral("deleted"), Register(++maximum, state.device, true));
			folders.insert(i.key(), folder);
		}
	}
	next.document.insert(QStringLiteral("folders"), FolderArray(folders));
	next.ids = mappings;
	auto actual = std::vector<int>();
	for (const auto &reference : order) {
		const auto value = reference.toString();
		actual.push_back(value.startsWith(QStringLiteral("local:"))
			? mappings.value(value.mid(6)) : value.mid(7).toInt());
	}
	const auto projected = NativeOrder(next, actual);
	if (!state.document.contains(QStringLiteral("order")) || !projected || *projected != actual) {
		if (maximum == std::numeric_limits<qint64>::max()) {
			return std::nullopt;
		}
		next.document.insert(QStringLiteral("order"), Register(++maximum, state.device, order));
	}
	return EncodeState(next) ? std::make_optional(next) : std::nullopt;
}

std::optional<Replica> Combine(const Replica &state, const QJsonObject &incoming) {
	if (!EncodeState(state)) {
		return std::nullopt;
	}
	const auto merged = Merge(state.document, incoming);
	if (!merged) {
		return std::nullopt;
	}
	auto next = state;
	next.document = *merged;
	next.ids.clear();
	auto used = QSet<int>();
	const auto live = LiveIds(*merged);
	for (const auto &id : live) {
		if (state.ids.contains(id)) {
			next.ids.insert(id, state.ids.value(id));
			used.insert(state.ids.value(id));
		}
	}
	auto candidate = 1000;
	for (const auto &id : live) {
		if (next.ids.contains(id)) {
			continue;
		}
		while (used.contains(candidate)) {
			if (candidate == std::numeric_limits<int>::max()) {
				return std::nullopt;
			}
			++candidate;
		}
		next.ids.insert(id, candidate);
		used.insert(candidate);
	}
	return EncodeState(next) ? std::make_optional(next) : std::nullopt;
}

std::vector<LocalFolder> LocalFolders(const Replica &state) {
	if (!EncodeState(state)) {
		return {};
	}
	auto result = std::vector<LocalFolder>();
	const auto folders = FolderMap(state.document);
	for (const auto &id : LiveIds(state.document)) {
		auto values = QJsonObject();
		const auto fields = folders.value(id).value(QStringLiteral("fields")).toObject();
		for (auto i = fields.begin(); i != fields.end(); ++i) {
			values.insert(i.key(), i.value().toObject().value(QStringLiteral("value")));
		}
		result.push_back({ id, state.ids.value(id), values });
	}
	return result;
}

std::optional<int> NextLocalId(
		const Replica &state,
		const std::vector<int> &existing) {
	if (!EncodeState(state) || state.ids.size() >= 20) {
		return std::nullopt;
	}
	auto used = QSet<int>();
	for (const auto id : existing) {
		if (id < 0 || used.contains(id)) {
			return std::nullopt;
		}
		used.insert(id);
	}
	for (const auto id : state.ids) {
		used.insert(id);
	}
	auto candidate = 1000;
	while (used.contains(candidate)) {
		if (candidate == std::numeric_limits<int>::max()) {
			return std::nullopt;
		}
		++candidate;
	}
	return candidate;
}

std::optional<LocalFolder> RebaseEdit(
		const Replica &current,
		const QString &uuid,
		const QJsonObject &baseline,
		const QJsonObject &edited) {
	if (!EncodeState(current) || !Uuid(uuid)
		|| baseline.size() != 4 || edited.size() != 4) {
		return std::nullopt;
	}
	for (auto folder : LocalFolders(current)) {
		if (folder.uuid != uuid) {
			continue;
		}
		for (auto i = folder.values.begin(); i != folder.values.end(); ++i) {
			if (!baseline.contains(i.key()) || !edited.contains(i.key())) {
				return std::nullopt;
			}
			if (baseline.value(i.key()) != edited.value(i.key())) {
				i.value() = edited.value(i.key());
			}
		}
		return folder;
	}
	return std::nullopt;
}

std::optional<std::vector<int>> NativeOrder(const Replica &state, const std::vector<int> &existing) {
	if (!EncodeState(state)) return std::nullopt;
	auto present = QSet<int>();
	for (const auto id : existing) {
		if (id < 0 || present.contains(id)) return std::nullopt;
		present.insert(id);
	}
	auto result = std::vector<int>();
	for (const auto &reference : VisibleOrder(state.document)) {
		const auto id = reference.startsWith(QStringLiteral("local:"))
			? state.ids.value(reference.mid(6), -1)
			: reference.mid(7).toInt();
		if (present.remove(id)) result.push_back(id);
	}
	for (const auto id : existing) {
		if (present.remove(id)) result.push_back(id);
	}
	return result;
}

std::optional<std::vector<int>> ServerOrder(const std::vector<int> &existing, const std::vector<int> &incoming) {
	auto present = QSet<int>();
	for (const auto id : existing) {
		if (id < 0 || present.contains(id)) return std::nullopt;
		present.insert(id);
	}
	auto server = std::vector<int>();
	auto seen = QSet<int>();
	for (const auto id : incoming) {
		if (id < 0 || id >= 1000 || seen.contains(id)) return std::nullopt;
		seen.insert(id);
		if (id == 0 && !present.contains(0)) continue;
		if (!present.contains(id)) return std::nullopt;
		server.push_back(id);
	}
	auto result = std::vector<int>();
	auto position = size_t(0);
	for (const auto id : existing) {
		if (id >= 1000) {
			result.push_back(id);
		} else if (position < server.size()) {
			result.push_back(server[position++]);
		} else {
			return std::nullopt;
		}
	}
	return position == server.size() ? std::make_optional(result) : std::nullopt;
}

std::optional<Replica> Load(const QString &root, qint64 account) {
	if (account <= 0) {
		return std::nullopt;
	}
	QFile file(Path(root, account));
	if (!file.exists()) {
		return Fresh(account);
	} else if (!file.open(QIODevice::ReadOnly) || file.size() > StateBytes) {
		return std::nullopt;
	}
	return DecodeState(file.readAll(), account);
}

bool Save(const QString &root, const Replica &state) {
	const auto bytes = EncodeState(state);
	if (!bytes || !QDir(root).mkpath(QString::number(state.account))) {
		return false;
	}
	QSaveFile file(Path(root, state.account));
	return file.open(QIODevice::WriteOnly) && file.write(*bytes) == bytes->size() && file.commit();
}

}
