#include "ax/ui/appearance/glass_accent.h"

#include "base/basic_types.h"
#include "ui/style/style_core_palette.h"
#include <algorithm>
#include <cmath>

namespace AxGlass {
namespace {

QColor SelectedAccent;

double Luminance(QColor color) {
	const auto linear = [](double value) {
		return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4);
	};
	return .2126 * linear(color.redF()) + .7152 * linear(color.greenF())
		+ .0722 * linear(color.blueF());
}

QColor DefaultAccent(bool dark) {
	return dark ? QColor(0x85, 0xcc, 0xd9) : QColor(0x2d, 0x74, 0xab);
}

QColor ColorOn(QColor background) {
	const auto dark = QColor(Qt::black);
	const auto light = QColor(0xff, 0xff, 0xff);
	return Contrast(dark, background) > Contrast(light, background) ? dark : light;
}

bool AccentRole(QLatin1String name, QColor color) {
	const auto key = QString(name);
	if (key.contains(u"Muted") || key.contains(u"Shadow") || key.contains(u"SubText")
		|| key.contains(u"Disabled") || key.contains(u"Placeholder") || key.contains(u"Hint")
		|| key.contains(u"Peer") || key.startsWith(u"msgFile")
		|| key.startsWith(u"premium") || key.contains(u"Error")
		|| key.contains(u"Good") || key.contains(u"Attention")) {
		return false;
	}
	const auto hue = color.hslHue();
	if (hue < 170 || hue > 235) return false;
	const auto surfaceAccent = key.contains(u"Active") || key.contains(u"Selected")
		|| key.startsWith(u"msgOutBg");
	return surfaceAccent
		|| (color.hsvSaturationF() >= .15 && color.lightnessF() > .30);
}

QColor Recolor(QColor original, QColor selected, bool dark) {
	const auto base = DefaultAccent(dark);
	const auto target = Accent(selected, dark);
	const auto oldLight = base.lightnessF();
	const auto newLight = target.lightnessF();
	const auto sourceLight = original.lightnessF();
	const auto light = sourceLight < oldLight
		? sourceLight * newLight / oldLight
		: newLight + (sourceLight - oldLight) * (1. - newLight) / (1. - oldLight);
	const auto saturation = std::clamp<double>(original.hslSaturationF()
		* target.hslSaturationF() / base.hslSaturationF(), 0., 1.);
	return QColor::fromHslF(std::max<double>(target.hslHueF(), 0.), saturation,
		std::clamp(light, 0., 1.), original.alphaF());
}

}

double Contrast(QColor first, QColor second) {
	const auto a = Luminance(first);
	const auto b = Luminance(second);
	return (std::max(a, b) + .05) / (std::min(a, b) + .05);
}

QColor Accent(QColor selected, bool dark) {
	if (!selected.isValid()) return DefaultAccent(dark);
	selected.setAlpha(255);
	const auto background = dark ? QColor(0x18, 0x21, 0x2a) : QColor(0xf7, 0xfa, 0xfd);
	if (Contrast(selected, background) >= 4.5) return selected;
	const auto hue = std::max<double>(selected.hslHueF(), 0.);
	for (auto step = 1; step <= 100; ++step) {
		const auto light = selected.lightnessF()
			+ (dark ? 1. - selected.lightnessF() : -selected.lightnessF()) * step / 100.;
		const auto adjusted = QColor::fromHslF(hue, selected.hslSaturationF(), light);
		if (Contrast(adjusted, background) >= 4.5) return adjusted;
	}
	return dark ? QColor(Qt::white) : QColor(Qt::black);
}

QColor CurrentAccent(bool dark) {
	return Accent(SelectedAccent, dark);
}

void ConfigureAccent(QColor selected) {
	SelectedAccent = selected;
}

bool IsGlassTheme(const QString &path) {
	return path == QStringLiteral(":/gui/axgram-glass-dark.tdesktop-theme")
		|| path == QStringLiteral(":/gui/axgram-glass-light.tdesktop-theme");
}

void TintPalette(style::palette &palette, QColor selected, bool dark) {
	if (!selected.isValid()) return;
	palette.finalize();
	for (const auto &row : style::main_palette::data()) {
		const auto index = style::internal::GetPaletteIndex(row.name);
		if (index < 0) continue;
		const auto original = palette.colorAtIndex(index)->c;
		if (AccentRole(row.name, original)) {
			palette.setColor(row.name, Recolor(original, selected, dark));
		}
	}
	for (const auto &[foreground, background] : {
		std::pair{ "windowFgActive", "windowBgActive" },
		std::pair{ "activeButtonFg", "activeButtonBg" },
		std::pair{ "activeButtonFgOver", "activeButtonBgOver" },
		std::pair{ "dialogsUnreadFg", "dialogsUnreadBg" },
		std::pair{ "dialogsUnreadFgActive", "dialogsUnreadBgActive" },
		std::pair{ "profileVerifiedCheckFg", "profileVerifiedCheckBg" },
		std::pair{ "overviewCheckFgActive", "overviewCheckBgActive" }
	}) {
		const auto bg = style::internal::GetPaletteIndex(QLatin1String(background));
		if (bg >= 0) palette.setColor(QLatin1String(foreground), ColorOn(palette.colorAtIndex(bg)->c));
	}
}

}
