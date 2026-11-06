// AxGram native design controls. Modified 2026-11-03. GPLv3.
#include "ax/ui/appearance/glass_controls.h"
#include "ax/ui/appearance/glass_icon_animation.h"
#include "ax/ui/appearance/glass_icon_palette.h"

#include "ax/ax_settings.h"
#include "ax/ui/appearance/glass_rail.h"
#include "base/event_filter.h"
#include "lottie/lottie_icon.h"
#include "ui/painter.h"
#include "ui/power_saving.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "window/themes/window_theme.h"
#include "styles/style_glass_rail.h"
#include "styles/style_menu_icons.h"
#include "styles/style_ax_icons.h"
#include "styles/style_widgets.h"
#include "styles/style_settings.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QEvent>
#include <QtGui/QPaintEvent>
#include <cmath>

namespace AxGlass {
namespace {

constexpr auto kHoverDuration = crl::time(150);
constexpr auto kGlyphDuration = crl::time(420);

class SettingsButton final : public Ui::SettingsButton {
public:
	SettingsButton(
		QWidget *parent,
		rpl::producer<QString> text,
		const style::SettingsButton &style)
	: Ui::SettingsButton(parent, std::move(text), style) {
		PowerSaving::Changes(
		) | rpl::on_next([=] { _hover.stop(); update(); }, lifetime());
		AxSettings::getInstance().glassFoldersChanges(
		) | rpl::on_next([=] { _hover.stop(); update(); }, lifetime());
	}

protected:
	void onStateChanged(State was, StateChangeSource source) override {
		Ui::SettingsButton::onStateChanged(was, source);
		const auto over = (isOver() || isDown()) && !isDisabled();
		if (_over == over) {
			return;
		}
		const auto from = _hover.value(_over ? 1. : 0.);
		_over = over;
		if (!MotionAllowed()) {
			_hover.stop();
			update();
		} else {
			_hover.start([=] { update(); }, from, over ? 1. : 0.,
				kHoverDuration, anim::sineInOut);
		}
	}

