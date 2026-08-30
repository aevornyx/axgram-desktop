// Часть AxGram для Desktop.
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ax_promo.h"

#include "ax/ui/toasts.h"
#include "ax/utils/ax_update_check.h"
#include "core/application.h"
#include "lang_auto.h"
#include "ui/toast/toast.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QDesktopServices>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace Ax::Promo {
namespace {

// Показывать редко и не сразу: встречать нового пользователя напоминанием —
// верный способ получить удаление. Тост, а не окно: работу не перекрывает.
constexpr auto kIntervalDays = 3;
constexpr auto kFirstDelayDays = 3;
constexpr auto kShowAfterStartMs = 25 * 1000;
constexpr auto kRetryMs = 20 * 1000;
constexpr auto kMaxRetries = 6;

constexpr auto kKindChannel = 0;
constexpr auto kKindSupport = 1;
constexpr auto kKindCount = 2;

// Канал проекта. Пусто — напоминание про канал не показывается.
constexpr auto kChannel = "axgram_channel";

bool Shown = false;

[[nodiscard]] QString StatePath() {
	return cWorkingDir() + u"tdata/ax_promo.json"_q;
}

[[nodiscard]] QJsonObject ReadState() {
	auto file = QFile(StatePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	const auto document = QJsonDocument::fromJson(file.readAll());
	return document.isObject() ? document.object() : QJsonObject();
}

void WriteState(const QJsonObject &state) {
	auto file = QFile(StatePath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
	}
}

[[nodiscard]] qint64 Now() {
	return QDateTime::currentMSecsSinceEpoch();
}

void Open(const QString &domain) {
	QDesktopServices::openUrl(
		QUrl(QString("tg://resolve?domain=%1").arg(domain)));
}

bool ShowNow() {
	const auto window = Core::App().activeWindow();
	const auto controller = window ? window->sessionController() : nullptr;
	if (!controller) {
		return false;  // ещё не вошли или окно не готово — попробуем позже
	}

	auto state = ReadState();
	if (state.value("disabled").toBool()) {
		return true;  // выключено — больше не пытаемся
	}

	const auto now = Now();
	const auto day = qint64(24) * 60 * 60 * 1000;

	const auto firstRun = qint64(state.value("firstRun").toDouble());
	if (!firstRun) {
		state["firstRun"] = double(now);
		WriteState(state);
		return true;
	}
	if (now - firstRun < kFirstDelayDays * day) {
		return true;
	}
	if (now - qint64(state.value("lastShown").toDouble()) < kIntervalDays * day) {
		return true;
	}

	auto kind = state.value("nextKind").toInt(kKindChannel);
	const auto supportReady = Ax::Updates::SupportAvailable();
	const auto channelReady = (*kChannel != 0);
	if (kind == kKindSupport && !supportReady) {
		kind = kKindChannel;
	}
	if (kind == kKindChannel && !channelReady) {
		kind = supportReady ? kKindSupport : -1;
	}
	if (kind < 0) {
		return true;  // показывать нечего
	}

	const auto support = (kind == kKindSupport);
	Ax::Ui::ShowToastWithAction(
		::Ui::Toast::Config{
			.text = { support
				? tr::ax_PromoSupport(tr::now)
				: tr::ax_PromoChannel(tr::now) },
			.duration = 8 * crl::time(1000),
		},
		tr::ax_PromoOpen(tr::now),
		[=] { Open(support ? Ax::Updates::kSupportBot : kChannel); });

	state["lastShown"] = double(now);
	state["nextKind"] = (kind + 1) % kKindCount;
	WriteState(state);
	return true;
}

void Attempt(int left) {
	if (Shown) {
		return;
	}
	if (ShowNow()) {
		Shown = true;
		return;
	}
	if (left > 0) {
		QTimer::singleShot(kRetryMs, [=] { Attempt(left - 1); });
	}
}

} // namespace

void Schedule() {
	QTimer::singleShot(kShowAfterStartMs, [] { Attempt(kMaxRetries); });
}

} // namespace Ax::Promo
