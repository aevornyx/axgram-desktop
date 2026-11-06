#pragma once

#include "ui/widgets/buttons.h"
#include <rpl/combine.h>
#include "ui/style/style_core.h"
#include "styles/palette.h"
#include <QtGui/QLinearGradient>

namespace Intro {

inline void ApplyGlassButton(not_null<Ui::RoundButton*> button, qreal stroke) {
	button->setFullRadius(true);
	rpl::combine(button->sizeValue(),
		rpl::single(rpl::empty) | rpl::then(style::PaletteChanged())
	) | rpl::on_next([=](QSize size, const auto &) {
		const auto dark = st::windowBg->c.lightnessF() < .5;
		auto gradient = QLinearGradient(0., 0., 0., size.height());
		gradient.setColorAt(0., dark ? QColor(45, 64, 82, 222) : QColor(245, 250, 255, 232));
		gradient.setColorAt(1., dark ? QColor(24, 38, 51, 222) : QColor(220, 235, 248, 232));
		button->setBrushOverride(QBrush(gradient));
		auto border = QPen(dark ? QColor(220, 237, 250, 60) : QColor(255, 255, 255, 235));
		border.setWidthF(stroke);
		button->setPenOverride(border);
		button->setTextFgOverride(st::windowFg->c);
		button->setRippleOverride(dark ? QColor(180, 220, 240, 45) : QColor(55, 110, 155, 35));
	}, button->lifetime());
}

}
