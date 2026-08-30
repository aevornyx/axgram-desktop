// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.

#include "ax/features/streamer_mode/streamer_mode.h"

#include "window/window_controller.h"

#if defined Q_OS_WINRT || defined Q_OS_WIN
#include "ax/features/streamer_mode/platform/streamer_mode_win.h"
#elif defined Q_OS_MAC // Q_OS_WINRT || Q_OS_WIN
#include "ax/features/streamer_mode/platform/streamer_mode_mac.h"
#else // Q_OS_WINRT || Q_OS_WIN || Q_OS_MAC
#include "ax/features/streamer_mode/platform/streamer_mode_linux.h"
#endif // else for Q_OS_WINRT || Q_OS_WIN || Q_OS_MAC

namespace AxFeatures::StreamerMode
{

bool isEnabledVal;

bool isEnabled()
{
	return isEnabledVal;
}

void enable()
{
	isEnabledVal = true;
	Impl::enableHook();
}

void disable()
{
	isEnabledVal = false;
	Impl::disableHook();
}

void hideWidgetWindow(QWidget *widget)
{
	Impl::hideWidgetWindow(widget);
}

void showWidgetWindow(QWidget *widget)
{
	Impl::showWidgetWindow(widget);
}

}
