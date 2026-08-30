// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include <QString>

namespace Window {
class SessionController;
}

namespace Ui {

class GenericBox;

struct PluginMetadata {
	QString id;
	QString name;
	QString description;
	QString author;
	QString version;
	QString icon;
	QString minVersion;
	QStringList requirements;
};

[[nodiscard]] PluginMetadata ParsePluginMetadata(const QByteArray &data);

void ShowPluginInfoBox(
	not_null<Window::SessionController*> controller,
	const QString& pluginPath,
	PluginMetadata metadata);

} // namespace Ui
