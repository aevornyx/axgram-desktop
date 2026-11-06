#include "ax/features/folders/folder_document.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <cmath>
#include <limits>

namespace AxFolders {
namespace {

constexpr auto MaxBytes = 1024 * 1024 - 33;

QStringList Fields() {
	return { QStringLiteral("title"), QStringLiteral("appearance"), QStringLiteral("rules"), QStringLiteral("pinned") };
}

bool Keys(const QJsonObject &object, const QStringList &allowed) {
	for (auto i = object.begin(); i != object.end(); ++i) {
		if (!allowed.contains(i.key())) {
			return false;
		}
	}
	return true;
}

bool Uuid(const QJsonValue &value) {
	static const auto pattern = QRegularExpression(QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\z"));
	return value.isString() && pattern.match(value.toString()).hasMatch();
}

std::optional<qint64> Decimal(const QJsonValue &value, bool zero) {
	if (!value.isString()) {
		return std::nullopt;
	}
	static const auto pattern = QRegularExpression(QStringLiteral("^(0|[1-9][0-9]{0,18})\\z"));
	const auto text = value.toString();
	if (!pattern.match(text).hasMatch() || (!zero && text == QStringLiteral("0"))) {
		return std::nullopt;
	}
	auto ok = false;
	const auto number = text.toLongLong(&ok);
	return ok ? std::make_optional(number) : std::nullopt;
}

std::optional<QByteArray> Canonical(const QJsonValue &value, int depth = 0) {
	if (depth > 24) {
		return std::nullopt;
	}
	if (value.isNull()) {
		return QByteArrayLiteral("null");
	} else if (value.isBool()) {
		return value.toBool() ? QByteArrayLiteral("true") : QByteArrayLiteral("false");
	} else if (value.isDouble()) {
		const auto number = value.toDouble();
		if (!std::isfinite(number) || std::trunc(number) != number || std::abs(number) > std::numeric_limits<int>::max()) {
			return std::nullopt;
		}
		return QByteArray::number(qint64(number));
	} else if (value.isString()) {
		const auto text = value.toString();
		auto escaped = QStringLiteral("\"");
		for (auto i = 0; i < text.size(); ++i) {
			const auto c = text[i];
			if (c.isHighSurrogate()) {
				if (i + 1 >= text.size() || !text[i + 1].isLowSurrogate()) {
					return std::nullopt;
				}
				escaped += c;
				escaped += text[++i];
			} else if (c.isLowSurrogate()) {
				return std::nullopt;
			} else if (c == QLatin1Char('"') || c == QLatin1Char('\\')) {
				escaped += QLatin1Char('\\');
				escaped += c;
			} else if (c.unicode() < 32) {
				escaped += QStringLiteral("\\u") + QString::number(c.unicode(), 16).rightJustified(4, QLatin1Char('0'));
			} else {
				escaped += c;
			}
		}
		return (escaped + QLatin1Char('"')).toUtf8();
	} else if (value.isArray()) {
		auto result = QByteArrayLiteral("[");
		auto first = true;
		for (const auto &item : value.toArray()) {
			const auto encoded = Canonical(item, depth + 1);
			if (!encoded) {
				return std::nullopt;
			}
			if (!first) {
				result += ',';
			}
			first = false;
			result += *encoded;
		}
		return result + ']';
	} else if (value.isObject()) {
		const auto object = value.toObject();
		auto names = object.keys();
		names.sort();
		auto result = QByteArrayLiteral("{");
		auto first = true;
		for (const auto &name : names) {
			const auto key = Canonical(name, depth + 1);
			const auto encoded = Canonical(object.value(name), depth + 1);
			if (!key || !encoded) {
				return std::nullopt;
			}
			if (!first) {
				result += ',';
			}
			first = false;
			result += *key + ':' + *encoded;
		}
		return result + '}';
	}
	return std::nullopt;
}

bool Register(const QJsonValue &value) {
	if (!value.isObject()) {
		return false;
	}
	const auto object = value.toObject();
	return Keys(object, { QStringLiteral("clock"), QStringLiteral("device"), QStringLiteral("value") })
		&& Decimal(object.value(QStringLiteral("clock")), true).has_value()
		&& Uuid(object.value(QStringLiteral("device")))
		&& object.contains(QStringLiteral("value"));
}

bool Integer(const QJsonValue &value, int min, int max) {
	const auto number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
	return value.isDouble() && std::isfinite(number) && std::trunc(number) == number && number >= min && number <= max;
}

bool Peers(const QJsonValue &value) {
	if (!value.isArray() || value.toArray().size() > 10000) {
		return false;
	}
	auto seen = QSet<QString>();
	for (const auto &entry : value.toArray()) {
		const auto peer = entry.toObject();
		const auto type = peer.value(QStringLiteral("type"));
		if (!entry.isObject() || !Keys(peer, { QStringLiteral("type"), QStringLiteral("id") })
			|| !QStringList({ QStringLiteral("user"), QStringLiteral("chat"), QStringLiteral("channel") }).contains(type.toString())
			|| !Decimal(peer.value(QStringLiteral("id")), false)) {
			return false;
		}
		const auto key = type.toString() + QLatin1Char(':') + peer.value(QStringLiteral("id")).toString();
		if (seen.contains(key)) {
			return false;
		}
		seen.insert(key);
	}
	return true;
}

bool Definition(const QString &name, const QJsonValue &value) {
	if (name == QStringLiteral("pinned")) {
		return Peers(value);
	} else if (!value.isObject()) {
		return false;
	}
	const auto object = value.toObject();
	if (name == QStringLiteral("appearance")) {
		return Keys(object, { QStringLiteral("emoji"), QStringLiteral("color") })
			&& object.value(QStringLiteral("emoji")).isString()
			&& object.value(QStringLiteral("emoji")).toString().size() <= 64
			&& (object.value(QStringLiteral("color")).isNull() || Integer(object.value(QStringLiteral("color")), 0, 255));
	} else if (name == QStringLiteral("rules")) {
		const auto flags = QStringList{ QStringLiteral("contacts"), QStringLiteral("nonContacts"), QStringLiteral("groups"), QStringLiteral("channels"), QStringLiteral("bots"), QStringLiteral("excludeMuted"), QStringLiteral("excludeRead"), QStringLiteral("excludeArchived"), QStringLiteral("onlyArchived") };
		if (!Keys(object, flags + QStringList{ QStringLiteral("include"), QStringLiteral("exclude") })) {
			return false;
		}
		for (const auto &flag : flags) {
			if (!object.value(flag).isBool()) {
				return false;
			}
		}
		return !(object.value(QStringLiteral("onlyArchived")).toBool() && object.value(QStringLiteral("excludeArchived")).toBool())
			&& Peers(object.value(QStringLiteral("include"))) && Peers(object.value(QStringLiteral("exclude")));
	} else if (name != QStringLiteral("title")) {
		return false;
	}
	const auto text = object.value(QStringLiteral("text")).toString();
	auto nonBlank = false;
	for (const auto c : text) {
		if (c.unicode() > 32) {
			nonBlank = true;
			break;
		}
	}
	const auto entities = object.value(QStringLiteral("entities"));
	if (!Keys(object, { QStringLiteral("text"), QStringLiteral("noAnimate"), QStringLiteral("entities") })
		|| !object.value(QStringLiteral("text")).isString() || !nonBlank || text.size() > 256
		|| !object.value(QStringLiteral("noAnimate")).isBool() || !entities.isArray() || entities.toArray().size() > 256) {
		return false;
	}
	for (const auto &entry : entities.toArray()) {
		const auto entity = entry.toObject();
		const auto type = entity.value(QStringLiteral("type")).toString();
		const auto allowed = QStringList{ QStringLiteral("bold"), QStringLiteral("italic"), QStringLiteral("underline"), QStringLiteral("strike"), QStringLiteral("spoiler"), QStringLiteral("code"), QStringLiteral("pre"), QStringLiteral("url"), QStringLiteral("textUrl"), QStringLiteral("mention"), QStringLiteral("mentionName"), QStringLiteral("customEmoji"), QStringLiteral("blockquote"), QStringLiteral("expandableBlockquote"), QStringLiteral("hashtag"), QStringLiteral("cashtag"), QStringLiteral("botCommand"), QStringLiteral("email"), QStringLiteral("phone"), QStringLiteral("bankCard") };
		if (!entry.isObject() || !Keys(entity, { QStringLiteral("type"), QStringLiteral("offset"), QStringLiteral("length"), QStringLiteral("id"), QStringLiteral("url"), QStringLiteral("language") })
			|| !allowed.contains(type) || !Integer(entity.value(QStringLiteral("offset")), 0, text.size())) {
			return false;
		}
		const auto offset = entity.value(QStringLiteral("offset")).toInt();
		if (!Integer(entity.value(QStringLiteral("length")), 1, text.size() - offset)) {
			return false;
		}
		const auto length = entity.value(QStringLiteral("length")).toInt();
		if ((offset < text.size() && text[offset].isLowSurrogate())
			|| (offset + length < text.size() && text[offset + length].isLowSurrogate())) {
			return false;
		}
		if ((type == QStringLiteral("customEmoji") || type == QStringLiteral("mentionName")) && !Decimal(entity.value(QStringLiteral("id")), false)) {
			return false;
		}
		if (type == QStringLiteral("textUrl") && (!entity.value(QStringLiteral("url")).isString() || entity.value(QStringLiteral("url")).toString().size() > 2048)) {
			return false;
		}
		if (type == QStringLiteral("pre") && (!entity.value(QStringLiteral("language")).isString() || entity.value(QStringLiteral("language")).toString().size() > 128)) {
			return false;
		}
	}
	return true;
}

bool Validate(const QJsonObject &root) {
	if (!Keys(root, { QStringLiteral("v"), QStringLiteral("account"), QStringLiteral("folders"), QStringLiteral("order") })
		|| !root.value(QStringLiteral("v")).isDouble()
		|| root.value(QStringLiteral("v")).toDouble() != 1
		|| !Decimal(root.value(QStringLiteral("account")), false)
		|| !root.value(QStringLiteral("folders")).isArray()) {
		return false;
	}
	const auto folders = root.value(QStringLiteral("folders")).toArray();
	if (folders.size() > 2048) {
		return false;
	}
	auto seen = QSet<QString>();
	for (const auto &value : folders) {
		const auto folder = value.toObject();
		const auto id = folder.value(QStringLiteral("id"));
		if (!value.isObject()
			|| !Keys(folder, { QStringLiteral("id"), QStringLiteral("fields"), QStringLiteral("deleted") })
			|| !Uuid(id) || seen.contains(id.toString())
			|| !folder.value(QStringLiteral("fields")).isObject()) {
			return false;
		}
		seen.insert(id.toString());
		const auto fields = folder.value(QStringLiteral("fields")).toObject();
		if (!Keys(fields, Fields())) {
			return false;
		}
		for (const auto &name : Fields()) {
			if (fields.contains(name)
				? (!Register(fields.value(name)) || !Definition(name, fields.value(name).toObject().value(QStringLiteral("value"))))
				: !folder.contains(QStringLiteral("deleted"))) {
				return false;
			}
		}
		if (folder.contains(QStringLiteral("deleted"))) {
			const auto deleted = folder.value(QStringLiteral("deleted"));
			if (!Register(deleted) || deleted.toObject().value(QStringLiteral("value")) != QJsonValue(true)) {
				return false;
			}
		}
	}
	if (root.contains(QStringLiteral("order"))) {
		const auto order = root.value(QStringLiteral("order"));
		const auto value = order.toObject().value(QStringLiteral("value"));
		if (!Register(order) || !value.isArray() || value.toArray().size() > 4096) {
			return false;
		}
		seen.clear();
		for (const auto &entry : value.toArray()) {
			const auto id = entry.toString();
			if (!entry.isString() || seen.contains(id)) {
				return false;
			}
			seen.insert(id);
			if (id.startsWith(QStringLiteral("local:"))) {
				if (!Uuid(id.mid(6))) {
					return false;
				}
			} else if (!id.startsWith(QStringLiteral("remote:")) || !Decimal(id.mid(7), true)) {
				return false;
			}
		}
	}
	return true;
}

QJsonValue Winner(const QJsonValue &a, const QJsonValue &b) {
	if (a.isUndefined()) {
		return b;
	} else if (b.isUndefined()) {
		return a;
	}
	const auto left = a.toObject(), right = b.toObject();
	const auto ac = *Decimal(left.value(QStringLiteral("clock")), true);
	const auto bc = *Decimal(right.value(QStringLiteral("clock")), true);
	if (ac != bc) {
		return ac > bc ? a : b;
	}
	const auto ad = left.value(QStringLiteral("device")).toString();
	const auto bd = right.value(QStringLiteral("device")).toString();
	if (ad != bd) {
		return ad > bd ? a : b;
	}
	const auto av = QString::fromUtf8(*Canonical(left.value(QStringLiteral("value"))));
	const auto bv = QString::fromUtf8(*Canonical(right.value(QStringLiteral("value"))));
	return av >= bv ? a : b;
}

QJsonObject Normalize(QJsonObject document) {
	auto sorted = QMap<QString, QJsonValue>();
	for (const auto &value : document.value(QStringLiteral("folders")).toArray()) {
		sorted.insert(value.toObject().value(QStringLiteral("id")).toString(), value);
	}
	auto folders = QJsonArray();
	for (const auto &value : sorted) {
		folders.append(value);
	}
	document.insert(QStringLiteral("folders"), folders);
	return document;
}

}

std::optional<QByteArray> Encode(QJsonObject document) {
	if (!Validate(document)) {
		return std::nullopt;
	}
	const auto result = Canonical(Normalize(std::move(document)));
	return result && result->size() <= MaxBytes ? result : std::nullopt;
}

std::optional<QByteArray> CanonicalJson(const QJsonObject &value, int maxBytes) {
	const auto result = Canonical(value);
	return result && result->size() <= maxBytes ? result : std::nullopt;
}

std::optional<QJsonObject> ParseCanonicalJson(const QByteArray &bytes, int maxBytes) {
	if (bytes.size() > maxBytes) {
		return std::nullopt;
	}
	auto depth = 0;
	auto string = false;
	auto escape = false;
	for (const auto c : bytes) {
		if (string) {
			if (escape) escape = false;
			else if (c == '\\') escape = true;
			else if (c == '"') string = false;
		} else if (c == '"') {
			string = true;
		} else if ((c == '{' || c == '[') && ++depth > 24) {
			return std::nullopt;
		} else if (c == '}' || c == ']') {
			--depth;
		}
	}
	const auto parsed = QJsonDocument::fromJson(bytes);
	if (!parsed.isObject()) {
		return std::nullopt;
	}
	const auto root = parsed.object();
	const auto encoded = CanonicalJson(root, maxBytes);
	return encoded && *encoded == bytes ? std::make_optional(root) : std::nullopt;
}

std::optional<QJsonObject> Decode(const QByteArray &bytes, qint64 account) {
	const auto root = ParseCanonicalJson(bytes, MaxBytes);
	if (!root || account <= 0) {
		return std::nullopt;
	}
	const auto encoded = Encode(*root);
	if (!encoded || *encoded != bytes || root->value(QStringLiteral("account")) != QJsonValue(QString::number(account))) {
		return std::nullopt;
	}
	return root;
}

std::optional<QJsonObject> Merge(const QJsonObject &a, const QJsonObject &b) {
	if (!Encode(a) || !Encode(b) || a.value(QStringLiteral("account")) != b.value(QStringLiteral("account"))) {
		return std::nullopt;
	}
	auto all = QMap<QString, QJsonObject>();
	for (const auto &root : { a, b }) {
		for (const auto &value : root.value(QStringLiteral("folders")).toArray()) {
			const auto folder = value.toObject();
			const auto id = folder.value(QStringLiteral("id")).toString();
			if (!all.contains(id)) {
				all.insert(id, folder);
				continue;
			}
			auto combined = all.value(id);
			auto fields = combined.value(QStringLiteral("fields")).toObject();
			const auto other = folder.value(QStringLiteral("fields")).toObject();
			for (const auto &name : Fields()) {
				const auto winner = Winner(fields.value(name), other.value(name));
				if (!winner.isUndefined()) {
					fields.insert(name, winner);
				}
			}
			combined.insert(QStringLiteral("fields"), fields);
			const auto deleted = Winner(combined.value(QStringLiteral("deleted")), folder.value(QStringLiteral("deleted")));
			if (!deleted.isUndefined()) {
				combined.insert(QStringLiteral("deleted"), deleted);
			}
			all.insert(id, combined);
		}
	}
	auto result = a;
	auto folders = QJsonArray();
	for (const auto &folder : all) {
		folders.append(folder);
	}
	result.insert(QStringLiteral("folders"), folders);
	const auto order = Winner(a.value(QStringLiteral("order")), b.value(QStringLiteral("order")));
	if (!order.isUndefined()) {
		result.insert(QStringLiteral("order"), order);
	}
	return Encode(result) ? std::make_optional(result) : std::nullopt;
}

QStringList VisibleOrder(const QJsonObject &document) {
	if (!Encode(document)) {
		return {};
	}
	auto remaining = QSet<QString>();
	for (const auto &value : document.value(QStringLiteral("folders")).toArray()) {
		const auto folder = value.toObject();
		if (!folder.contains(QStringLiteral("deleted"))) {
			remaining.insert(QStringLiteral("local:") + folder.value(QStringLiteral("id")).toString());
		}
	}
	auto result = QStringList();
	for (const auto &entry : document.value(QStringLiteral("order")).toObject().value(QStringLiteral("value")).toArray()) {
		const auto id = entry.toString();
		if (id.startsWith(QStringLiteral("remote:")) || remaining.remove(id)) {
			result.push_back(id);
		}
	}
	auto appended = remaining.values();
	appended.sort();
	result.append(appended);
	return result;
}

}
