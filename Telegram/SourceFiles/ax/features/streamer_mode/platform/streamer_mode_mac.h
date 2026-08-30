// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

namespace AxFeatures::StreamerMode::Impl {

void enableHook();
void disableHook();
void hideWidgetWindow(QWidget *widget);
void showWidgetWindow(QWidget *widget);

}