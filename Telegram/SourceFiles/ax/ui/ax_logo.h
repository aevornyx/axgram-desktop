// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#define ICON(name, value) const auto name##_ICON = QStringLiteral(value)

namespace AxAssets {

ICON(DEFAULT, "default");
ICON(ALT, "alt");
ICON(DISCORD, "discord");
ICON(SPOTIFY, "spotify");
ICON(NOTHING, "nothing");
ICON(BARD, "bard");
ICON(YAPLUS, "yaplus");
ICON(WIN95, "win95");

void loadAppIco();
QString appIcoPath();

QImage loadPreview(const QString& name);

QString currentAppLogoName();
QImage currentAppLogo();
QImage currentAppLogoPad();

}
