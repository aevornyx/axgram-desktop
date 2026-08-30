// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ui/rp_widget.h"
#include "ui/effects/animations.h"

class IconPicker : public Ui::RpWidget
{
public:
	IconPicker(QWidget *parent);
	~IconPicker() = default;

	static constexpr int kColumns = 4;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	void drawIcon(QPainter &p, const QImage &icon, int x, int y, float strokeOpacity);
	[[nodiscard]] int cellWidth() const;

	Ui::Animations::Simple _animation;
	QString _wasSelected;
	std::unordered_map<QString, QImage> _cachedIcons;
};
