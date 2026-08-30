// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ax_state.h"

#include "ax/ax_settings.h"

namespace AxState {

std::unordered_map<PeerId, std::unordered_set<MsgId>> hiddenMessages;
Main::Session *disableGhostModeOnStoryCloseSession = nullptr;

void hide(PeerId peerId, MsgId messageId) {
	hiddenMessages[peerId].insert(messageId);
}

void hide(not_null<HistoryItem*> item) {
	hide(item->history()->peer->id, item->id);
}

bool isHidden(PeerId peerId, MsgId messageId) {
	const auto it = hiddenMessages.find(peerId);
	if (it != hiddenMessages.end()) {
		return it->second.contains(messageId);
	}
	return false;
}

bool isHidden(not_null<HistoryItem*> item) {
	return isHidden(item->history()->peer->id, item->id);
}

void setDisableGhostModeOnStoryClose(Main::Session *session) {
	disableGhostModeOnStoryCloseSession = session;
}

void disableGhostModeOnStoryClose(Main::Session *session) {
	if (disableGhostModeOnStoryCloseSession != session) {
		return;
	}
	disableGhostModeOnStoryCloseSession = nullptr;
	if (session) {
		AxSettings::ghost(session).setGhostModeEnabled(false);
	}
}

}
