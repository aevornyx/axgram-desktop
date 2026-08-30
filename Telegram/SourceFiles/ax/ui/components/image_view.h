// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ui/rp_widget.h"
#include "ui/effects/animations.h"

class ImageView : public Ui::RpWidget
{
public:
	ImageView(QWidget *parent);

	void setImage(const QImage &image);
	QImage getImage() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

	void computeDiffImages(const QImage &prev, const QImage &curr);

private:
	QImage image;
	QImage prevImage;
	QImage baseImage;
	QImage prevDiffImage;
	QImage newDiffImage;

	Ui::Animations::Simple animation;

};
