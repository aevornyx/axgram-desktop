// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "history/history.h"
#include "main/main_session.h"

namespace AxForward {
bool isForwarding(const PeerId &id);
void cancelForward(const PeerId &id, const Main::Session &session);
std::pair<QString, QString> stateName(const PeerId &id);

class ForwardState
{
public:
	enum class State
	{
		Preparing,
		Downloading,
		Sending,
		Finished
	};
	void updateBottomBar(const Main::Session &session, const PeerId *peer, const State &st);

	int totalChunks;
	int currentChunk;
	int totalMessages;
	int sentMessages;

	State state = State::Preparing;
	bool stopRequested = false;

};

bool isAxForwardNeeded(const std::vector<not_null<HistoryItem*>> &items);
bool isAxForwardNeeded(not_null<HistoryItem*> item);
bool isFullAxForwardNeeded(not_null<HistoryItem*> item);
void intelligentForward(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	const Data::ResolvedForwardDraft &draft);
void forwardMessages(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	bool forwardState,
	const Data::ResolvedForwardDraft &draft);

}
