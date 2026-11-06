#include "ax/features/folders/folder_filter_codec.h"

#include "ax/features/folders/folder_document.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/history.h"
#include "main/main_session.h"
#include <QtCore/QJsonArray>
#include <algorithm>
#include <limits>

namespace AxFolders {
namespace {

using Flag = Data::ChatFilter::Flag;
constexpr std::pair<const char*, Flag> Rules[] = {
	{ "contacts", Flag::Contacts }, { "nonContacts", Flag::NonContacts },
	{ "groups", Flag::Groups }, { "channels", Flag::Channels }, { "bots", Flag::Bots },
	{ "excludeMuted", Flag::NoMuted }, { "excludeRead", Flag::NoRead }, { "excludeArchived", Flag::NoArchived },
	{ "onlyArchived", Flag::OnlyArchived },
};
constexpr std::pair<EntityType, const char*> Entities[] = {
	{ EntityType::Bold, "bold" }, { EntityType::Italic, "italic" }, { EntityType::Underline, "underline" },
	{ EntityType::StrikeOut, "strike" }, { EntityType::Spoiler, "spoiler" }, { EntityType::Code, "code" },
	{ EntityType::Pre, "pre" }, { EntityType::Url, "url" }, { EntityType::CustomUrl, "textUrl" },
	{ EntityType::Mention, "mention" }, { EntityType::MentionName, "mentionName" },
	{ EntityType::CustomEmoji, "customEmoji" }, { EntityType::Hashtag, "hashtag" },
	{ EntityType::Cashtag, "cashtag" }, { EntityType::BotCommand, "botCommand" },
	{ EntityType::Email, "email" }, { EntityType::Phone, "phone" }, { EntityType::BankCard, "bankCard" },
};

bool Validate(const QJsonObject &values) {
	const auto uuid = QStringLiteral("00000000-0000-0000-0000-000000000001");
	auto fields = QJsonObject();
	for (auto i = values.begin(); i != values.end(); ++i) {
		fields.insert(i.key(), QJsonObject{
			{ QStringLiteral("clock"), QStringLiteral("0") },
			{ QStringLiteral("device"), uuid },
			{ QStringLiteral("value"), i.value() },
		});
	}
	return Encode({
		{ QStringLiteral("v"), 1 },
		{ QStringLiteral("account"), QStringLiteral("1") },
		{ QStringLiteral("folders"), QJsonArray{ QJsonObject{
			{ QStringLiteral("id"), uuid },
			{ QStringLiteral("fields"), fields },
		} } },
	}).has_value();
}

std::optional<QJsonObject> Peer(not_null<History*> history) {
	const auto id = history->peer->id;
	const auto bare = id.value & PeerId::kChatTypeMask;
	const auto type = peerIsUser(id) ? QStringLiteral("user") : peerIsChat(id)
		? QStringLiteral("chat") : peerIsChannel(id) ? QStringLiteral("channel") : QString();
	if (!bare || type.isEmpty()) {
		return std::nullopt;
	}
	return QJsonObject{ { QStringLiteral("type"), type }, { QStringLiteral("id"), QString::number(bare) } };
}

template <typename Range>
std::optional<QJsonArray> Peers(const Range &range, bool sorted) {
	auto entries = std::vector<std::pair<QByteArray, QJsonObject>>();
	for (const auto history : range) {
		const auto peer = Peer(history);
		if (!peer) {
			return std::nullopt;
		}
		entries.emplace_back(*CanonicalJson(*peer, 1024), *peer);
	}
	if (sorted) {
		std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	}
	auto result = QJsonArray();
	for (const auto &entry : entries) {
		result.push_back(entry.second);
	}
	return result;
}

std::optional<QJsonObject> Entity(const EntityInText &entity) {
	auto type = QString();
	for (const auto &[native, wire] : Entities) {
		if (entity.type() == native) {
			type = QString::fromLatin1(wire);
			break;
		}
	}
	if (entity.type() == EntityType::Blockquote) {
		type = entity.data() == QStringLiteral("1") ? QStringLiteral("expandableBlockquote") : QStringLiteral("blockquote");
	}
	if (type.isEmpty()) {
		return std::nullopt;
	}
	auto result = QJsonObject{
		{ QStringLiteral("type"), type },
		{ QStringLiteral("offset"), entity.offset() },
		{ QStringLiteral("length"), entity.length() },
	};
	if (entity.type() == EntityType::CustomEmoji) {
		const auto id = Data::ParseCustomEmojiData(entity.data());
		if (!id || id > std::numeric_limits<qint64>::max()) {
			return std::nullopt;
		}
		result.insert(QStringLiteral("id"), QString::number(id));
	} else if (entity.type() == EntityType::MentionName) {
		const auto id = TextUtilities::MentionNameDataToFields(entity.data()).userId;
		result.insert(QStringLiteral("id"), QString::number(id));
	} else if (entity.type() == EntityType::CustomUrl) {
		result.insert(QStringLiteral("url"), entity.data());
	} else if (entity.type() == EntityType::Pre) {
		result.insert(QStringLiteral("language"), entity.data());
	}
	return result;
}

std::optional<History*> HistoryFromPeer(not_null<Data::Session*> owner, const QJsonObject &peer) {
	const auto id = peer.value(QStringLiteral("id")).toString().toULongLong();
	if (!id || id > PeerId::kChatTypeMask) {
		return std::nullopt;
	}
	const auto type = peer.value(QStringLiteral("type")).toString();
	return owner->history(type == QStringLiteral("user") ? PeerId(UserId(id))
		: type == QStringLiteral("chat") ? PeerId(ChatId(id)) : PeerId(ChannelId(id))).get();
}

std::optional<EntitiesInText> ParseEntities(not_null<Data::Session*> owner, const QJsonArray &array) {
	auto result = EntitiesInText();
	for (const auto &entry : array) {
		const auto object = entry.toObject();
		const auto type = object.value(QStringLiteral("type")).toString();
		auto native = EntityType::Invalid;
		for (const auto &[value, wire] : Entities) {
			if (type == QString::fromLatin1(wire)) {
				native = value;
				break;
			}
		}
		auto data = QString();
		if (type == QStringLiteral("blockquote") || type == QStringLiteral("expandableBlockquote")) {
			native = EntityType::Blockquote;
			if (type == QStringLiteral("expandableBlockquote")) data = QStringLiteral("1");
		} else if (native == EntityType::CustomEmoji) {
			data = Data::SerializeCustomEmojiId(object.value(QStringLiteral("id")).toString().toULongLong());
		} else if (native == EntityType::MentionName) {
			data = TextUtilities::MentionNameDataFromFields({
				.selfId = owner->session().userId().bare,
				.userId = object.value(QStringLiteral("id")).toString().toULongLong(),
				.accessHash = 0,
			});
		} else if (native == EntityType::CustomUrl) {
			data = object.value(QStringLiteral("url")).toString();
		} else if (native == EntityType::Pre) {
			data = object.value(QStringLiteral("language")).toString();
		}
		if (native == EntityType::Invalid) {
			return std::nullopt;
		}
		result.push_back(EntityInText(native, object.value(QStringLiteral("offset")).toInt(), object.value(QStringLiteral("length")).toInt(), data));
	}
	return result;
}

}

std::optional<QJsonObject> ExportFilter(const Data::ChatFilter &filter) {
	if (filter.id() < 1000 || (filter.flags() & ~(Flag::RulesMask | Flag::StaticTitle))) {
		return std::nullopt;
	}
	const auto include = Peers(filter.always(), true), exclude = Peers(filter.never(), true), pinned = Peers(filter.pinned(), false);
	if (!include || !exclude || !pinned) {
		return std::nullopt;
	}
	auto entities = QJsonArray();
	for (const auto &entity : filter.titleText().entities) {
		const auto encoded = Entity(entity);
		if (!encoded) {
			return std::nullopt;
		}
		entities.push_back(*encoded);
	}
	auto rules = QJsonObject{
		{ QStringLiteral("include"), *include }, { QStringLiteral("exclude"), *exclude },
	};
	for (const auto &[name, flag] : Rules) {
		rules.insert(QString::fromLatin1(name), bool(filter.flags() & flag));
	}
	auto result = QJsonObject{
		{ QStringLiteral("title"), QJsonObject{
			{ QStringLiteral("text"), filter.titleText().text },
			{ QStringLiteral("noAnimate"), filter.staticTitle() },
			{ QStringLiteral("entities"), entities },
		} },
		{ QStringLiteral("appearance"), QJsonObject{
			{ QStringLiteral("emoji"), filter.iconEmoji() },
			{ QStringLiteral("color"), filter.colorIndex() ? QJsonValue(int(*filter.colorIndex())) : QJsonValue(QJsonValue::Null) },
		} },
		{ QStringLiteral("rules"), rules },
		{ QStringLiteral("pinned"), *pinned },
	};
	return Validate(result) ? std::make_optional(result) : std::nullopt;
}

std::optional<Data::ChatFilter> ImportFilter(not_null<Data::Session*> owner, FilterId localId, const QJsonObject &values) {
	if (localId < 1000 || !Validate(values)) {
		return std::nullopt;
	}
	const auto rules = values.value(QStringLiteral("rules")).toObject();
	auto flags = Data::ChatFilter::Flags();
	for (const auto &[name, flag] : Rules) {
		if (rules.value(QString::fromLatin1(name)).toBool()) flags |= flag;
	}
	auto always = base::flat_set<not_null<History*>>();
	auto never = base::flat_set<not_null<History*>>();
	auto pinned = std::vector<not_null<History*>>();
	const auto fill = [&](const QJsonArray &array, auto callback) {
		for (const auto &entry : array) {
			const auto history = HistoryFromPeer(owner, entry.toObject());
			if (!history) return false;
			callback(not_null<History*>(*history));
		}
		return true;
	};
	if (!fill(rules.value(QStringLiteral("include")).toArray(), [&](auto history) { always.insert(history); })
		|| !fill(rules.value(QStringLiteral("exclude")).toArray(), [&](auto history) { never.insert(history); })
		|| !fill(values.value(QStringLiteral("pinned")).toArray(), [&](auto history) { pinned.push_back(history); always.insert(history); })) {
		return std::nullopt;
	}
	const auto title = values.value(QStringLiteral("title")).toObject();
	const auto entities = ParseEntities(owner, title.value(QStringLiteral("entities")).toArray());
	if (!entities) {
		return std::nullopt;
	}
	const auto appearance = values.value(QStringLiteral("appearance")).toObject();
	const auto color = appearance.value(QStringLiteral("color"));
	return Data::ChatFilter(localId,
		{ { title.value(QStringLiteral("text")).toString(), *entities }, title.value(QStringLiteral("noAnimate")).toBool() },
		appearance.value(QStringLiteral("emoji")).toString(),
		color.isNull() ? std::nullopt : std::make_optional(uint8(color.toInt())), flags,
		std::move(always), std::move(pinned), std::move(never));
}

}
