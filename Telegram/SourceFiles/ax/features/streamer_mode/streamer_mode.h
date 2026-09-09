// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

class QWidget;

namespace AxFeatures::StreamerMode {

void apply(bool enabled);
void hideWidgetWindow(QWidget *widget);
void showWidgetWindow(QWidget *widget);

} // namespace AxFeatures::StreamerMode
