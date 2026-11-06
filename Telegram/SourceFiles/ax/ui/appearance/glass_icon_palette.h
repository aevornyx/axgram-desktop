// AxGram glyph palette preserving transparent icon layers. Modified 2026-11-03. GPLv3.
#pragma once

#include "ui/style/style_core_palette.h"

namespace AxGlass {

[[nodiscard]] inline const style::palette &IconPalette(QColor accent, bool activeForeground = false) {
	static auto result = style::palette();
	static auto version = -1;
	static auto previousAccent = QColor();
	static auto previousActiveForeground = false;
	if (version != style::PaletteVersion() || previousAccent != accent
		|| previousActiveForeground != activeForeground) {
		result = *style::main_palette::get();
		for (const auto role : { "menuIconColor", "menuIconFg", "menuIconFgOver",
				"settingsIconFg", "historyComposeIconFg", "historyComposeIconFgOver",
				"windowBoldFg", "windowBoldFgOver", "windowFg", "windowFgOver",
				"boxTextFg", "historyPeerUserpicFg" }) {
			result.setColor(QLatin1String(role), accent);
		}
		if (activeForeground) {
			result.setColor(QLatin1String("windowFgActive"), accent);
		}
		result.finalize();
		version = style::PaletteVersion();
		previousAccent = accent;
		previousActiveForeground = activeForeground;
	}
	return result;
}

} // namespace AxGlass
