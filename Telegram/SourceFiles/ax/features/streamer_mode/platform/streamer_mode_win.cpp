// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.

#include "ax/features/streamer_mode/platform/streamer_mode_win.h"

#include "core/application.h"
#include "window/window_controller.h"

namespace AxFeatures::StreamerMode::Impl {

void enableHook() {
	Core::App().enumerateWindows([&](not_null<Window::Controller*> w) {
		SetWindowDisplayAffinity(w->widget()->psHwnd(), WDA_EXCLUDEFROMCAPTURE);
	});
}

void disableHook() {
	Core::App().enumerateWindows([&](not_null<Window::Controller*> w) {
		SetWindowDisplayAffinity(w->widget()->psHwnd(), WDA_NONE);
	});
}

void hideWidgetWindow(QWidget *widget) {
	auto handle = reinterpret_cast<HWND>(widget->window()->winId());
	SetWindowDisplayAffinity(handle, WDA_EXCLUDEFROMCAPTURE);
}

void showWidgetWindow(QWidget *widget) {
	auto handle = reinterpret_cast<HWND>(widget->window()->winId());
	SetWindowDisplayAffinity(handle, WDA_NONE);
}

}
