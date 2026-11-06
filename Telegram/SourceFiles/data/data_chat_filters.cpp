/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_chat_filters.h"
#include "ax/features/folders/folder_archive_rules.h"
#include "ax/features/folders/folder_filter_codec.h"
#include "ax/features/folders/folder_runtime.h"
#include "ax/features/folders/folder_connection.h"
#include "boxes/url_auth_box.h"

#include "api/api_text_entities.h"
#include "history/history.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "dialogs/dialogs_main_list.h"
#include "history/history.h"
#include "history/history_unread_things.h"
#include "ui/ui_utility.h"
#include "ui/chat/more_chats_bar.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "main/main_app_config.h"
#include "apiwrap.h"

// AxGram includes
#include "ax/ax_settings.h"
#include <QtCore/QUuid>


namespace Data {
namespace {

constexpr auto kRefreshSuggestedTimeout = 7200 * crl::time(1000);
constexpr auto kLoadExceptionsAfter = 100;
constexpr auto kLoadExceptionsPerRequest = 100;

[[nodiscard]] QString LocalFoldersRoot() {
	return cWorkingDir() + u"tdata/ax_folders"_q;
}

void OrderLocalFolders(
		std::vector<ChatFilter> &filters,
		const AxFolders::Replica &replica) {
	auto existing = std::vector<int>();
	for (const auto &filter : filters) existing.push_back(filter.id());
	const auto order = AxFolders::NativeOrder(replica, existing);
	if (!order) return;
	auto position = filters.begin();
	for (const auto id : *order) {
		const auto found = ranges::find(position, filters.end(), id, &ChatFilter::id);
		if (found != filters.end()) {
			std::iter_swap(position, found);
			++position;
		}
	}
}

[[nodiscard]] crl::time RequestUpdatesEach(not_null<Session*> owner) {
	const auto appConfig = &owner->session().appConfig();
	return appConfig->get<int>(u"chatlist_update_period"_q, 3600)
		* crl::time(1000);
}

} // namespace

TextWithEntities ForceCustomEmojiStatic(TextWithEntities text) {
	for (auto &entity : text.entities) {
		if (entity.type() == EntityType::CustomEmoji) {
			entity = EntityInText(
				EntityType::CustomEmoji,
				entity.offset(),
				entity.length(),
				u"force-static:"_q + entity.data());
		}
	}
	return text;
}

ChatFilter::ChatFilter(
	FilterId id,
	ChatFilterTitle title,
	QString iconEmoji,
	std::optional<uint8> colorIndex,
	Flags flags,
	base::flat_set<not_null<History*>> always,
	std::vector<not_null<History*>> pinned,
	base::flat_set<not_null<History*>> never)
: _id(id)
, _title(std::move(title.text))
, _iconEmoji(std::move(iconEmoji))
, _colorIndex(colorIndex)
, _always(std::move(always))
, _pinned(std::move(pinned))
, _never(std::move(never))
, _flags(title.isStatic
	? (flags | Flag::StaticTitle)
	: (flags & ~Flag::StaticTitle)) {
}

ChatFilter ChatFilter::FromTL(
		const MTPDialogFilter &data,
		not_null<Session*> owner) {
	return data.match([&](const MTPDdialogFilter &data) {
		const auto flags = (data.is_contacts() ? Flag::Contacts : Flag(0))
			| (data.is_non_contacts() ? Flag::NonContacts : Flag(0))
			| (data.is_groups() ? Flag::Groups : Flag(0))
			| (data.is_broadcasts() ? Flag::Channels : Flag(0))
			| (data.is_bots() ? Flag::Bots : Flag(0))
			| (data.is_exclude_muted() ? Flag::NoMuted : Flag(0))
			| (data.is_exclude_read() ? Flag::NoRead : Flag(0))
			| (data.is_exclude_archived() ? Flag::NoArchived : Flag(0))
			| (data.is_title_noanimate() ? Flag::StaticTitle : Flag(0));
		auto &&to_histories = ranges::views::transform([&](
				const MTPInputPeer &input) {
			const auto peer = Data::PeerFromInputMTP(owner, input);
			return peer ? owner->history(peer).get() : nullptr;
		}) | ranges::views::filter([](History *history) {
			return history != nullptr;
		}) | ranges::views::transform([](History *history) {
			return not_null<History*>(history);
		});
		auto &&always = ranges::views::concat(
			data.vinclude_peers().v
		) | to_histories;
		auto pinned = ranges::views::all(
			data.vpinned_peers().v
		) | to_histories | ranges::to_vector;
		auto &&never = ranges::views::all(
			data.vexclude_peers().v
		) | to_histories;
		auto &&all = ranges::views::concat(always, pinned);
		auto list = base::flat_set<not_null<History*>>{
			all.begin(),
			all.end()
		};
		return ChatFilter(
			data.vid().v,
			{
				Api::ParseTextWithEntities(&owner->session(), data.vtitle()),
				data.is_title_noanimate(),
			},
			qs(data.vemoticon().value_or_empty()),
			data.vcolor()
				? std::make_optional(data.vcolor()->v)
				: std::nullopt,
			flags,
			std::move(list),
			std::move(pinned),
			{ never.begin(), never.end() });
	}, [](const MTPDdialogFilterDefault &) {
		return ChatFilter();
	}, [&](const MTPDdialogFilterChatlist &data) {
		auto &&to_histories = ranges::views::transform([&](
				const MTPInputPeer &data) {
			const auto peer = data.match([&](const MTPDinputPeerUser &data) {
				const auto user = owner->user(data.vuser_id().v);
				user->setAccessHash(data.vaccess_hash().v);
				return (PeerData*)user;
			}, [&](const MTPDinputPeerChat &data) {
				return (PeerData*)owner->chat(data.vchat_id().v);
			}, [&](const MTPDinputPeerChannel &data) {
				const auto channel = owner->channel(data.vchannel_id().v);
				channel->setAccessHash(data.vaccess_hash().v);
				return (PeerData*)channel;
			}, [&](const MTPDinputPeerSelf &data) {
				return (PeerData*)owner->session().user();
			}, [&](const auto &data) {
				return (PeerData*)nullptr;
			});
			return peer ? owner->history(peer).get() : nullptr;
		}) | ranges::views::filter([](History *history) {
			return history != nullptr;
		}) | ranges::views::transform([](History *history) {
			return not_null<History*>(history);
		});
		auto &&always = ranges::views::concat(
			data.vinclude_peers().v
		) | to_histories;
		auto pinned = ranges::views::all(
			data.vpinned_peers().v
		) | to_histories | ranges::to_vector;
		auto &&all = ranges::views::concat(always, pinned);
		auto list = base::flat_set<not_null<History*>>{
			all.begin(),
			all.end()
		};
		return ChatFilter(
			data.vid().v,
			{
				Api::ParseTextWithEntities(&owner->session(), data.vtitle()),
				data.is_title_noanimate(),
			},
			qs(data.vemoticon().value_or_empty()),
			data.vcolor()
				? std::make_optional(data.vcolor()->v)
				: std::nullopt,
			(Flag::Chatlist
				| (data.is_has_my_invites() ? Flag::HasMyLinks : Flag())
				| (data.is_title_noanimate() ? Flag::StaticTitle : Flag(0))),
			std::move(list),
			std::move(pinned),
			{});
	});
}

ChatFilter ChatFilter::withId(FilterId id) const {
	auto result = *this;
	result._id = id;
	return result;
}

ChatFilter ChatFilter::withTitle(ChatFilterTitle title) const {
	auto result = *this;
	result._title = std::move(title.text);
	if (title.isStatic) {
		result._flags |= Flag::StaticTitle;
	} else {
		result._flags &= ~Flag::StaticTitle;
	}
	return result;
}

ChatFilter ChatFilter::withColorIndex(std::optional<uint8> c) const {
	auto result = *this;
	result._colorIndex = c;
	return result;
}

ChatFilter ChatFilter::withChatlist(bool chatlist, bool hasMyLinks) const {
	auto result = *this;
	result._flags &= Flag::RulesMask;
	if (chatlist) {
		result._flags |= Flag::Chatlist;
		if (hasMyLinks) {
			result._flags |= Flag::HasMyLinks;
		} else {
			result._flags &= ~Flag::HasMyLinks;
		}
	}
	return result;
}

ChatFilter ChatFilter::withoutAlways(not_null<History*> history) const {
	auto result = *this;
	if (CanRemoveFromChatFilter(result, history)) {
		result._always.remove(history);
	}
	return result;
}

MTPDialogFilter ChatFilter::tl(FilterId replaceId) const {
	auto always = _always;
	auto pinned = QVector<MTPInputPeer>();
	pinned.reserve(_pinned.size());
	for (const auto &history : _pinned) {
		pinned.push_back(history->peer->input());
		always.remove(history);
	}
	auto include = QVector<MTPInputPeer>();
	include.reserve(always.size());
	for (const auto &history : always) {
		include.push_back(history->peer->input());
	}
	auto title = MTP_textWithEntities(
		MTP_string(_title.text),
		Api::EntitiesToMTP(
			nullptr,
			_title.entities,
			Api::ConvertOption::SkipLocal));
	if (_flags & Flag::Chatlist) {
		using TLFlag = MTPDdialogFilterChatlist::Flag;
		const auto flags = TLFlag::f_emoticon
			| (_colorIndex ? TLFlag::f_color : TLFlag(0))
			| (staticTitle() ? TLFlag::f_title_noanimate : TLFlag(0));
		return MTP_dialogFilterChatlist(
			MTP_flags(flags),
			MTP_int(replaceId ? replaceId : _id),
			std::move(title),
			MTP_string(_iconEmoji),
			MTP_int(_colorIndex.value_or(0)),
			MTP_vector<MTPInputPeer>(pinned),
			MTP_vector<MTPInputPeer>(include));
	}
	using TLFlag = MTPDdialogFilter::Flag;
	const auto flags = TLFlag::f_emoticon
		| (_colorIndex ? TLFlag::f_color : TLFlag(0))
		| (staticTitle() ? TLFlag::f_title_noanimate : TLFlag(0))
		| ((_flags & Flag::Contacts) ? TLFlag::f_contacts : TLFlag(0))
		| ((_flags & Flag::NonContacts) ? TLFlag::f_non_contacts : TLFlag(0))
		| ((_flags & Flag::Groups) ? TLFlag::f_groups : TLFlag(0))
		| ((_flags & Flag::Channels) ? TLFlag::f_broadcasts : TLFlag(0))
		| ((_flags & Flag::Bots) ? TLFlag::f_bots : TLFlag(0))
		| ((_flags & Flag::NoMuted) ? TLFlag::f_exclude_muted : TLFlag(0))
		| ((_flags & Flag::NoRead) ? TLFlag::f_exclude_read : TLFlag(0))
		| ((_flags & Flag::NoArchived)
			? TLFlag::f_exclude_archived
			: TLFlag(0));
	auto never = QVector<MTPInputPeer>();
	never.reserve(_never.size());
	for (const auto &history : _never) {
		never.push_back(history->peer->input());
	}
	return MTP_dialogFilter(
		MTP_flags(flags),
		MTP_int(replaceId ? replaceId : _id),
		std::move(title),
		MTP_string(_iconEmoji),
		MTP_int(_colorIndex.value_or(0)),
		MTP_vector<MTPInputPeer>(pinned),
		MTP_vector<MTPInputPeer>(include),
		MTP_vector<MTPInputPeer>(never));
}

FilterId ChatFilter::id() const {
	return _id;
}

const TextWithEntities &ChatFilter::titleText() const {
	return _title;
}

ChatFilterTitle ChatFilter::title() const {
	return { _title, !!(_flags & Flag::StaticTitle) };
}

QString ChatFilter::iconEmoji() const {
	return _iconEmoji;
}

std::optional<uint8> ChatFilter::colorIndex() const {
	return _colorIndex;
}

ChatFilter::Flags ChatFilter::flags() const {
	return _flags;
}

bool ChatFilter::staticTitle() const {
	return _flags & Flag::StaticTitle;
}

bool ChatFilter::chatlist() const {
	return _flags & Flag::Chatlist;
}

bool ChatFilter::hasMyLinks() const {
	return _flags & Flag::HasMyLinks;
}

const base::flat_set<not_null<History*>> &ChatFilter::always() const {
	return _always;
}

const std::vector<not_null<History*>> &ChatFilter::pinned() const {
	return _pinned;
}

const base::flat_set<not_null<History*>> &ChatFilter::never() const {
	return _never;
}

bool ChatFilter::contains(
		not_null<History*> history,
		bool ignoreFakeUnread) const {
	const auto flag = [&] {
		const auto peer = history->peer;
		if (const auto user = peer->asUser()) {
			return user->isBot()
				? Flag::Bots
				: user->isContact()
				? Flag::Contacts
				: Flag::NonContacts;
		} else if (peer->isChat()) {
			return Flag::Groups;
		} else if (const auto channel = peer->asChannel()) {
			if (channel->isBroadcast()) {
				return Flag::Channels;
			} else {
				return Flag::Groups;
			}
		} else {
			Unexpected("Peer type in ChatFilter::contains.");
		}
	}();
	if (_never.contains(history)) {
		return false;
	}
	const auto channel = history->peer->asChannel();
	if (channel && channel->isCommunity()) {
		// A community never matches a filter by chat type (it is neither a
		// group nor a channel); it can only be included explicitly by id.
		return _always.contains(history);
	}
	const auto state = (_flags & (Flag::NoMuted | Flag::NoRead))
		? history->chatListBadgesState()
		: Dialogs::BadgesState();
	return false
		|| ((_flags & flag)
			&& (!(_flags & Flag::NoMuted)
				|| !history->muted()
				|| (state.mention
					&& history->folderKnown()
					&& !history->folder()))
			&& (!(_flags & Flag::NoRead)
				|| state.unread
				|| state.mention
				|| (!ignoreFakeUnread && history->fakeUnreadWhileOpened()))
			&& AxFolders::MatchesArchiveRule(
				bool(_flags & Flag::OnlyArchived),
				bool(_flags & Flag::NoArchived),
				history->folderKnown(),
				history->folder() != nullptr))
		|| _always.contains(history);
}

ChatFilters::ChatFilters(not_null<Session*> owner)
: _owner(owner)
, _moreChatsTimer([=] { checkLoadMoreChatsLists(); }) {
	_list.emplace_back();
	crl::on_main(&owner->session(), [=] { load(); });

	AxSettings::getInstance().hideAllChatsFolderChanges()
	| rpl::on_next([=](bool hide) {
		if (!_loaded) {
			return;
		}
		if (hide) {
			if (_list.size() <= 1) {
				return;
			}
			const auto it = ranges::find(_list, FilterId(0), &ChatFilter::id);
			if (it != end(_list)) {
				_list.erase(it);
				_listChanged.fire({});
			}
		} else {
			if (!ranges::contains(_list, FilterId(0), &ChatFilter::id)) {
				_list.insert(begin(_list), ChatFilter());
				_listChanged.fire({});
			}
		}
	}, _lifetime);
}

ChatFilters::~ChatFilters() {
	_localConnection.reset();
	_localSync.reset();
}

not_null<Dialogs::MainList*> ChatFilters::chatsList(FilterId filterId) {
	auto &pointer = _chatsLists[filterId];
	if (!pointer) {
		auto limit = rpl::single(rpl::empty_value()) | rpl::then(
			_owner->session().appConfig().refreshed()
		) | rpl::map([=] {
			return _owner->pinnedChatsLimit(filterId);
		});
		pointer = std::make_unique<Dialogs::MainList>(
			&_owner->session(),
			filterId,
			_owner->maxPinnedChatsLimitValue(filterId));
	}
	return pointer.get();
}

void ChatFilters::clear() {
	_chatsLists.clear();
	_list.clear();
}

void ChatFilters::setPreloaded(
		const QVector<MTPDialogFilter> &result,
		bool tagsEnabled) {
	_loadRequestId = -1;
	_tagsEnabled = tagsEnabled;
	received(result);
	crl::on_main(&_owner->session(), [=] {
		if (_loadRequestId == -1) {
			_loadRequestId = 0;
		}
	});
}

void ChatFilters::load() {
	load(false);
}

void ChatFilters::reload() {
	_reloading = true;
	load();
}

void ChatFilters::load(bool force) {
	if (_loadRequestId && !force) {
		return;
	}
	auto &api = _owner->session().api();
	api.request(_loadRequestId).cancel();
	_loadRequestId = api.request(MTPmessages_GetDialogFilters(
	)).done([=](const MTPmessages_DialogFilters &result) {
		_tagsEnabled = result.data().is_tags_enabled();
		received(result.data().vfilters().v);
		_loadRequestId = 0;
	}).fail([=] {
		_loadRequestId = 0;
		if (_reloading) {
			_reloading = false;
			_listChanged.fire({});
		}
	}).send();
}

bool ChatFilters::tagsEnabled() const {
	return _tagsEnabled.current();
}

rpl::producer<bool> ChatFilters::tagsEnabledValue() const {
	return _tagsEnabled.value();
}

rpl::producer<bool> ChatFilters::tagsEnabledChanges() const {
	return _tagsEnabled.changes();
}

void ChatFilters::requestToggleTags(bool value, Fn<void()> fail) {
	if (_toggleTagsRequestId) {
		return;
	}
	_toggleTagsRequestId = _owner->session().api().request(
		MTPmessages_ToggleDialogFilterTags(MTP_bool(value))
	).done([=](const MTPBool &result) {
		_tagsEnabled = value;
		_toggleTagsRequestId = 0;
	}).fail([=](const MTP::Error &error) {
		const auto message = error.type();
		_toggleTagsRequestId = 0;
		LOG(("API Error: Toggle Tags - %1").arg(message));
		fail();
	}).send();
}

void ChatFilters::received(const QVector<MTPDialogFilter> &list) {
	// AxGram hideAllChatsFolder
	const auto &settings = AxSettings::getInstance();
	loadLocalReplica();
	auto incoming = std::vector<ChatFilter>();
	for (const auto &filter : list) {
		incoming.push_back(ChatFilter::FromTL(filter, _owner));
	}
	if (_localReplica) {
		auto restored = std::vector<ChatFilter>();
		auto complete = true;
		for (const auto &local : AxFolders::LocalFolders(*_localReplica)) {
			const auto filter = AxFolders::ImportFilter(_owner, local.id, local.values);
			if (!filter) {
				complete = false;
				break;
			}
			restored.push_back(*filter);
		}
		if (complete) {
			incoming.insert(incoming.end(), restored.begin(), restored.end());
			OrderLocalFolders(incoming, *_localReplica);
		} else {
			LOG(("AxGram: local folders use unsupported settings."));
			for (const auto &filter : _list) {
				if (filter.id() >= 1000) incoming.push_back(filter);
			}
		}
	}

	auto position = 0;
	auto changed = false;
	for (auto &parsed : incoming) {
		if (settings.hideAllChatsFolder() && parsed.id() == 0 && incoming.size() > 1) {
			continue;
		}
		const auto b = begin(_list) + position;
		const auto e = end(_list);
		const auto i = ranges::find(b, e, parsed.id(), &ChatFilter::id);
		if (i == e) {
			applyInsert(std::move(parsed), position);
			changed = true;
		} else if (i == b) {
			if (applyChange(*b, std::move(parsed))) {
				changed = true;
			}
		} else {
			std::swap(*i, *b);
			applyChange(*b, std::move(parsed));
			changed = true;
		}
		++position;
	}
	while (position < _list.size()) {
		applyRemove(position);
		changed = true;
	}
	if (!settings.hideAllChatsFolder() && !ranges::contains(begin(_list), end(_list), 0, &ChatFilter::id)) {
		_list.insert(begin(_list), ChatFilter());
	}
	if (changed || !_loaded || _reloading) {
		_loaded = true;
		_reloading = false;
		ensureLocalConnection();
		_listChanged.fire({});
	}
}

void ChatFilters::apply(const MTPUpdate &update) {
	// AxGram hideAllChatsFolder
	const auto &settings = AxSettings::getInstance();

	update.match([&](const MTPDupdateDialogFilter &data) {
		if (const auto filter = data.vfilter()) {
			auto parsed = ChatFilter::FromTL(*filter, _owner);
			if (settings.hideAllChatsFolder() && parsed.id() == 0) {
				return;
			}
			set(parsed);
		} else {
			remove(data.vid().v);
		}
	}, [&](const MTPDupdateDialogFilters &data) {
		load(true);
	}, [&](const MTPDupdateDialogFilterOrder &data) {
		if (applyOrder(data.vorder().v)) {
			if (_localReplica && !_localReplica->ids.isEmpty()
				&& !saveLocalFilter(nullptr, -1)) {
				LOG(("AxGram: local folder order was not saved."));
			}
			_listChanged.fire({});
		} else {
			load(true);
		}
	}, [](auto&&) {
		Unexpected("Update in ChatFilters::apply.");
	});
}

ChatFilterLink ChatFilters::add(
		FilterId id,
		const MTPExportedChatlistInvite &update) {
	const auto i = ranges::find(_list, id, &ChatFilter::id);
	if (i == end(_list) || !i->chatlist()) {
		LOG(("Api Error: "
			"Attempt to add chatlist link to a non-chatlist filter: %1"
			).arg(id));
		return {};
	}
	auto &links = _chatlistLinks[id];
	const auto &data = update.data();
	const auto url = qs(data.vurl());
	const auto title = qs(data.vtitle());
	auto chats = data.vpeers().v | ranges::views::transform([&](
			const MTPPeer &peer) {
		return _owner->history(peerFromMTP(peer));
	}) | ranges::to_vector;
	const auto j = ranges::find(links, url, &ChatFilterLink::url);
	if (j != end(links)) {
		if (j->title != title || j->chats != chats) {
			j->title = title;
			j->chats = std::move(chats);
			_chatlistLinksUpdated.fire_copy(id);
		}
		return *j;
	}
	links.push_back({
		.id = id,
		.url = url,
		.title = title,
		.chats = std::move(chats),
	});
	_chatlistLinksUpdated.fire_copy(id);
	return links.back();
}

void ChatFilters::edit(
		FilterId id,
		const QString &url,
		const QString &title) {
	auto &links = _chatlistLinks[id];
	const auto i = ranges::find(links, url, &ChatFilterLink::url);
	if (i != end(links)) {
		i->title = title;
		_chatlistLinksUpdated.fire_copy(id);

		_owner->session().api().request(MTPchatlists_EditExportedInvite(
			MTP_flags(MTPchatlists_EditExportedInvite::Flag::f_title),
			MTP_inputChatlistDialogFilter(MTP_int(id)),
			MTP_string(url),
			MTP_string(title),
			MTPVector<MTPInputPeer>() // peers
		)).done([=](const MTPExportedChatlistInvite &result) {
			//const auto &data = result.data();
			//const auto link = _owner->chatsFilters().add(id, result);
			//done(link);
		}).fail([=](const MTP::Error &error) {
			//done({ .id = id });
		}).send();
	}
}

void ChatFilters::destroy(FilterId id, const QString &url) {
	auto &links = _chatlistLinks[id];
	const auto i = ranges::find(links, url, &ChatFilterLink::url);
	if (i != end(links)) {
		links.erase(i);
		_chatlistLinksUpdated.fire_copy(id);

		const auto api = &_owner->session().api();
		api->request(_linksRequestId).cancel();
		_linksRequestId = api->request(MTPchatlists_DeleteExportedInvite(
			MTP_inputChatlistDialogFilter(MTP_int(id)),
			MTP_string(url)
		)).send();
	}
}

rpl::producer<std::vector<ChatFilterLink>> ChatFilters::chatlistLinks(
		FilterId id) const {
	return _chatlistLinksUpdated.events_starting_with_copy(
		id
	) | rpl::filter(rpl::mappers::_1 == id) | rpl::map([=] {
		const auto i = _chatlistLinks.find(id);
		return (i != end(_chatlistLinks))
			? i->second
			: std::vector<ChatFilterLink>();
	});
}

void ChatFilters::reloadChatlistLinks(FilterId id) {
	const auto api = &_owner->session().api();
	api->request(_linksRequestId).cancel();
	_linksRequestId = api->request(MTPchatlists_GetExportedInvites(
		MTP_inputChatlistDialogFilter(MTP_int(id))
	)).done([=](const MTPchatlists_ExportedInvites &result) {
		const auto &data = result.data();
		_owner->processUsers(data.vusers());
		_owner->processChats(data.vchats());
		_chatlistLinks[id].clear();
		for (const auto &link : data.vinvites().v) {
			add(id, link);
		}
		_chatlistLinksUpdated.fire_copy(id);
	}).send();
}

void ChatFilters::set(ChatFilter filter) {
	if (!filter.id()) {
		return;
	}
	if (filter.id() >= 1000) {
		if (!saveLocalFilter(&filter, filter.id())) {
			LOG(("AxGram: local folder was not saved."));
		}
		return;
	}
	const auto i = ranges::find(_list, filter.id(), &ChatFilter::id);
	if (i == end(_list)) {
		applyInsert(std::move(filter), _list.size());
		_listChanged.fire({});
	} else if (applyChange(*i, std::move(filter))) {
		_listChanged.fire({});
	}
}

void ChatFilters::applyInsert(ChatFilter filter, int position) {
	Expects(position >= 0 && position <= _list.size());

	_list.insert(
		begin(_list) + position,
		ChatFilter(filter.id(), {}, {}, {}, {}, {}, {}, {}));
	applyChange(*(begin(_list) + position), std::move(filter));
}

void ChatFilters::remove(FilterId id) {
	if (id >= 1000) {
		if (!saveLocalFilter(nullptr, id)) {
			LOG(("AxGram: local folder was not removed."));
		}
		return;
	}
	const auto i = ranges::find(_list, id, &ChatFilter::id);
	if (i == end(_list)) {
		return;
	}
	applyRemove(i - begin(_list));
	_listChanged.fire({});
}

void ChatFilters::loadLocalReplica() {
	if (_localReplicaLoaded) return;
	_localReplicaLoaded = true;
	_localReplica = AxFolders::Load(LocalFoldersRoot(), _owner->session().userId().bare);
	if (!_localReplica) {
		LOG(("AxGram: local folder state could not be read."));
	}
}

std::optional<AxFolders::Replica> ChatFilters::localReplica() const {
	return _localReplica;
}

std::optional<ChatFilter> ChatFilters::createLocalFilter(
		const ChatFilter &draft) {
	if (!_loaded || !_localReplica) {
		return std::nullopt;
	}
	auto existing = std::vector<int>();
	for (const auto &filter : _list) {
		existing.push_back(filter.id());
	}
	const auto id = AxFolders::NextLocalId(*_localReplica, existing);
	if (!id) {
		return std::nullopt;
	}
	const auto created = draft.withId(*id);
	if (!saveLocalFilter(&created, *id)) {
		return std::nullopt;
	}
	return created;
}

std::optional<ChatFilter> ChatFilters::rebaseLocalEdit(
		const QString &uuid,
		const ChatFilter &baseline,
		const ChatFilter &edited) const {
	if (!_loaded || !_localReplica) {
		return std::nullopt;
	}
	const auto before = AxFolders::ExportFilter(baseline);
	const auto after = AxFolders::ExportFilter(edited);
	if (!before || !after) {
		return std::nullopt;
	}
	const auto rebased = AxFolders::RebaseEdit(
		*_localReplica,
		uuid,
		*before,
		*after);
	return rebased
		? AxFolders::ImportFilter(_owner, rebased->id, rebased->values)
		: std::nullopt;
}

bool ChatFilters::applyLocalReplica(
		const AxFolders::Replica &expected,
		const AxFolders::Replica &updated) {
	if (!_loaded || !_localReplica
		|| updated.account != _owner->session().userId().bare
		|| updated.device != expected.device) {
		return false;
	}
	const auto before = AxFolders::EncodeState(expected);
	const auto current = AxFolders::EncodeState(*_localReplica);
	if (!before || !current || *before != *current || !AxFolders::EncodeState(updated)) {
		return false;
	}
	auto filters = std::vector<ChatFilter>();
	for (const auto &local : AxFolders::LocalFolders(updated)) {
		const auto filter = AxFolders::ImportFilter(_owner, local.id, local.values);
		if (!filter) return false;
		filters.push_back(*filter);
	}
	if (!AxFolders::Save(LocalFoldersRoot(), updated)) return false;
	if (before == AxFolders::EncodeState(updated)) {
		return true;
	}
	_localReplica = updated;
	projectLocalReplica(filters);
	return true;
}

bool ChatFilters::saveLocalFilter(const ChatFilter *filter, FilterId id) {
	if (!_loaded || !_localReplica || (id < 1000 && id != -1)) return false;
	auto source = std::vector<AxFolders::LocalFolder>();
	const auto append = [&](const ChatFilter &value) {
		if (value.id() < 1000) {
			source.push_back({ {}, value.id(), {} });
			return true;
		}
		const auto values = AxFolders::ExportFilter(value);
		if (!values) return false;
		auto uuid = _localReplica->ids.key(value.id());
		if (uuid.isEmpty()) {
			if (!filter || value.id() != id) return false;
			uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
		}
		source.push_back({ uuid, value.id(), *values });
		return true;
	};
	auto found = false;
	for (const auto &value : _list) {
		if (value.id() == id) {
			found = true;
			if (filter && !append(*filter)) return false;
		} else if (!append(value)) {
			return false;
		}
	}
	if (filter && !found && !append(*filter)) return false;
	const auto changed = AxFolders::Capture(*_localReplica, source);
	if (!changed) {
		return false;
	}
	if (_localSync && AxFolders::EncodeState(*changed) != AxFolders::EncodeState(*_localReplica)) {
		_localSync->wake();
	}
	return applyLocalReplica(*_localReplica, *changed);
}

bool ChatFilters::startLocalSync(
		const QString &workspace,
		const QByteArray &token,
		const QByteArray &key,
		std::function<void(AxFolders::RuntimeStatus)> status) {
	if (!_loaded || !_localReplica) {
		return false;
	}
	_localSync.reset();
	_localSync = std::make_unique<AxFolders::FolderRuntime>(
		std::make_unique<AxFolders::FolderHttp>(QUrl(u"https://mistyk-games.ru/axgram-sync"_q)),
		_owner->session().userId().bare,
		workspace,
		token,
		key,
		AxFolders::ExchangeStore{
			[=]() -> std::optional<AxFolders::Replica> {
				return _loaded ? _localReplica : std::nullopt;
			},
			[=](const auto &expected, const auto &updated) {
				return applyLocalReplica(expected, updated);
			},
		},
		std::move(status));
	_localSync->wake();
	return true;
}

void ChatFilters::stopLocalSync() {
	_localSync.reset();
}

void ChatFilters::retryLocalSync() {
	if (_localConnection) {
		_localConnection->retry();
	} else if (_localSync) {
		_localSync->retry();
	}
}

void ChatFilters::ensureLocalConnection() {
	if (_localConnection || !_loaded) return;
	if (!_localReplica) { _localConnectionText = u"Не удалось прочитать папки"_q; return; }
	const auto endpoint = QUrl(u"https://mistyk-games.ru/axgram-sync"_q);
	const auto root = LocalFoldersRoot();
	const auto account = qint64(_owner->session().userId().bare);
	const auto profileKey = _owner->session().local().peekLegacyLocalKey();
	_localConnection = std::make_unique<AxFolders::FolderConnection>(
		std::make_unique<AxFolders::FolderHttp>(endpoint), account,
		AxFolders::ConnectionStorage{
			[=] { return AxFolders::LoadLogin(root, endpoint, account, profileKey); },
			[=](const auto &credential) { return AxFolders::SaveLogin(root, endpoint, credential, profileKey); },
			[=] { return AxFolders::ClearLogin(root, account); },
		},
		AxFolders::ConnectionRuntime{
			[=](const auto &credential, auto changed) {
				return startLocalSync(credential.workspace, credential.token, credential.key, std::move(changed));
			},
			[=] { stopLocalSync(); },
			[=] { if (_localSync) _localSync->retry(); },
		},
		[=](AxFolders::ConnectionPhase phase) {
			using Phase = AxFolders::ConnectionPhase;
			switch (phase) {
			case Phase::Off: _localConnectionText = u"Выключено"_q; break;
			case Phase::Restoring: _localConnectionText = u"Восстанавливаем подключение"_q; break;
			case Phase::Confirming: _localConnectionText = u"Подтвердите подключение в AxGram"_q; break;
			case Phase::Connecting: _localConnectionText = u"Подключаем папки"_q; break;
			case Phase::Ready: _localConnectionText = u"Подключено"_q; break;
			case Phase::Syncing: _localConnectionText = u"Синхронизация"_q; break;
			case Phase::Retrying: _localConnectionText = u"Повторим при восстановлении связи"_q; break;
			case Phase::NeedsLogin: _localConnectionText = u"Подтвердите подключение снова"_q; break;
			case Phase::Failure: _localConnectionText = u"Не удалось подключиться. Повторите попытку"_q; break;
			case Phase::ConfirmationExpired: _localConnectionText = u"Срок подтверждения Telegram истёк. Получите новое подтверждение и попробуйте снова. Ваши папки сохранены."_q; break;
			}
		});
	crl::on_main(&_owner->session(), [=] {
		if (_localConnection) _localConnection->restore();
	});
}

void ChatFilters::connectLocalAccount(std::shared_ptr<Ui::Show> show) {
	ensureLocalConnection();
	if (!_localConnection) return;
	const auto weak = base::make_weak(&_owner->session());
	_localConnection->connectAccount([=](AxFolders::ConnectionConsent consent) {
		if (!weak || !consent.active()) return;
		auto callbacks = std::make_shared<UrlAuthBox::ServiceCallbacks>();
		callbacks->clientId = consent.flow.clientId;
		callbacks->origin = consent.flow.endpoint.adjusted(
			QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment
		).toString(QUrl::FullyEncoded);
		callbacks->account = consent.flow.account;
		callbacks->active = [weak, current = consent.active] { return weak && current(); };
		callbacks->accepted = [accepted = consent.accepted](QString url) { accepted(std::move(url)); };
		callbacks->cancelled = [cancel = consent.cancelled] { cancel(); };
		callbacks->failed = [failed = consent.failed] { failed(); };
		callbacks->expired = [expired = consent.expired] { if (expired) expired(); };
		callbacks->bindClose = [bind = consent.bindClose](Fn<void()> close) { bind([close] { close(); }); };
		UrlAuthBox::ActivateService(show, &_owner->session(), consent.flow.confirmation, callbacks);
	});
}

void ChatFilters::disconnectLocalAccount() {
	if (_localConnection) {
		_localConnection->disconnectAccount();
	} else {
		stopLocalSync();
		if (!AxFolders::ClearLogin(LocalFoldersRoot(), _owner->session().userId().bare)) {
			_localConnectionText = u"Не удалось удалить сохранённое подключение"_q;
		}
	}
}

void ChatFilters::cancelLocalAccount() {
	if (_localConnection) _localConnection->cancel();
}

void ChatFilters::localDevices(std::function<void(AxFolders::DevicesResult)> done) {
	ensureLocalConnection();
	if (_localConnection) _localConnection->devices(std::move(done));
	else done({ AxFolders::LoginError::Expired });
}

void ChatFilters::revokeLocalDevice(const QString &device, std::function<void(AxFolders::LoginError)> done) {
	if (_localConnection) _localConnection->revoke(device, std::move(done));
	else done(AxFolders::LoginError::Expired);
}

AxFolders::ConnectionPhase ChatFilters::localConnectionPhase() const {
	return _localConnection ? _localConnection->phase() : AxFolders::ConnectionPhase::Off;
}

rpl::producer<QString> ChatFilters::localConnectionText() const {
	return _localConnectionText.value();
}

void ChatFilters::projectLocalReplica(const std::vector<ChatFilter> &filters) {
	for (auto i = int(_list.size()); i > 0; --i) {
		const auto id = _list[i - 1].id();
		if (id >= 1000 && !ranges::contains(filters, id, &ChatFilter::id)) {
			applyRemove(i - 1);
		}
	}
	for (const auto &filter : filters) {
		const auto i = ranges::find(_list, filter.id(), &ChatFilter::id);
		if (i == _list.end()) {
			applyInsert(filter, _list.size());
		} else {
			applyChange(*i, ChatFilter(filter));
		}
	}
	const auto all = ranges::find(_list, FilterId(0), &ChatFilter::id);
	if (AxSettings::getInstance().hideAllChatsFolder() && _list.size() > 1) {
		if (all != _list.end()) _list.erase(all);
	} else if (all == _list.end()) {
		_list.insert(_list.begin(), ChatFilter());
	}
	OrderLocalFolders(_list, *_localReplica);
	_listChanged.fire({});
}

void ChatFilters::moveAllToFront() {
	const auto i = ranges::find(_list, FilterId(), &ChatFilter::id);
	if (!_list.empty() && i == begin(_list)) {
		return;
	} else if (i != end(_list)) {
		_list.erase(i);
	}
	_list.insert(begin(_list), ChatFilter());
}

void ChatFilters::applyRemove(int position) {
	Expects(position >= 0 && position < _list.size());

	// Remove the filter from the list before applyChange() tears down its
	// chats. Unpinning a chat re-caches its siblings' pinned indices, which
	// runs Session::refreshChatListEntry(); while the filter is still listed
	// but emptied that re-enters PinnedList::setPinned() on the same pinned
	// list mid-iteration and crashes (see also PinnedList::setPinned).
	const auto i = begin(_list) + position;
	auto filter = std::move(*i);
	_list.erase(i);
	applyChange(filter, ChatFilter(filter.id(), {}, {}, {}, {}, {}, {}, {}));
}

bool ChatFilters::applyChange(ChatFilter &filter, ChatFilter &&updated) {
	Expects(filter.id() == updated.id());

	using Flag = ChatFilter::Flag;

	const auto id = filter.id();
	const auto exceptionsChanged = filter.always() != updated.always();
	const auto rulesMask = Flag() | Flag::RulesMask;
	const auto rulesChanged = exceptionsChanged
		|| ((filter.flags() & rulesMask) != (updated.flags() & rulesMask))
		|| (filter.never() != updated.never());
	const auto pinnedChanged = (filter.pinned() != updated.pinned());
	const auto chatlistChanged = (filter.chatlist() != updated.chatlist())
		|| (filter.hasMyLinks() != updated.hasMyLinks());
	const auto listUpdated = rulesChanged
		|| pinnedChanged
		|| (filter.titleText() != updated.titleText())
		|| (filter.staticTitle() != updated.staticTitle())
		|| (filter.iconEmoji() != updated.iconEmoji());
	const auto colorChanged = filter.colorIndex() != updated.colorIndex();
	const auto colorExistenceChanged = (!filter.colorIndex())
		!= (!updated.colorIndex());
	if (!listUpdated && !chatlistChanged && !colorChanged) {
		return false;
	}
	const auto wasFilter = std::move(filter);
	filter = std::move(updated);
	auto entryToRefreshHeight = (Dialogs::Entry*)(nullptr);
	if (rulesChanged) {
		const auto filterList = _owner->chatsFilters().chatsList(id);
		const auto areTagsEnabled = tagsEnabled();
		const auto tagsExistence = [&](not_null<Dialogs::Row*> row) {
			return (!areTagsEnabled || entryToRefreshHeight)
				? false
				: row->entry()->hasChatsFilterTags(0);
		};
		const auto feedHistory = [&](not_null<History*> history) {
			const auto now = filter.contains(history);
			const auto was = wasFilter.contains(history);
			if (now != was) {
				if (now) {
					history->addToChatList(id, filterList);
				} else {
					history->removeFromChatList(id, filterList);
				}
			}
		};
		const auto feedList = [&](not_null<const Dialogs::MainList*> list) {
			for (const auto &entry : *list->indexed()) {
				if (const auto history = entry->history()) {
					const auto wasTags = tagsExistence(entry);
					feedHistory(history);
					if (wasTags != tagsExistence(entry)) {
						entryToRefreshHeight = entry->entry();
					}
				}
			}
		};
		feedList(_owner->chatsList());
		if (const auto folder = _owner->folderLoaded(Data::Folder::kId)) {
			feedList(folder->chatsList());
		}
		if (exceptionsChanged && !filter.always().empty()) {
			_exceptionsToLoad.push_back(id);
			Ui::PostponeCall(&_owner->session(), [=] {
				_owner->session().api().requestMoreDialogsIfNeeded();
			});
		}
	}
	if (pinnedChanged) {
		const auto filterList = _owner->chatsFilters().chatsList(id);
		filterList->pinned()->applyList(filter.pinned());
	}
	if (chatlistChanged) {
		_isChatlistChanged.fire_copy(id);
	}
	if (colorChanged) {
		_tagColorChanged.fire_copy(TagColorChanged{
			.filterId = id,
			.colorExistenceChanged = colorExistenceChanged,
		});
	}
	if (entryToRefreshHeight) {
		// Trigger a full refresh of height for the main list.
		entryToRefreshHeight->updateChatListEntryHeight();
	}
	return listUpdated;
}

bool ChatFilters::applyOrder(const QVector<MTPint> &order) {
	const auto hasLocal = ranges::any_of(_list, [](const ChatFilter &filter) {
		return filter.id() >= 1000;
	});
	const auto serverOnly = ranges::all_of(order, [](const MTPint &id) {
		return id.v < 1000;
	});
	if (hasLocal && serverOnly) {
		auto server = std::vector<int>();
		for (const auto &id : order) server.push_back(id.v);
		auto existing = std::vector<int>();
		for (const auto &filter : _list) existing.push_back(filter.id());
		const auto resolved = AxFolders::ServerOrder(existing, server);
		if (!resolved) return false;
		auto complete = QVector<MTPint>();
		for (const auto id : *resolved) complete.push_back(MTP_int(id));
		return applyOrder(complete);
	}
	if (order.size() != _list.size()) {
		return false;
	} else if (_list.empty()) {
		return true;
	}
	auto indices = ranges::views::all(
		_list
	) | ranges::views::transform(
		&ChatFilter::id
	) | ranges::to_vector;
	auto b = indices.begin(), e = indices.end();
	for (const auto &id : order) {
		const auto i = ranges::find(b, e, id.v);
		if (i == e) {
			return false;
		} else if (i != b) {
			std::swap(*i, *b);
		}
		++b;
	}
	auto changed = false;
	auto begin = _list.begin(), end = _list.end();
	for (const auto &id : order) {
		const auto i = ranges::find(begin, end, id.v, &ChatFilter::id);
		Assert(i != end);
		if (i != begin) {
			changed = true;
			std::swap(*i, *begin);
		}
		++begin;
	}
	if (changed) {
		_listChanged.fire({});
	}
	return true;
}

const ChatFilter &ChatFilters::applyUpdatedPinned(
		FilterId id,
		const std::vector<Dialogs::Key> &dialogs) {
	const auto i = ranges::find(_list, id, &ChatFilter::id);
	Assert(i != end(_list));

	const auto limit = _owner->pinnedChatsLimit(id);
	auto always = i->always();
	auto pinned = std::vector<not_null<History*>>();
	pinned.reserve(dialogs.size());
	for (const auto &row : dialogs) {
		if (const auto history = row.history()) {
			if (always.contains(history)) {
				pinned.push_back(history);
			} else if (always.size() < limit) {
				always.insert(history);
				pinned.push_back(history);
			}
		}
	}
	set(ChatFilter(
		id,
		i->title(),
		i->iconEmoji(),
		i->colorIndex(),
		i->flags(),
		std::move(always),
		std::move(pinned),
		i->never()));
	return *i;
}

void ChatFilters::saveOrder(
		const std::vector<FilterId> &order,
		mtpRequestId after) {
	if (after) {
		_saveOrderAfterId = after;
	}
	const auto api = &_owner->session().api();
	api->request(_saveOrderRequestId).cancel();
	auto previous = QVector<MTPint>();
	for (const auto &filter : _list) previous.push_back(MTP_int(filter.id()));

	auto ids = QVector<MTPint>();
	ids.reserve(order.size());
	for (const auto id : order) {
		ids.push_back(MTP_int(id));
	}
	if (!applyOrder(ids)) return;
	if (_localReplica && !_localReplica->ids.isEmpty()
		&& !saveLocalFilter(nullptr, -1)) {
		applyOrder(previous);
		LOG(("AxGram: local folder order was not saved."));
		return;
	}
	ids.erase(std::remove_if(ids.begin(), ids.end(), [](const MTPint &id) {
		return id.v >= 1000;
	}), ids.end());
	const auto wrapped = MTP_vector<MTPint>(ids);
	_saveOrderRequestId = api->request(MTPmessages_UpdateDialogFiltersOrder(
		wrapped
	)).afterRequest(_saveOrderAfterId).send();
}

bool ChatFilters::archiveNeeded() const {
	for (const auto &filter : _list) {
		if (!(filter.flags() & ChatFilter::Flag::NoArchived)) {
			return true;
		}
	}
	return false;
}

const std::vector<ChatFilter> &ChatFilters::list() const {
	return _list;
}

FilterId ChatFilters::defaultId() const {
	return lookupId(0);
}

FilterId ChatFilters::lookupId(int index) const {
	// Expects(index >= 0 && index < _list.size());
	if (!(index >= 0 && index < _list.size())) {
		return FilterId(); // AxGram: fix crash when using `hideAllChatsFolder`
	}

	const auto &settings = AxSettings::getInstance();

	if (_owner->session().user()->isPremium() || !_list.front().id() || settings.hideAllChatsFolder()) {
		return _list[index].id();
	}
	const auto i = ranges::find(_list, FilterId(0), &ChatFilter::id);
	return !index
		? FilterId()
		: (index <= int(i - begin(_list)))
		? _list[index - 1].id()
		: _list[index].id();
}

bool ChatFilters::loaded() const {
	return _loaded;
}

bool ChatFilters::has() const {
	return _list.size() > 1;
}

rpl::producer<> ChatFilters::changed() const {
	return _listChanged.events();
}

rpl::producer<FilterId> ChatFilters::isChatlistChanged() const {
	return _isChatlistChanged.events();
}

rpl::producer<TagColorChanged> ChatFilters::tagColorChanged() const {
	return _tagColorChanged.events();
}

bool ChatFilters::loadNextExceptions(bool chatsListLoaded) {
	if (_exceptionsLoadRequestId) {
		return true;
	} else if (!chatsListLoaded
		&& (_owner->chatsList()->fullSize().current()
			< kLoadExceptionsAfter)) {
		return false;
	}
	auto inputs = QVector<MTPInputDialogPeer>();
	const auto collectExceptions = [&](FilterId id) {
		auto result = QVector<MTPInputDialogPeer>();
		const auto i = ranges::find(_list, id, &ChatFilter::id);
		if (i != end(_list)) {
			result.reserve(i->always().size());
			for (const auto &history : i->always()) {
				if (!history->folderKnown()) {
					inputs.push_back(
						MTP_inputDialogPeer(history->peer->input()));
				}
			}
		}
		return result;
	};
	while (!_exceptionsToLoad.empty()) {
		const auto id = _exceptionsToLoad.front();
		const auto exceptions = collectExceptions(id);
		if (inputs.size() + exceptions.size() > kLoadExceptionsPerRequest) {
			Assert(!inputs.isEmpty());
			break;
		}
		_exceptionsToLoad.pop_front();
		inputs.append(exceptions);
	}
	if (inputs.isEmpty()) {
		return false;
	}
	const auto api = &_owner->session().api();
	_exceptionsLoadRequestId = api->request(MTPmessages_GetPeerDialogs(
		MTP_vector(inputs)
	)).done([=](const MTPmessages_PeerDialogs &result) {
		_exceptionsLoadRequestId = 0;
		_owner->session().data().histories().applyPeerDialogs(result);
		_owner->session().api().requestMoreDialogsIfNeeded();
	}).fail([=] {
		_exceptionsLoadRequestId = 0;
		_owner->session().api().requestMoreDialogsIfNeeded();
	}).send();
	return true;
}

void ChatFilters::refreshHistory(not_null<History*> history) {
	if (history->inChatList() && !list().empty()) {
		_owner->refreshChatListEntry(history);
	}
}

void ChatFilters::requestSuggested() {
	if (_suggestedRequestId) {
		return;
	}
	if (_suggestedLastReceived > 0
		&& crl::now() - _suggestedLastReceived < kRefreshSuggestedTimeout) {
		return;
	}
	const auto api = &_owner->session().api();
	_suggestedRequestId = api->request(MTPmessages_GetSuggestedDialogFilters(
	)).done([=](const MTPVector<MTPDialogFilterSuggested> &data) {
		_suggestedRequestId = 0;
		_suggestedLastReceived = crl::now();

		_suggested = ranges::views::all(
			data.v
		) | ranges::views::transform([&](const MTPDialogFilterSuggested &f) {
			return f.match([&](const MTPDdialogFilterSuggested &data) {
				return SuggestedFilter{
					Data::ChatFilter::FromTL(data.vfilter(), _owner),
					qs(data.vdescription())
				};
			});
		}) | ranges::to_vector;

		_suggestedUpdated.fire({});
	}).fail([=] {
		_suggestedRequestId = 0;
		_suggestedLastReceived = crl::now() + kRefreshSuggestedTimeout / 2;

		_suggestedUpdated.fire({});
	}).send();
}

bool ChatFilters::suggestedLoaded() const {
	return (_suggestedLastReceived > 0);
}

const std::vector<SuggestedFilter> &ChatFilters::suggestedFilters() const {
	return _suggested;
}

rpl::producer<> ChatFilters::suggestedUpdated() const {
	return _suggestedUpdated.events();
}

rpl::producer<Ui::MoreChatsBarContent> ChatFilters::moreChatsContent(
		FilterId id) {
	if (!id) {
		return rpl::single(Ui::MoreChatsBarContent{ .count = 0 });
	}
	return [=](auto consumer) {
		auto result = rpl::lifetime();

		auto &entry = _moreChatsData[id];
		auto watching = entry.watching.lock();
		if (!watching) {
			watching = std::make_shared<bool>(true);
			entry.watching = watching;
		}
		result.add([watching] {});

		_moreChatsUpdated.events_starting_with_copy(
			id
		) | rpl::on_next([=] {
			consumer.put_next(Ui::MoreChatsBarContent{
				.count = int(moreChats(id).size()),
			});
		}, result);
		loadMoreChatsList(id);

		return result;
	};
}

const std::vector<not_null<PeerData*>> &ChatFilters::moreChats(
		FilterId id) const {
	static const auto kEmpty = std::vector<not_null<PeerData*>>();
	if (!id) {
		return kEmpty;
	}
	const auto i = _moreChatsData.find(id);
	return (i != end(_moreChatsData)) ? i->second.missing : kEmpty;
}

void ChatFilters::moreChatsHide(FilterId id, bool localOnly) {
	if (!localOnly) {
		const auto api = &_owner->session().api();
		api->request(MTPchatlists_HideChatlistUpdates(
			MTP_inputChatlistDialogFilter(MTP_int(id))
		)).send();
	}

	const auto i = _moreChatsData.find(id);
	if (i != end(_moreChatsData)) {
		if (const auto requestId = base::take(i->second.requestId)) {
			_owner->session().api().request(requestId).cancel();
		}
		i->second.missing = {};
		i->second.lastUpdate = crl::now();
		_moreChatsUpdated.fire_copy(id);
	}
}

void ChatFilters::loadMoreChatsList(FilterId id) {
	Expects(id != 0);

	const auto i = ranges::find(_list, id, &ChatFilter::id);
	if (i == end(_list) || !i->chatlist()) {
		return;
	}

	auto &entry = _moreChatsData[id];
	const auto now = crl::now();
	if (!entry.watching.lock() || entry.requestId) {
		return;
	}
	const auto last = entry.lastUpdate;
	const auto next = last ? (last + RequestUpdatesEach(_owner)) : 0;
	if (next > now) {
		if (!_moreChatsTimer.isActive()) {
			_moreChatsTimer.callOnce(next - now);
		}
		return;
	}
	auto &api = _owner->session().api();
	entry.requestId = api.request(MTPchatlists_GetChatlistUpdates(
		MTP_inputChatlistDialogFilter(MTP_int(id))
	)).done([=](const MTPchatlists_ChatlistUpdates &result) {
		const auto &data = result.data();
		_owner->processUsers(data.vusers());
		_owner->processChats(data.vchats());
		auto list = ranges::views::all(
			data.vmissing_peers().v
		) | ranges::views::transform([&](const MTPPeer &peer) {
			return _owner->peer(peerFromMTP(peer));
		}) | ranges::to_vector;

		auto &entry = _moreChatsData[id];
		entry.requestId = 0;
		entry.lastUpdate = crl::now();
		if (!_moreChatsTimer.isActive()) {
			_moreChatsTimer.callOnce(RequestUpdatesEach(_owner));
		}
		if (entry.missing != list) {
			entry.missing = std::move(list);
			_moreChatsUpdated.fire_copy(id);
		}
	}).fail([=] {
		auto &entry = _moreChatsData[id];
		entry.requestId = 0;
		entry.lastUpdate = crl::now();
	}).send();
}

void ChatFilters::checkLoadMoreChatsLists() {
	for (const auto &[id, entry] : _moreChatsData) {
		loadMoreChatsList(id);
	}
}

bool CanRemoveFromChatFilter(
		const ChatFilter &filter,
		not_null<History*> history) {
	using Flag = ChatFilter::Flag;
	const auto flagsWithoutNoReadNoArchivedNoMuted = filter.flags()
		& ~(Flag::NoRead | Flag::NoArchived | Flag::NoMuted);
	return (filter.always().size() > 1 || flagsWithoutNoReadNoArchivedNoMuted)
		&& filter.contains(history);
}

} // namespace Data
