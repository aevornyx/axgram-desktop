// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ui/layers/box_content.h"
#include "ui/style/style_core_palette.h"
#include "window/window_session_controller.h"

using Callback = Fn<void(style::palette &)>;

class ThemeSelectorBox : public Ui::BoxContent
{
public:
	ThemeSelectorBox(QWidget *parent, not_null<Window::SessionController*> controller);

	rpl::producer<style::palette> paletteSelected();
	rpl::producer<QString> themeNameChanged();

protected:
	void prepare() override;

private:
	void setupContent();

	not_null<Window::SessionController*> _controller;

	rpl::event_stream<style::palette> _palettes;
	rpl::event_stream<QString> _themeNames;

	style::palette _selectedPalette;
};
