// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/utils/rc_manager.h"

#include <QJsonArray>
#include <qjsondocument.h>
#include <QTimer>

namespace {

// AxGram: сервер исходного проекта убран. Сам RCManager отключён в
// ax_infra.cpp, но адрес не оставляем и в коде.
constexpr auto kPrimaryUrl = "";
constexpr auto kFetchTimeout = 15 * 1000;

}

// AxGram: списки аккаунтов исходных проектов очищены. Они раздавали особый
// значок разработчика 46 чужим аккаунтам и каналам прямо в этом клиенте.
std::unordered_set<ID> default_developers = {};

std::unordered_set<ID> default_channels = {};

void RCManager::start() {
	DEBUG_LOG(("RCManager: starting"));
	_manager = std::make_unique<QNetworkAccessManager>();

	makeRequest();

	_timer = new QTimer(this);
	connect(_timer, &QTimer::timeout, this, &RCManager::makeRequest);
	_timer->start(60 * 60 * 1000); // 1 hour
}

void RCManager::makeRequest() {
	_retryAttempted = false;
	sendRequest();
}

void RCManager::sendRequest() {
	if (!_manager) {
		return;
	}

	// AxGram: удалённая конфигурация не запрашивается ни с какого адреса.
	// Основной сервер исходного проекта и его запасной (api.exteragram.app)
	// удалены; списки разработчиков и каналов остаются встроенными.
	const auto url = QString::fromLatin1(kPrimaryUrl);
	if (url.isEmpty()) {
		return;
	}

	LOG(("RCManager: requesting map"));

	clearSentRequest();

	auto request = QNetworkRequest(QUrl(url));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(kFetchTimeout);
	_reply = _manager->get(request);
	connect(_reply,
			&QNetworkReply::finished,
			[=]
			{
				gotResponse();
			});
	connect(_reply,
			&QNetworkReply::errorOccurred,
			[=](auto e)
			{
				gotFailure(e);
			});
}

bool RCManager::tryRetryWithAxFallback() {
	if (_retryAttempted || _useAxFallback) {
		return false;
	}
	LOG(("RCManager: switching to ax fallback endpoint"));
	_useAxFallback = true;
	_retryAttempted = true;
	sendRequest();
	return true;
}

void RCManager::gotResponse() {
	if (!_reply) {
		return;
	}

	const auto response = _reply->readAll();
	clearSentRequest();

	if (!handleResponse(response)) {
		LOG(("RCManager: Error bad map size: %1").arg(response.size()));
		gotFailure(QNetworkReply::UnknownContentError);
	}
}

bool RCManager::handleResponse(const QByteArray &response) {
	try {
		return applyResponse(response);
	} catch (...) {
		LOG(("RCManager: Failed to apply response"));
		return false;
	}
}

bool RCManager::applyResponse(const QByteArray &response) {
	auto error = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(response, &error);
	if (error.error != QJsonParseError::NoError) {
		LOG(("RCManager: Failed to parse JSON, error: %1"
		).arg(error.errorString()));
		return false;
	}
	if (!document.isObject()) {
		LOG(("RCManager: not an object received in JSON"));
		return false;
	}
	const auto root = document.object();

	const auto developers = root.value("developers").toArray();
	const auto officialChannels = root.value("officialChannels").toArray();
	const auto supporters = root.value("supporters").toArray();
	const auto supporterChannels = root.value("supporterChannels").toArray();
	const auto customBadges = root.value("customBadges").toArray();

	_developers.clear();
	_officialChannels.clear();
	_supporters.clear();
	_supporterChannels.clear();
	_customBadges.clear();

	for (const auto &developer : developers) {
		if (const auto id = developer.toVariant().toLongLong()) {
			_developers.insert(id);
		}
	}

	for (const auto &channel : officialChannels) {
		if (const auto id = channel.toVariant().toLongLong()) {
			_officialChannels.insert(id);
		}
	}

	for (const auto &supporter : supporters) {
		if (const auto id = supporter.toVariant().toLongLong()) {
			_supporters.insert(id);
		}
	}

	for (const auto &channel : supporterChannels) {
		if (const auto id = channel.toVariant().toLongLong()) {
			_supporterChannels.insert(id);
		}
	}

	for (const auto &badge : customBadges) {
		if (!badge.isObject()) {
			continue;
		}
		const auto obj = badge.toObject();
		const auto id = obj.value("id").toVariant().toLongLong();
		if (!id) {
			continue;
		}
		const auto badgeObj = obj.value("badge");
		if (!badgeObj.isObject()) {
			continue;
		}
		const auto badgeData = badgeObj.toObject();
		CustomBadge customBadge;
		if (const auto emojiStatusId = badgeData.value("documentId").toVariant().toLongLong()) {
			customBadge.emojiStatusId = EmojiStatusId(emojiStatusId);
		} else {
			continue;
		}
		if (const auto text = badgeData.value("text").toString(); !text.isEmpty()) {
			customBadge.text = text;
		}
		_customBadges[id] = customBadge;
	}

	initialized = true;

	LOG(("RCManager: Loaded %1 developers, %2 official channels"
	).arg(_developers.size()).arg(_officialChannels.size()));

	return true;
}

void RCManager::gotFailure(QNetworkReply::NetworkError e) {
	LOG(("RCManager: Error %1").arg(e));
	if (tryRetryWithAxFallback()) {
		LOG(("RCManager: retrying request with ax fallback endpoint"));
		return;
	}
	LOG(("RCManager: no retry left for failed request"));
	if (const auto reply = base::take(_reply)) {
		reply->deleteLater();
	}
}

void RCManager::clearSentRequest() {
	const auto reply = base::take(_reply);
	if (!reply) {
		return;
	}
	disconnect(reply, &QNetworkReply::finished, nullptr, nullptr);
	disconnect(reply, &QNetworkReply::errorOccurred, nullptr, nullptr);
	reply->abort();
	reply->deleteLater();
}

RCManager::~RCManager() {
	clearSentRequest();
	_manager = nullptr;
}
