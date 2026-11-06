#include "ax/ui/appearance/glass_accent_settings.h"

#include "ax/ui/appearance/glass_accent.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "window/themes/window_theme.h"
#include "ui/style/style_core_palette.h"
#include <rpl/event_stream.h>

namespace AxGlass {
namespace {

constexpr auto kAccentPreference = "axgram.glassAccent";
rpl::event_stream<QColor> Changes;

void Refresh() {
	const auto background = Window::Theme::Background();
	const auto path = background->themeObject().pathAbsolute;
	if (!IsGlassTheme(path)) return;
	auto instance = Window::Theme::Instance();
	if (Window::Theme::LoadFromFile(path, &instance, nullptr, nullptr)) {
		style::main_palette::apply(instance.palette);
		background->appliedEditedPalette();
		style::NotifyPaletteChanged();
	}
}

}

QColor SavedAccent() {
	return QColor(Core::App().settings().readPref<QString>(kAccentPreference));
}

void SaveAccent(QColor color) {
	if (color.isValid()) color.setAlpha(255);
	Core::App().settings().writePref<QString>(kAccentPreference,
		color.isValid() ? color.name(QColor::HexRgb) : QString());
	Core::App().saveSettingsDelayed();
	ConfigureAccent(color);
	Refresh();
	Changes.fire_copy(color);
}

void InitializeAccent() {
	ConfigureAccent(SavedAccent());
	Refresh();
}

rpl::producer<QColor> AccentChanges() {
	return Changes.events();
}

}
