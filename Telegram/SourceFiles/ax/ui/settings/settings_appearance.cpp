// Р§Р°СЃС‚СЊ AxGram РґР»СЏ Desktop. Р¤Р°Р№Р» РёР·РјРµРЅС‘РЅ.
//
// РћСЃРЅРѕРІР°РЅРѕ РЅР° РёСЃС…РѕРґРЅРѕРј РєРѕРґРµ AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Р Р°СЃРїСЂРѕСЃС‚СЂР°РЅСЏРµС‚СЃСЏ РїРѕ GNU GPL v3 вЂ” СЃРј. LICENSE Рё NOTICE.md.
#include "ax/ui/settings/settings_appearance.h"

#include "lang_auto.h"
#include "ax/ax_settings.h"
#include "core/application.h"
#include "ax/ui/appearance/glass_accent.h"
#include "ax/ui/appearance/glass_accent_settings.h"
#include "ui/widgets/color_editor.h"
#include "ui/layers/generic_box.h"
#include "ax/ax_ui_settings.h"
#include "ax/ui/boxes/font_selector.h"
#include "ax/ui/components/avatar_corners_preview.h"
#include "ax/ui/components/icon_picker.h"
#include "ax/ui/settings/ax_builder.h"
#include "ax/ui/settings/settings_ax_utils.h"
#include "ax/ui/settings/settings_main.h"
#include "inline_bots/bot_attach_web_view.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ax_icons.h"
#include "styles/style_ax_styles.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "window/themes/window_theme.h"

namespace Settings {

using namespace Builder;
using namespace AxBuilder;

namespace {

bool HasDrawerBots(not_null<Window::SessionController*> controller) {
	// todo: maybe iterate through all accounts
	const auto bots = &controller->session().attachWebView();
	for (const auto &bot : bots->attachBots()) {
		if (!bot.inMainMenu || !bot.media) {
			continue;
		}
		return true;
	}
	return false;
}

void BuildAppIcon(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle({
		.id = u"ax/appIcon"_q,
		.title = tr::ax_AppIconHeader(),
	});

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto onChanged = Fn<void()>();
#ifdef Q_OS_LINUX
		if (!qEnvironmentVariableIsEmpty("APPIMAGE")) {
			onChanged = [controller = ctx.controller] {
				ShowRestartPrompt(controller, [] {
					qputenv("AXGRAM_APPIMAGE_ICON_RESTART", "1");
					Core::Restart();
				});
			};
		}
#endif
		return {
			.widget = object_ptr<IconPicker>(ctx.container, std::move(onChanged)),
			.margin = st::settingsButtonNoIcon.padding,
		};
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	builder.addDivider();
	builder.addSkip();
	ax.addSettingToggle({
		.id = u"ax/hideNotificationBadge"_q,
		.title = tr::ax_HideNotificationBadge(),
		.getter = &AxSettings::hideNotificationBadge,
		.setter = &AxSettings::setHideNotificationBadge,
	});
	builder.addSkip();
	builder.addDividerText(tr::ax_HideNotificationBadgeDescription());
	builder.addSkip();
#else
    builder.addDivider();
    builder.addSkip();
#endif
}

void BuildAvatarCorners(SectionBuilder &builder, AxSectionBuilder &ax) {
	auto *settings = &AxSettings::getInstance();
	const auto controller = builder.controller();

	const auto mapRadius = [](int val)
	{
		if (val == 0) {
			return tr::ax_AvatarCornersSquare(tr::now).toUpper();
		} else if (val == AxUiSettings::kMaxAvatarCorners) {
			return tr::ax_AvatarCornersCircle(tr::now).toUpper();
		}
		return QString::number(val);
	};

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		const auto container = ctx.container;
		auto title = object_ptr<Ui::FlatLabel>(
			container,
			tr::ax_AvatarCorners(),
			st::defaultSubsectionTitle);
		const auto titleRaw = title.data();

		const auto badge = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
			container,
			object_ptr<Ui::FlatLabel>(
				container,
				settings->avatarCornersValue() | rpl::map(mapRadius),
				st::settingsPremiumNewBadge),
			st::axBetaBadgePadding);
		badge->show();
		badge->setAttribute(Qt::WA_TransparentForMouseEvents);
		badge->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(badge);
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgActive);
			const auto r = st::axBetaBadgePadding.left();
			p.drawRoundedRect(badge->rect(), r, r);
		}, badge->lifetime());

		titleRaw->geometryValue() | rpl::on_next([=](QRect geometry) {
			badge->moveToLeft(
				geometry.x()
					+ titleRaw->textMaxWidth()
					+ st::settingsPremiumNewBadgePosition.x(),
				geometry.y()
					+ (geometry.height() - badge->height()) / 2);
		}, badge->lifetime());

		return {
			.widget = std::move(title),
			.margin = st::defaultSubsectionTitlePadding,
		};
	}, [] {
		return SearchEntry{
			.id = u"ax/avatarCorners"_q,
			.title = tr::ax_AvatarCorners(tr::now),
		};
	});

	auto *previewRaw = static_cast<AvatarCornersPreview*>(nullptr);
	builder.add([&](const Builder::WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<AvatarCornersPreview>(
			ctx.container,
			controller);
		previewRaw = preview.data();
		const auto vMargin = st::settingsButtonNoIcon.padding
			- st::defaultDialogRow.padding;
		return {
			.widget = std::move(preview),
			.margin = QMargins(0, vMargin.top(), 0, vMargin.bottom()),
		};
	});

	ax.addSlider({
		.id = u"ax/avatarCornersSlider"_q,
		.title = rpl::single(QString()),
		.showTitle = false,
		.steps = AxUiSettings::kMaxAvatarCorners + 1,
		.current = settings->avatarCorners(),
		.onChanged = [=](int val) {
			AxSettings::getInstance().setAvatarCorners(val);
			if (previewRaw) {
				previewRaw->update();
			}
		},
		.onFinalChanged = [=](int val) {
			AxSettings::getInstance().setAvatarCorners(val);
			ShowRestartPrompt(controller);
		},
	});

	ax.addSettingToggle({
		.id = u"ax/singleCornerRadius"_q,
		.title = tr::ax_SingleCornerRadius(),
		.getter = &AxSettings::singleCornerRadius,
		.setter = &AxSettings::setSingleCornerRadius,
	});

	builder.addSkip();
	builder.addDividerText(tr::ax_SingleCornerRadiusDescription());
	builder.addSkip();
}

