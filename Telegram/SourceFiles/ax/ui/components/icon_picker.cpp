// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ui/components/icon_picker.h"

#include "tray.h"
#include "ax/ax_settings.h"
#include "ax/ui/ax_logo.h"
#include "core/application.h"
#include "main/main_domain.h"
#include "styles/style_ax_styles.h"
#include "ui/painter.h"
#include "window/main_window.h"
#include "window/window_controller.h"

#ifdef Q_OS_WIN
#include "ax/utils/windows_utils.h"
#endif

namespace {

const QVector<QString> icons{
	AxAssets::DEFAULT_ICON,
	AxAssets::ALT_ICON,
	AxAssets::DISCORD_ICON,
	AxAssets::SPOTIFY_ICON,
	AxAssets::NOTHING_ICON,
	AxAssets::BARD_ICON,
	AxAssets::YAPLUS_ICON,
	AxAssets::WIN95_ICON,
};

const auto rows = static_cast<int>(icons.size()) / IconPicker::kColumns
	+ std::min(1, static_cast<int>(icons.size()) % IconPicker::kColumns);

void applyIcon() {
#ifdef Q_OS_WIN
	AxAssets::loadAppIco();
	reloadAppIconFromTaskBar();
#endif

	Window::OverrideApplicationIcon(AxAssets::currentAppLogo());
	Core::App().refreshApplicationIcon();
	Core::App().tray().updateIconCounters();
	Core::App().domain().notifyUnreadBadgeChanged();

	// AxGram: also re-apply the icon to every open window so the taskbar / window
	// icon updates live (AxGram only refreshed the QApplication icon and tray).
	Core::App().enumerateWindows([](not_null<Window::Controller*> window) {
		window->widget()->updateWindowIcon();
	});
}

} // namespace

IconPicker::IconPicker(QWidget *parent, Fn<void()> onChanged)
	: RpWidget(parent)
	, _onChanged(std::move(onChanged)) {
	widthValue() | rpl::on_next([=](int w) {
		const auto cell = w / kColumns;
		const auto iconSize = st::iconPickerIconSize;
		const auto contentSize = iconSize + st::iconPickerImagePadding * 2;
		const auto h = rows * cell - (cell - contentSize);
		resize(w, h);
	}, lifetime());
}

void IconPicker::drawIcon(QPainter &p, const QImage &icon, int x, int y, float strokeOpacity) {
	{
		PainterHighQualityEnabler hq(p);
		p.save();
		p.setPen(QPen(st::boxDividerBg, 0));
		p.setBrush(QBrush(st::boxDividerBg));
		p.setOpacity(strokeOpacity);
		p.drawRoundedRect(
			x + st::iconPickerSelectedPadding,
			y + st::iconPickerSelectedPadding,
			st::iconPickerIconSize + st::iconPickerSelectedPadding * 2,
			st::iconPickerIconSize + st::iconPickerSelectedPadding * 2,
			st::iconPickerSelectedRounding,
			st::iconPickerSelectedRounding
		);
		p.restore();
	}

	const auto rect = QRect(
		x + st::iconPickerImagePadding,
		y + st::iconPickerImagePadding,
		st::iconPickerIconSize,
		st::iconPickerIconSize
	);
	p.drawImage(rect, icon);
}

int IconPicker::cellWidth() const {
	return width() / kColumns;
}

void IconPicker::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto cell = cellWidth();
	const auto iconSize = st::iconPickerIconSize;

	for (int row = 0; row < rows; row++) {
		const auto columns = std::min(kColumns, static_cast<int>(icons.size()) - row * kColumns);
		for (int i = 0; i < columns; i++) {
			auto const idx = i + row * kColumns;

			const auto &iconName = icons[idx];
			if (iconName.isEmpty()) {
				continue;
			}
			QImage icon;
			if (const auto cached = _cachedIcons.find(iconName); cached != _cachedIcons.end()) {
				icon = cached->second;
			} else {
				icon = _cachedIcons[iconName] = AxAssets::loadPreview(iconName);
			}
			auto opacity = 0.0f;
			if (iconName == _wasSelected) {
				opacity = 1.0f - _animation.value(1.0f);
			} else if (iconName == AxAssets::currentAppLogoName()) {
				opacity = _wasSelected.isEmpty() ? 1.0f : _animation.value(1.0f);
			}

			const auto x = i * cell + (cell - iconSize) / 2;
			const auto y = row * cell;

			drawIcon(p, icon, x, y, opacity);
		}
	}
}

void IconPicker::mousePressEvent(QMouseEvent *e) {
	const auto &settings = AxSettings::getInstance();
	auto changed = false;

	const auto cell = cellWidth();
	const auto iconSize = st::iconPickerIconSize;

	for (int row = 0; row < rows; row++) {
		const auto columns = std::min(kColumns, static_cast<int>(icons.size()) - row * kColumns);
		for (int i = 0; i < columns; i++) {
			auto const idx = i + row * kColumns;

			const auto x = i * cell + (cell - iconSize) / 2;
			const auto y = row * cell;

			if (e->pos().x() >= x && e->pos().x() <= x + iconSize
				&& e->pos().y() >= y && e->pos().y() <= y + iconSize) {
				const auto &iconName = icons[idx];
				if (iconName.isEmpty()) {
					break;
				}

				if (settings.appIcon() != iconName) {
					_wasSelected = settings.appIcon();
					_animation.start(
						[=]
						{
							update();
						},
						0.0,
						1.0,
						200,
						anim::easeOutCubic
					);

					AxSettings::getInstance().setAppIcon(iconName);
					changed = true;
					break;
				}
			}
		}
	}

	if (changed) {
		applyIcon();

		repaint();
		if (_onChanged) {
			_onChanged();
		}
	}
}
