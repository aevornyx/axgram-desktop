// Часть AxGram для Desktop.
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/utils/ax_update_check.h"

#include "base/weak_ptr.h"
#include "core/version.h"
#include "lang_auto.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "window/window_session_controller.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Ax::Updates {
namespace {

constexpr auto kTimeoutMs = 15 * 1000;
constexpr auto kMaxBodyBytes = 512 * 1024;

// Один менеджер на всё приложение: создавать его на каждый запрос дорого,
// а проверка редкая и ручная.
QNetworkAccessManager &Manager() {
	static auto instance = QNetworkAccessManager();
	return instance;
}

[[nodiscard]] QString ApiUrl() {
	return QString("https://api.github.com/repos/%1/%2/releases/latest")
		.arg(kGithubOwner)
		.arg(kGithubRepo);
}

// "v6.7.8" или "6.7.8" -> 6007008, как в core/version.h.
// 0 означает, что тег разобрать не удалось.
[[nodiscard]] int ParseVersion(const QString &tag) {
	static const auto expression = QRegularExpression(
		u"^v?(\d+)\.(\d+)(?:\.(\d+))?$"_q);
	const auto match = expression.match(tag.trimmed());
	if (!match.hasMatch()) {
		return 0;
	}
	const auto major = match.captured(1).toInt();
	const auto minor = match.captured(2).toInt();
	const auto patch = match.capturedLength(3) ? match.captured(3).toInt() : 0;
	if (major < 0 || minor < 0 || patch < 0 || minor > 999 || patch > 999) {
		return 0;
	}
	return major * 1000000 + minor * 1000 + patch;
}

void ShowLatest(not_null<Window::SessionController*> controller) {
	controller->showToast(tr::ax_UpdateNoNew(tr::now));
}

void ShowFailed(not_null<Window::SessionController*> controller) {
	controller->showToast(tr::ax_UpdateCheckFailed(tr::now));
}

void ShowFound(
		not_null<Window::SessionController*> controller,
		const QString &version) {
	controller->show(Ui::MakeConfirmBox({
		.text = tr::ax_UpdateAvailable(tr::now, lt_version, version),
		.confirmed = [=](Fn<void()> &&close) {
			QDesktopServices::openUrl(QUrl(ReleasesPageUrl()));
			close();
		},
		.confirmText = tr::ax_UpdateOpenPage(),
	}));
}

} // namespace

QString ReleasesPageUrl() {
	return QString("https://github.com/%1/%2/releases/latest")
		.arg(kGithubOwner)
		.arg(kGithubRepo);
}

bool SupportAvailable() {
	return *kSupportBot != 0;
}

void OpenSupport() {
	if (!SupportAvailable()) {
		return;
	}
	// Открываем бота внутри самого клиента, а не в браузере.
	QDesktopServices::openUrl(
		QUrl(QString("tg://resolve?domain=%1&start=support").arg(kSupportBot)));
}

void CheckManually(not_null<Window::SessionController*> controller) {
	auto request = QNetworkRequest(QUrl(ApiUrl()));
	request.setRawHeader(
		QByteArrayLiteral("Accept"),
		QByteArrayLiteral("application/vnd.github+json"));
	request.setRawHeader(
		QByteArrayLiteral("User-Agent"),
		QByteArrayLiteral("AxGram/") + AppVersionStr);
	request.setTransferTimeout(kTimeoutMs);
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	const auto reply = Manager().get(request);
	const auto weak = base::make_weak(controller);
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		reply->deleteLater();
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		const auto status = reply
			->attribute(QNetworkRequest::HttpStatusCodeAttribute)
			.toInt();
		if (status == 404) {
			// релизов ещё нет — это не ошибка
			ShowLatest(strong);
			return;
		}
		if (reply->error() != QNetworkReply::NoError || status != 200) {
			ShowFailed(strong);
			return;
		}
		const auto body = reply->read(kMaxBodyBytes);
		auto error = QJsonParseError();
		const auto document = QJsonDocument::fromJson(body, &error);
		if (error.error != QJsonParseError::NoError || !document.isObject()) {
			ShowFailed(strong);
			return;
		}
		const auto object = document.object();
		if (object.value("draft").toBool() || object.value("prerelease").toBool()) {
			ShowLatest(strong);
			return;
		}
		const auto tag = object.value("tag_name").toString();
		const auto version = ParseVersion(tag);
		if (!version) {
			ShowFailed(strong);
		} else if (version > AppVersion) {
			ShowFound(strong, tag.startsWith('v') ? tag.mid(1) : tag);
		} else {
			ShowLatest(strong);
		}
	});
}

} // namespace Ax::Updates
