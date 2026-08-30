// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ax/features/message_shot/message_shot.h"
#include "ui/layers/box_content.h"

class MessageShotBox : public Ui::BoxContent
{
public:
	MessageShotBox(QWidget *parent, AxFeatures::MessageShot::ShotConfig config);

	bool tookShot() const {
		return _tookShot;
	}

protected:
	void prepare() override;

private:
	void setupContent();

	AxFeatures::MessageShot::ShotConfig _config;
	std::shared_ptr<style::palette> _selectedPalette;

	bool _tookShot = false;
};
