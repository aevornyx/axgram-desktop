#include "ax/ui/appearance/glass_rail.h"
#include "ax/ui/appearance/glass_accent.h"

#include <QtGui/QLinearGradient>
#include <QtGui/QPainter>
#include <algorithm>

namespace AxGlass {
namespace {

[[nodiscard]] QColor WithAlpha(QColor color, qreal opacity) {
	color.setAlphaF(opacity);
	return color;
}

}

Palette Colors(bool dark) {
	return dark ? Palette{
		QColor(0x10, 0x16, 0x1d),
		QColor(0x18, 0x21, 0x2a),
		QColor(0xea, 0xf0, 0xf6),
		QColor(0xa0, 0xad, 0xbb),
		CurrentAccent(true),
	} : Palette{
		QColor(0xf1, 0xf5, 0xfa),
		QColor(0xf7, 0xfa, 0xfd),
		QColor(0x17, 0x22, 0x34),
		QColor(0x60, 0x71, 0x85),
		CurrentAccent(false),
	};
}

QPainterPath RailOutline(const RailMetrics &m) {
	const auto left = m.inset;
	const auto right = m.width - m.inset;
	const auto top = m.header + m.neck;
	const auto edge = std::max(m.edge, top);
	auto result = QPainterPath();
	result.moveTo(right, -m.stroke);
	result.cubicTo(right, m.header * .85, left, m.header * .95, left, top);
	result.lineTo(left, edge);
	result.cubicTo(
		left,
		edge + m.depth * .5,
		right,
		edge + m.depth * .5,
		right,
		edge + m.depth);
	result.lineTo(right, std::max(m.height, edge + m.depth) + m.stroke);
	result.lineTo(m.width + m.stroke, std::max(m.height, edge + m.depth) + m.stroke);
	result.lineTo(m.width + m.stroke, -m.stroke);
	result.closeSubpath();
	return result;
}

void PaintRail(QPainter &p, const RailMetrics &m, const Palette &colors) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.fillRect(QRectF(0., 0., m.width, m.height), colors.background);
	auto fill = QLinearGradient(0., 0., m.width, m.height);
	fill.setColorAt(0., WithAlpha(colors.surface, .48));
	fill.setColorAt(1., WithAlpha(colors.surface, .08));
	p.setBrush(fill);
	auto line = QPen(WithAlpha(colors.muted, .26));
	line.setWidthF(m.stroke);
	p.setPen(line);
	p.drawPath(RailOutline(m));
	p.restore();
}

QBrush PanelBrush(const QRectF &rect, QColor base, bool dark) {
	// A restrained reflection shared by the search and conversation headers.
	// All stops are opaque: partial repainting cannot accumulate the highlight.
	const auto strength = dark ? .035 : .22;
	const auto highlight = QColor::fromRgbF(
		base.redF() + (1. - base.redF()) * strength,
		base.greenF() + (1. - base.greenF()) * strength,
		base.blueF() + (1. - base.blueF()) * strength);
	auto fill = QLinearGradient(rect.topLeft(), rect.bottomLeft());
	fill.setColorAt(0., highlight);
	fill.setColorAt(1., base);
	return QBrush(fill);
}

void PaintPanel(QPainter &p, const QRectF &rect, QColor base, bool dark) {
	p.fillRect(rect, PanelBrush(rect, base, dark));
}

void PaintCard(
		QPainter &p,
		const QPainterPath &outline,
		QColor background,
		QColor foreground,
		qreal stroke,
		qreal hover) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(background);
	auto border = QPen(WithAlpha(foreground, .035 + .045 * std::clamp(hover, 0., 1.)));
	border.setWidthF(stroke);
	p.setPen(border);
	p.drawPath(outline);
	p.restore();
}

void PaintSelection(
		QPainter &p,
		const QPainterPath &outline,
		const QBrush &background,
		QColor foreground,
		qreal stroke,
		bool active) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(background);
	if (active) {
		auto border = QPen(WithAlpha(foreground, .08));
		border.setWidthF(stroke);
		p.setPen(border);
	} else {
		p.setPen(Qt::NoPen);
	}
	p.drawPath(outline);
	p.restore();
}

void PaintButton(
		QPainter &p,
		QRectF rect,
		qreal stroke,
		const Palette &colors,
		bool active,
		bool circular,
		bool utility) {
	if (!active && !utility) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	auto fill = QLinearGradient(rect.topLeft(), rect.bottomRight());
	fill.setColorAt(0., WithAlpha(colors.accent, active ? .15 : .10));
	fill.setColorAt(1., WithAlpha(colors.accent, active ? .10 : .06));
	p.setBrush(fill);
	auto line = QPen(WithAlpha(colors.text, active ? .09 : 0.));
	line.setWidthF(stroke);
	p.setPen(line);
	if (circular) {
		p.drawEllipse(rect);
	} else {
		const auto radius = std::min(rect.width(), rect.height()) * .28;
		p.drawRoundedRect(rect, radius, radius);
	}
	p.restore();
}

}
