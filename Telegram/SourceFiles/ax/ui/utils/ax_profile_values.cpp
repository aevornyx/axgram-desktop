// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ui/utils/ax_profile_values.h"

#include "ax/ax_settings.h"
#include "ax/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "lang/lang_text_entity.h"

constexpr auto kMaxChannelId = -1000000000000;

QString IDString(const not_null<PeerData*> peer) {
	auto resultId = QString::number(getBareID(peer));

	const auto &settings = AxSettings::getInstance();
	if (settings.showPeerId() == PeerIdDisplay::BotApi) {
		if (peer->isChannel()) {
			resultId = QString::number(peerToChannel(peer->id).bare - kMaxChannelId).prepend("-");
		} else if (peer->isChat()) {
			resultId = resultId.prepend("-");
		}
	}

	return resultId;
}

QString IDString(MsgId topicRootId) {
	return QString::number(topicRootId.bare);
}

rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer) {
	return AxSettings::getInstance().showPeerIdValue(
	) | rpl::map([=](PeerIdDisplay display) {
		return (display == PeerIdDisplay::Hidden)
			? TextWithEntities()
			: tr::marked(IDString(peer));
	});
}

rpl::producer<TextWithEntities> IDValue(MsgId topicRootId) {
	return AxSettings::getInstance().showPeerIdValue(
	) | rpl::map([=](PeerIdDisplay display) {
		return (display == PeerIdDisplay::Hidden)
			? TextWithEntities()
			: tr::marked(IDString(topicRootId));
	});
}