void BuildAppearance(SectionBuilder &builder, AxSectionBuilder &ax) {
	auto *settings = &AxSettings::getInstance();

	builder.addSubsectionTitle(tr::ax_CategoryAppearance());

	const auto telegramInterface = builder.addButton({
		.id = u"ax/telegramInterface"_q,
		.altIds = { u"ax/glassFolders"_q },
		.title = tr::ax_GlassFolders(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(settings->glassFolders())
			| rpl::then(settings->glassFoldersChanges())
			| rpl::map([](bool glass) { return !glass; }),
		.keywords = { u"Telegram"_q, u"interface"_q, u"standard"_q },
	});
	if (telegramInterface) {
		telegramInterface->toggledChanges(
		) | rpl::filter([=](bool telegram) {
			return telegram == settings->glassFolders();
		}) | rpl::on_next([=](bool telegram) {
			const auto dark = Window::Theme::IsNightMode();
			if (telegram) {
				Window::Theme::ApplyDefaultWithPath(
					dark ? Window::Theme::NightThemePath() : QString());
				Window::Theme::KeepApplied();
				settings->setGlassFolders(false);
			} else {
				const auto path = dark
					? u":/gui/axgram-glass-dark.tdesktop-theme"_q
					: u":/gui/axgram-glass-light.tdesktop-theme"_q;
				if (Window::Theme::Apply(path)) {
					Window::Theme::KeepApplied();
					settings->setGlassFolders(true);
				}
			}
		}, telegramInterface->lifetime());
	}

	const auto themeController = builder.controller();
	for (const auto dark : { false, true }) {
		builder.addButton({
			.id = dark ? u"ax/glassDarkTheme"_q : u"ax/glassLightTheme"_q,
			.title = (dark
				? tr::lng_settings_theme_night()
				: tr::lng_settings_theme_day()
			) | rpl::map([](QString name) {
				return u"AxGram Glass В· "_q + name;
			}),
			.st = &st::settingsButtonNoIcon,
			.onClick = [=] {
				const auto path = dark
					? u":/gui/axgram-glass-dark.tdesktop-theme"_q
					: u":/gui/axgram-glass-light.tdesktop-theme"_q;
				if (Window::Theme::IsNightMode() == dark) {
					if (Window::Theme::Apply(path)) {
						AxSettings::getInstance().setGlassFolders(true);
						Window::Theme::KeepApplied();
					}
				} else {
					Window::Theme::ToggleNightModeWithConfirmation(
						&themeController->window(),
						[=] {
							Window::Theme::ToggleNightMode(path);
							AxSettings::getInstance().setGlassFolders(true);
							Window::Theme::KeepApplied();
						});
				}
			},
		});
	}

	builder.addButton({
		.id = u"ax/glassAccent"_q,
		.title = tr::lng_settings_theme_accent_title()
			| rpl::map([](QString title) { return u"AxGram Glass · "_q + title; }),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(AxGlass::SavedAccent())
			| rpl::then(AxGlass::AccentChanges())
			| rpl::map([](QColor color) {
				return color.isValid() ? color.name(QColor::HexRgb).toUpper()
					: tr::lng_settings_notifications_display_default(tr::now);
			}),
		.onClick = [=] {
			themeController->show(Box([](not_null<Ui::GenericBox*> box) {
				const auto saved = AxGlass::SavedAccent();
				const auto current = saved.isValid() ? saved
					: AxGlass::CurrentAccent(Window::Theme::IsNightMode());
				const auto editor = box->addRow(object_ptr<ColorEditor>(
					box, ColorEditor::Mode::HSL, current));
				const auto save = [=] {
					AxGlass::SaveAccent(editor->color());
					box->closeBox();
				};
				editor->submitRequests() | rpl::on_next(save, editor->lifetime());
				box->setFocusCallback([=] { editor->setInnerFocus(); });
				box->setTitle(tr::lng_settings_theme_accent_title());
				box->setWidth(editor->width());
				box->addButton(tr::lng_settings_save(), save);
				box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
				box->addLeftButton(tr::lng_shortcuts_reset(), [=] {
					AxGlass::SaveAccent(QColor());
					box->closeBox();
				});
			}));
		},
	});

	ax.addSettingToggle({
		.id = u"ax/materialSwitches"_q,
		.altIds = { u"ax/newSwitchStyle"_q },
		.title = tr::ax_MaterialSwitches(),
		.getter = &AxSettings::materialSwitches,
		.setter = &AxSettings::setMaterialSwitches,
	});
	ax.addSettingToggle({
		.id = u"ax/disableCustomBackgrounds"_q,
		.altIds = { u"ax/customThemes"_q },
		.title = tr::ax_DisableCustomBackgrounds(),
		.getter = &AxSettings::disableCustomBackgrounds,
		.setter = &AxSettings::setDisableCustomBackgrounds,
	});
	ax.addSettingToggle({
		.id = u"ax/hidePremiumStatuses"_q,
		.title = tr::ax_HidePremiumStatuses(),
		.getter = &AxSettings::hidePremiumStatuses,
		.setter = &AxSettings::setHidePremiumStatuses,
	});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"ax/monoFont"_q,
		.title = tr::ax_MonospaceFont(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(
			settings->monoFont().isEmpty()
				? tr::ax_FontDefault(tr::now)
				: settings->monoFont()),
		.onClick = [=] {
			AxUi::FontSelectorBox::Show(
				controller,
				[=](const QString &font) {
					AxSettings::getInstance().setMonoFont(font);
				});
		},
	});

	ax.addSectionDivider();
}

