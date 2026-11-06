#pragma once

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QLinearGradient>
#include <algorithm>
#include <array>

namespace Intro::Brand {

[[nodiscard]] inline qreal Progress(qreal time, qreal start, qreal duration) {
	return std::clamp((time - start) / duration, qreal(0.), qreal(1.));
}

[[nodiscard]] inline qreal Ease(qreal value) {
	const auto remaining = 1. - value;
	return 1. - remaining * remaining * remaining;
}

[[nodiscard]] inline qreal Channel(qreal time, qreal start, qreal duration) {
	return Ease(Progress(time, start, duration));
}

inline void PaintMark(QPainter &p, QRectF rect, bool dark, qreal elapsed) {
	static const auto paths = [] {
		auto result = std::array<QPainterPath, 3>();
		const auto points = std::array{
			QVector<QPointF>{ {47,10}, {55,10}, {94,90}, {77,90}, {50,35}, {39,56}, {22,56} },
			QVector<QPointF>{ {16,72}, {33,72}, {24,90}, {6,90} },
			QVector<QPointF>{ {15,57}, {54,57}, {61,71}, {9,71} },
		};
		for (auto i = 0; i != result.size(); ++i) {
			result[i].addPolygon(QPolygonF(points[i]));
			result[i].closeSubpath();
		}
		return result;
	}();
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.translate(rect.topLeft());
	p.scale(rect.width() / 100., rect.height() / 100.);
	p.setPen(Qt::NoPen);
	for (auto i = 0; i != paths.size(); ++i) {
		const auto amount = Channel(elapsed, i == 0 ? 100. : i == 1 ? 160. : 280., i == 2 ? 460. : 540.);
		const auto remaining = 1. - amount;
		p.save();
		p.setOpacity(p.opacity() * amount);
		p.translate((i == 0 ? -100. : i == 1 ? 105. : -130.) * remaining,
			(i == 0 ? 110. : i == 1 ? -115. : 14.) * remaining);
		p.translate(50., 50.);
		p.rotate((i == 0 ? -62. : i == 1 ? 60. : 0.) * remaining);
		p.scale(i == 2 ? 1. - .7 * remaining : 1. - .65 * remaining,
			i == 2 ? 1. : 1. - .65 * remaining);
		p.translate(-50., -50.);
		auto gradient = QLinearGradient(0., 0., 80., 100.);
		if (i == 0) {
			gradient.setColorAt(0., QColor(dark ? 0xe5eff8 : 0x586b83));
			gradient.setColorAt(1., QColor(dark ? 0x91abc3 : 0x24374c));
		} else {
			gradient.setColorAt(0., QColor(dark ? 0x79b9ff : 0x8ac0ff));
			gradient.setColorAt(1., QColor(dark ? 0xa9d2ff : 0xb6d9ff));
			if (i == 2) {
				gradient = QLinearGradient(9., 57., 61., 71.);
				gradient.setColorAt(0., QColor(dark ? 0xd9ecff : 0xdfefff));
				gradient.setColorAt(.24, QColor(dark ? 0x79b9ff : 0x8ac0ff));
				gradient.setColorAt(.75, QColor(dark ? 0xd9ecff : 0xdfefff));
			}
		}
		p.setBrush(gradient);
		p.drawPath(paths[i]);
		p.restore();
	}
	p.restore();
}

inline void PaintWallpaper(QPainter &p, QRectF rect, const QImage &image, QColor base, qreal opacity, qreal elapsed) {
	p.fillRect(rect, base);
	if (image.isNull() || rect.isEmpty()) {
		return;
	}
	const auto zoom = 1. + .035 * (1. - Channel(elapsed, 0., 1400.));
	const auto scale = std::max(rect.width() / image.width(), rect.height() / image.height()) * zoom;
	const auto size = QSizeF(image.size()) * scale;
	p.save();
	p.setClipRect(rect);
	p.setOpacity(opacity);
	p.drawImage(QRectF(rect.center() - QPointF(size.width(), size.height()) / 2., size), image);
	p.restore();
}

}
