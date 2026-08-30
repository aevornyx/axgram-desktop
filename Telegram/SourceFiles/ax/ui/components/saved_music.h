// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ui/widgets/buttons.h"

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace Data {
class DocumentMedia;
}

namespace Info::Profile {

struct MusicButtonData;

struct ResultCover
{
	QPixmap pix;
	QColor bg;
	bool noCover;
};

class AxMusicButton final : public Ui::RippleButton
{
public:
	AxMusicButton(QWidget *parent, MusicButtonData data, std::optional<QColor> overrideBg, Fn<void()> handler);
	~AxMusicButton();

	void updateData(MusicButtonData data);

	rpl::producer<> onReady() const {
		return _onReady.events();
	}

private:
	void downloadAndMakeCover(FullMsgId msgId);
	void makeCover();

	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

	std::unique_ptr<Ui::FlatLabel> _performer;
	std::unique_ptr<Ui::FlatLabel> _title;
	std::shared_ptr<Data::DocumentMedia> _mediaView;
	std::optional<ResultCover> _currentCover;
	rpl::event_stream<> _onReady;

	QString _performerText;
	QString _titleText;

	std::optional<QColor> _overrideBg;

};

} // namespace Info::Profile