void BuildChatFolders(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::ax_ChatFoldersHeader());

	ax.addSettingToggle({
		.id = u"ax/hideNotificationCounters"_q,
		.altIds = { u"ax/tabCounter"_q },
		.title = tr::ax_HideNotificationCounters(),
		.getter = &AxSettings::hideNotificationCounters,
		.setter = &AxSettings::setHideNotificationCounters,
	});
	ax.addSettingToggle({
		.id = u"ax/hideAllChatsFolder"_q,
		.altIds = { u"ax/hideAllChats"_q },
		.title = tr::ax_HideAllChats(),
		.getter = &AxSettings::hideAllChatsFolder,
		.setter = &AxSettings::setHideAllChatsFolder,
	});

	ax.addSectionDivider();
}

void BuildTrayElements(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::ax_TrayElementsHeader());

	ax.addSettingToggle({
		.id = u"ax/showGhostToggleInTray"_q,
		.title = tr::ax_EnableGhostModeTray(),
		.getter = &AxSettings::showGhostToggleInTray,
		.setter = &AxSettings::setShowGhostToggleInTray,
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ax.addSettingToggle({
		.id = u"ax/showStreamerToggleInTray"_q,
		.title = tr::ax_EnableStreamerModeTray(),
		.getter = &AxSettings::showStreamerToggleInTray,
		.setter = &AxSettings::setShowStreamerToggleInTray,
	});
#endif

	ax.addSectionDivider();
}

void BuildDrawerElements(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::ax_DrawerElementsHeader());

	ax.addSettingToggle({
		.id = u"ax/showMyProfileInDrawer"_q,
		.title = tr::lng_menu_my_profile(),
		.getter = &AxSettings::showMyProfileInDrawer,
		.setter = &AxSettings::setShowMyProfileInDrawer,
		.icon = { &st::menuIconProfile },
	});

	const auto controller = builder.controller();
	if (controller && HasDrawerBots(controller)) {
		ax.addSettingToggle({
			.id = u"ax/showBotsInDrawer"_q,
			.title = tr::lng_filters_type_bots(),
			.getter = &AxSettings::showBotsInDrawer,
			.setter = &AxSettings::setShowBotsInDrawer,
			.icon = { &st::menuIconBot },
		});
	}

	ax.addSettingToggle({
		.id = u"ax/showNewGroupInDrawer"_q,
		.title = tr::lng_create_group_title(),
		.getter = &AxSettings::showNewGroupInDrawer,
		.setter = &AxSettings::setShowNewGroupInDrawer,
		.icon = { &st::menuIconGroups },
	});
	ax.addSettingToggle({
		.id = u"ax/showNewChannelInDrawer"_q,
		.title = tr::lng_create_channel_title(),
		.getter = &AxSettings::showNewChannelInDrawer,
		.setter = &AxSettings::setShowNewChannelInDrawer,
		.icon = { &st::menuIconChannel },
	});
	ax.addSettingToggle({
		.id = u"ax/showContactsInDrawer"_q,
		.title = tr::lng_menu_contacts(),
		.getter = &AxSettings::showContactsInDrawer,
		.setter = &AxSettings::setShowContactsInDrawer,
		.icon = { &st::menuIconUserShow },
	});
	ax.addSettingToggle({
		.id = u"ax/showCallsInDrawer"_q,
		.title = tr::lng_menu_calls(),
		.getter = &AxSettings::showCallsInDrawer,
		.setter = &AxSettings::setShowCallsInDrawer,
		.icon = { &st::menuIconPhone },
	});
	ax.addSettingToggle({
		.id = u"ax/showSavedMessagesInDrawer"_q,
		.title = tr::lng_saved_messages(),
		.getter = &AxSettings::showSavedMessagesInDrawer,
		.setter = &AxSettings::setShowSavedMessagesInDrawer,
		.icon = { &st::menuIconSavedMessages },
	});
	ax.addSettingToggle({
		.id = u"ax/showLReadToggleInDrawer"_q,
		.title = tr::ax_LReadMessages(),
		.getter = &AxSettings::showLReadToggleInDrawer,
		.setter = &AxSettings::setShowLReadToggleInDrawer,
		.icon = { &st::axLReadMenuIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showSReadToggleInDrawer"_q,
		.title = tr::ax_SReadMessages(),
		.getter = &AxSettings::showSReadToggleInDrawer,
		.setter = &AxSettings::setShowSReadToggleInDrawer,
		.icon = { &st::axSReadMenuIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showNightModeToggleInDrawer"_q,
		.title = tr::lng_menu_night_mode(),
		.getter = &AxSettings::showNightModeToggleInDrawer,
		.setter = &AxSettings::setShowNightModeToggleInDrawer,
		.icon = { &st::menuIconNightMode },
	});
	ax.addSettingToggle({
		.id = u"ax/showGhostToggleInDrawer"_q,
		.title = tr::ax_GhostModeToggle(),
		.getter = &AxSettings::showGhostToggleInDrawer,
		.setter = &AxSettings::setShowGhostToggleInDrawer,
		.icon = { &st::axGhostIcon },
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ax.addSettingToggle({
		.id = u"ax/showStreamerToggleInDrawer"_q,
		.title = tr::ax_StreamerModeToggle(),
		.getter = &AxSettings::showStreamerToggleInDrawer,
		.setter = &AxSettings::setShowStreamerToggleInDrawer,
		.icon = { &st::axStreamerModeMenuIcon },
	});
#endif

	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AxAppearance::Id(),
	.parentId = AxMain::Id(),
	.title = &tr::ax_CategoryAppearance,
	.icon = &st::menuIconPalette,
}, [](SectionBuilder &builder) {
	auto ax = AxSectionBuilder(builder);

	builder.addSkip();
	BuildAppIcon(builder, ax);
	BuildAvatarCorners(builder, ax);
	BuildAppearance(builder, ax);
	BuildChatFolders(builder, ax);
	BuildTrayElements(builder, ax);
	BuildDrawerElements(builder, ax);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AxAppearance::title() {
	return tr::ax_CategoryAppearance();
}

AxAppearance::AxAppearance(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AxAppearance::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AxAppearanceId() {
	return AxAppearance::Id();
}

} // namespace Settings