	void paintEvent(QPaintEvent *event) override {
		if (!Enabled()) {
			Ui::SettingsButton::paintEvent(event);
			return;
		}
		auto p = Painter(this);
		p.fillRect(event->rect(), st().textBg);
		const auto degree = MotionAllowed()
			? _hover.value(_over ? 1. : 0.)
			: (_over ? 1. : 0.);
		const auto bounds = QRectF(rect()).adjusted(
			st::axGlassControlInset, st::axGlassControlVerticalInset,
			-st::axGlassControlInset, -st::axGlassControlVerticalInset);
		auto outline = QPainterPath();
		outline.addRoundedRect(
			bounds, st::axGlassControlRadius, st::axGlassControlRadius);
		PaintCard(p, outline, anim::color(st().textBg, st().textBgOver, degree),
			st::windowFg->c, st::lineWidth, degree);
		p.save();
		p.setClipPath(outline, Qt::IntersectClip);
		paintRipple(p, 0, 0);
		p.restore();
		const auto over = (isOver() || isDown()) && !isDisabled();
		paintText(p, over, width());
		paintToggle(p, width());
	}

private:
	Ui::Animations::Simple _hover;
	bool _over = false;

};

[[nodiscard]] QString AnimationName(const style::internal::Icon *source) {
	const auto pairs = {
		std::pair{ &st::menuIconPalette, u"settings"_q },
		std::pair{ &st::menuIconProfile, u"user"_q },
		std::pair{ &st::menuIconNotifications, u"notification"_q },
		std::pair{ &st::menuIconLock, u"lock"_q },
		std::pair{ &st::menuIconChatBubble, u"message-circle"_q },
		std::pair{ &st::menuIconShowInFolder, u"folder"_q },
		std::pair{ &st::menuIconManage, u"settings"_q },
		std::pair{ &st::menuIconUnmute, u"phone"_q },
		std::pair{ &st::menuIconPowerUsage, u"power"_q },
		std::pair{ &st::menuIconTranslate, u"globe"_q },
		std::pair{ &st::menuIconPremium, u"star"_q },
		std::pair{ &st::menuIconSettings, u"settings"_q },
		std::pair{ &st::menuIconTagFilter, u"filter"_q },
		std::pair{ &st::menuIconGroupReactions, u"shield"_q },
		std::pair{ &st::menuIconDevices, u"monitor"_q },
		std::pair{ &st::menuIconDownload, u"download"_q },
		std::pair{ &st::menuIconEdit, u"edit"_q },
		std::pair{ &st::menuIconTagEdit, u"edit"_q },
		std::pair{ &st::menuIconArchive, u"archive"_q },
		std::pair{ &st::menuIconArchiveOpen, u"archive"_q },
		std::pair{ &st::menuIconStoriesArchiveSection, u"archive"_q },
		std::pair{ &st::menuIconPhone, u"phone"_q },
		std::pair{ &st::menuIconRecoveryEmail, u"mail"_q },
		std::pair{ &st::menuIconRatingUsers, u"users"_q },
		std::pair{ &st::menuIconVideoChat, u"video"_q },
		std::pair{ &st::menuIconStar, u"star"_q },
		std::pair{ &st::menuIconSearch, u"search-to-x"_q },
		std::pair{ &st::menuIconGroups, u"users"_q },
		std::pair{ &st::menuIconChannel, u"speaker"_q },
		std::pair{ &st::menuIconUserShow, u"user"_q },
		std::pair{ &st::menuIconSavedMessages, u"bookmark"_q },
		std::pair{ &st::menuIconNightMode, u"moon"_q },
		std::pair{ &st::axLReadMenuIcon, u"mail"_q },
		std::pair{ &st::axSReadMenuIcon, u"mail"_q }
	};
	for (const auto &[icon, name] : pairs) {
		if (source == icon) {
			return name;
		}
	}
	return {};
}

[[nodiscard]] QString Interaction(const QString &name) {
	static const auto modes = [] {
		auto file = QFile(u":/gui/ax-animations/modes.json"_q);
		return file.open(QIODevice::ReadOnly)
			? QJsonDocument::fromJson(file.readAll()).object()
			: QJsonObject();
	}();
	return modes.value(name).toString();
}

} // namespace

bool Enabled() {
	return AxSettings::getInstance().glassFolders();
}

bool MotionAllowed() {
	return Enabled() && !anim::Disabled() && !On(PowerSaving::kAnimations);
}

object_ptr<Ui::SettingsButton> CreateSettingsButton(
		QWidget *parent,
		rpl::producer<QString> text,
		const style::SettingsButton &style) {
	return object_ptr<SettingsButton>(parent, std::move(text), style);
}

void SetupSettingsIcon(
		not_null<Ui::AbstractButton*> button,
		not_null<Ui::RpWidget*> canvas,
		const style::internal::Icon *source,
		Fn<void(QPainter&)> fallback) {
	struct State {
		std::unique_ptr<Lottie::Icon> icon;
		std::unique_ptr<style::icon> fallbackIcon;
		int fallbackPaletteVersion = -1;
		QColor fallbackAccent;
		bool attempted = false;
		Ui::Animations::Simple nudge;
		bool hovered = false;
	};
	const auto state = canvas->lifetime().make_state<State>();
	const auto attention = source == &st::menuIconDeleteAttention
		|| source == &st::menuIconLeaveAttention
		|| source == &st::menuIconCommunityRemoveAttention
		|| source == &st::menuIconDisableAttention
		|| source == &st::menuIconReportAttention
		|| source == &st::menuIconRestoreAttention
		|| source == &st::menuIconTagRemoveAttention
		|| source == &st::menuIconCancelAttention
		|| source == &st::menuIconBlockAttention
		|| source == &st::menuIconRemoveAttention;
	const auto name = AnimationName(source);
	const auto mode = Interaction(name);
	const auto prepare = [=] {
		if (!state->attempted && Enabled() && !name.isEmpty()) {
			state->attempted = true;
			state->icon = Lottie::MakeIcon({
				.path = u":/gui/ax-animations/"_q + name + u".json"_q,
				.sizeOverride = canvas->size(),
				.limitFps = true,
				.colorizeUsingAlpha = true,
			});
			if (!state->icon->valid()) {
				state->icon = nullptr;
			}
		}
	};
	const auto play = [=](bool forward) {
		prepare();
		if (!state->icon || !MotionAllowed() || button->isDisabled()) {
			return;
		}
		const auto last = state->icon->framesCount() - 1;
		if (last <= 0) {
			return;
		}
		const auto from = state->icon->frameIndex();
		const auto to = forward ? last : 0;
		const auto duration = std::max(crl::time(1),
			kGlyphDuration * std::abs(to - from) / last);
		state->icon->animate([=] { canvas->update(); }, from, to, duration);
	};
	const auto nudge = [=](bool over) {
		const auto from = state->nudge.value(state->hovered ? 1. : 0.);
		state->hovered = over;
		if (MotionAllowed() && !button->isDisabled()) {
			const auto duration = over && (mode == u"click-toggle"_q || name.isEmpty())
				? kGlyphDuration : kHoverDuration;
			state->nudge.start([=] { canvas->update(); },
				from, over ? 1. : 0., duration, anim::sineInOut);
		} else {
			state->nudge.stop();
			canvas->update();
		}
	};
	const auto reset = [=] {
		state->nudge.stop();
		state->hovered = false;
		if (state->icon) {
			state->icon->jumpTo(0, [=] { canvas->update(); });
		}
		canvas->update();
	};
	base::install_event_filter(canvas, button, [=](not_null<QEvent*> e) {
		switch (e->type()) {
		case QEvent::Enter:
		case QEvent::FocusIn:
			nudge(true);
			prepare();
			if (state->icon
				&& mode != u"click-toggle"_q
				&& MotionAllowed()
				&& !button->isDisabled()) {
				ReplayIcon(state->icon.get(), [=] { canvas->update(); }, kGlyphDuration);
			}
			break;
		case QEvent::Leave:
		case QEvent::FocusOut:
			nudge(false);
			if (mode != u"click-toggle"_q) {
				play(false);
			}
			break;
		case QEvent::Hide:
		case QEvent::EnabledChange:
			reset();
			break;
		default: break;
		}
		return base::EventFilterResult::Continue;
	});
	AxSettings::getInstance().glassFoldersChanges(
	) | rpl::on_next(reset, canvas->lifetime());
	style::PaletteChanged(
	) | rpl::on_next(reset, canvas->lifetime());
	PowerSaving::Changes(
	) | rpl::on_next(reset, canvas->lifetime());
	canvas->paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(canvas);
		if (!Enabled()) {
			fallback(p);
			return;
		}
		const auto colors = Colors(st::windowBg->c.lightnessF() < .5);
		prepare();
		if (state->icon && !MotionAllowed() && state->icon->animating()) {
			state->icon->jumpTo(0, [=] { canvas->update(); });
		}
		const auto degree = MotionAllowed()
			? state->nudge.value(state->hovered ? 1. : 0.) : 0.;
		const auto scale = 1. + .08 * degree;
		const auto center = QPointF(canvas->width() / 2., canvas->height() / 2.);
		p.save();
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		p.setRenderHint(QPainter::Antialiasing);
		p.translate(center);
		p.scale(scale, scale);
		if (mode == u"click-toggle"_q || !state->icon) {
			p.rotate(-6. * std::sin(degree * 3.14159265358979323846));
		}
		p.translate(-center);
		if (button->isDisabled()) {
			p.setOpacity(.45);
		}
		if (state->icon) {
			state->icon->paintInCenter(p, canvas->rect(), colors.accent);
		} else if (source && attention) {
			source->paint(p, QPoint(), canvas->width());
		} else if (source) {
			if (!state->fallbackIcon
				|| state->fallbackPaletteVersion != style::PaletteVersion()
				|| state->fallbackAccent != colors.accent) {
				state->fallbackIcon = std::make_unique<style::icon>(
					source->withPalette(IconPalette(
						colors.accent, source == &st::settingsIconAdd)));
				state->fallbackPaletteVersion = style::PaletteVersion();
				state->fallbackAccent = colors.accent;
			}
			state->fallbackIcon->paint(p, QPoint(), canvas->width());
		} else {
			fallback(p);
		}
		p.restore();
	}, canvas->lifetime());
}

} // namespace AxGlass
