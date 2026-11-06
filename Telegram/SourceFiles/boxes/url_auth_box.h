/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
struct HistoryMessageMarkupButton;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
class Show;
} // namespace Ui

namespace UrlAuthBox {

struct ServiceCallbacks {
	QString clientId;
	QString origin;
	qint64 account = 0;
	Fn<bool()> active;
	Fn<void(QString)> accepted;
	Fn<void()> cancelled;
	Fn<void()> failed;
	Fn<void(Fn<void()>)> bindClose;
	Fn<void()> expired;
};

void ActivateService(
	std::shared_ptr<Ui::Show> show,
	not_null<Main::Session*> session,
	const QString &url,
	std::shared_ptr<ServiceCallbacks> callbacks);

void ActivateButton(
	std::shared_ptr<Ui::Show> show,
	not_null<const HistoryItem*> message,
	int row,
	int column);
void ActivateUrl(
	std::shared_ptr<Ui::Show> show,
	not_null<Main::Session*> session,
	const QString &url,
	QVariant context);

} // namespace UrlAuthBox
