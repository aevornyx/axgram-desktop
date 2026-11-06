#pragma once

#include <QtGui/QColor>
#include <QtGui/QBrush>
#include <QtGui/QPainterPath>

class QPainter;

namespace AxGlass {

struct Palette {
	QColor background;
	QColor surface;
	QColor text;
	QColor muted;
	QColor accent;
};

struct RailMetrics {
	qreal width = 0.;
	qreal height = 0.;
	qreal header = 0.;
	qreal edge = 0.;
	qreal inset = 0.;
	qreal neck = 0.;
	qreal depth = 0.;
	qreal stroke = 0.;
};

[[nodiscard]] Palette Colors(bool dark);
[[nodiscard]] QPainterPath RailOutline(const RailMetrics &metrics);
void PaintRail(QPainter &p, const RailMetrics &metrics, const Palette &colors);
[[nodiscard]] QBrush PanelBrush(const QRectF &rect, QColor base, bool dark);
void PaintCard(
	QPainter &p,
	const QPainterPath &outline,
	QColor background,
	QColor foreground,
	qreal stroke,
	qreal hover);
void PaintPanel(QPainter &p, const QRectF &rect, QColor base, bool dark);
void PaintSelection(
	QPainter &p,
	const QPainterPath &outline,
	const QBrush &background,
	QColor foreground,
	qreal stroke,
	bool active);
void PaintButton(
	QPainter &p,
	QRectF rect,
	qreal stroke,
	const Palette &colors,
	bool active,
	bool circular,
	bool utility = false);

}
