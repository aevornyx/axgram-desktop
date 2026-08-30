// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ax/features/message_shot/message_shot_theme_state.h"
#include "history/view/history_view_list_widget.h"
#include "ui/chat/chat_style.h"
#include "window/window_session_controller.h"

class HistoryInner;

namespace AxFeatures::MessageShot {

struct ShotConfig
{
	not_null<Window::SessionController*> controller;
	std::shared_ptr<Ui::ChatStyle> st;
	std::vector<not_null<HistoryItem*>> messages;
};

enum RenderPart
{
	Date,
	Reactions,
	HeaderDecorations,
};

void setShotConfig(ShotConfig &config);
void resetShotConfig();
ShotConfig getShotConfig();

bool ignoreRender(RenderPart part);
bool isTakingShot();

bool isChoosingTheme();
bool setChoosingTheme(bool val);

// util
QColor makeDefaultBackgroundColor();

void Make(not_null<QWidget*> box, const ShotConfig &config, const Fn<void(QImage&,bool)>& callback);

void Wrapper(not_null<HistoryView::ListWidget*> widget, Fn<void()> clearSelected);
void Wrapper(not_null<HistoryInner*> widget, Fn<void()> clearSelected);

}
