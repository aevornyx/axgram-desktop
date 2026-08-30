// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ui/settings/settings_general.h"

#include "lang_auto.h"
#include "ax/ax_settings.h"
#include "ax/ui/settings/ax_builder.h"
#include "ax/ui/settings/settings_ax_utils.h"
#include "ax/ui/settings/settings_main.h"
#include "base/platform/base_platform_info.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "platform/platform_translate_provider.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;
using namespace AxBuilder;

namespace {

void BuildTranslator(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::lng_translate_settings_subtitle());

	auto *settings = &AxSettings::getInstance();

	const auto options = std::vector{
		std::pair(TranslationProvider::Telegram, QString("Telegram")),
		std::pair(TranslationProvider::Google, QString("Google")),
		std::pair(TranslationProvider::Yandex, QString("Yandex")),
	};
	const auto nativeAvailable = Platform::IsTranslateProviderAvailable();
	auto availableOptions = options;
	if (nativeAvailable) {
		availableOptions.push_back(std::pair(
			TranslationProvider::Native,
			[] {
				if constexpr (Platform::IsMac()) {
					return QString("macOS");
				} else if constexpr (Platform::IsWindows()) {
					return QString("Windows");
				} else {
					return QString("Linux");
				}
			}()));
	}
	auto optionLabels = std::vector<QString>();
	optionLabels.reserve(availableOptions.size());
	for (const auto &option : availableOptions) {
		optionLabels.push_back(option.second);
	}

	const auto getIndex = [=](TranslationProvider val) {
		const auto i = ranges::find(
			availableOptions,
			val,
			&std::pair<TranslationProvider, QString>::first);
		return (i != end(availableOptions))
			? int(i - begin(availableOptions))
			: 0;
	};

	auto currentVal = AxSettings::getInstance().translationProviderValue()
		| rpl::map(getIndex)
		| rpl::map([=](int val) { return availableOptions[val].second; });

	const auto button = builder.addButton({
		.id = u"ax/translationProvider"_q,
		.title = tr::ax_TranslationProvider(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentVal),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				controller->show(Box(
						[=](not_null<Ui::GenericBox*> box) {
							const auto save = [=](int index) {
								const auto option = availableOptions[index].first;
								AxSettings::getInstance().setTranslationProvider(option);

								if constexpr (Platform::IsMac()) {
									if (option == TranslationProvider::Native) {
										controller->showToast(Ui::Toast::Config{
											.text = tr::lng_translate_settings_use_platform_mac_about(tr::now, tr::rich),
											.duration = 6 * crl::time(1000)
										});
									}
								}
							};
							SingleChoiceBox(box, {
								.title = tr::ax_TranslationProvider(),
								.options = optionLabels,
								.initialSelection = getIndex(settings->translationProvider()),
								.callback = save,
							});
						}));
			}
		},
	});
	if (button) {
		ax.addBetaBadge(button);
	}
}

void BuildShowPeerId(SectionBuilder &builder) {
	auto *settings = &AxSettings::getInstance();

	const auto options = std::vector{
		QString(tr::ax_SettingsShowID_Hide(tr::now)),
		QString("Telegram API"),
		QString("Bot API")
	};

	auto currentVal = AxSettings::getInstance().showPeerIdValue()
		| rpl::map([=](PeerIdDisplay val) {
			return options[static_cast<int>(val)];
		});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"ax/showPeerId"_q,
		.altIds = { u"ax/showIdAndDc"_q },
		.title = tr::ax_SettingsShowID(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentVal),
		.onClick = [=] {
			controller->show(Box(
				[=](not_null<Ui::GenericBox*> box) {
					const auto save = [=](int index) {
						AxSettings::getInstance().setShowPeerId(
							static_cast<PeerIdDisplay>(index));
					};
					SingleChoiceBox(box, {
						.title = tr::ax_SettingsShowID(),
						.options = options,
						.initialSelection = static_cast<int>(settings->showPeerId()),
						.callback = save,
					});
				}));
		},
	});
}

void BuildQoLToggles(SectionBuilder &builder, AxSectionBuilder &ax) {
	auto *settings = &AxSettings::getInstance();

	BuildTranslator(builder, ax);
	ax.addSectionDivider();

	builder.addSubsectionTitle(tr::ax_CategoryGeneral());

	const auto controller = builder.controller();
	ax.addToggle({
		.id = u"ax/disableStories"_q,
		.altIds = { u"ax/hideStories"_q },
		.title = tr::ax_DisableStories(),
		.getter = [=] { return settings->disableStories(); },
		.setter = [=](bool enabled) {
			AxSettings::getInstance().setDisableStories(enabled);
			ShowRestartPrompt(controller);
		},
	});

	ax.addSettingToggle({
		.id = u"ax/disableOpenLinkWarning"_q,
		.title = tr::ax_DisableOpenLinkWarning(),
		.getter = &AxSettings::disableOpenLinkWarning,
		.setter = &AxSettings::setDisableOpenLinkWarning,
	});

	ax.addCollapsibleToggle({
		.id = u"ax/similarChannels"_q,
		.title = tr::ax_DisableSimilarChannels(),
		.checkboxes = {
			NestedEntry{
				tr::ax_CollapseSimilarChannels(tr::now),
				[] { return AxSettings::getInstance().collapseSimilarChannels(); },
				[](bool v) { AxSettings::getInstance().setCollapseSimilarChannels(v); }
			},
			NestedEntry{
				tr::ax_HideSimilarChannelsTab(tr::now),
				[] { return AxSettings::getInstance().hideSimilarChannels(); },
				[](bool v) { AxSettings::getInstance().setHideSimilarChannels(v); }
			}
		},
		.toggledWhenAll = true,
	});

	ax.addSettingToggle({
		.id = u"ax/disableNotificationsDelay"_q,
		.title = tr::ax_DisableNotificationsDelay(),
		.getter = &AxSettings::disableNotificationsDelay,
		.setter = &AxSettings::setDisableNotificationsDelay,
	});

	ax.addSectionDivider();

	const auto zalgoButton = builder.addButton({
		.id = u"ax/filterZalgo"_q,
		.title = tr::ax_FilterZalgo(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(settings->filterZalgo()),
	});
	if (zalgoButton) {
		zalgoButton->toggledValue(
		) | rpl::filter(
			[=](bool enabled) {
				return (enabled != settings->filterZalgo());
			}
		) | on_next(
			[=](bool enabled) {
				AxSettings::getInstance().setFilterZalgo(enabled);
				ShowRestartPrompt(controller);
			},
			zalgoButton->lifetime());
		ax.addBetaBadge(zalgoButton);
	}

	ax.addSettingToggle({
		.id = u"ax/improveLinkPreviews"_q,
		.title = tr::ax_ImproveLinkPreviews(),
		.getter = &AxSettings::improveLinkPreviews,
		.setter = &AxSettings::setImproveLinkPreviews,
	});
	ax.addSettingToggle({
		.id = u"ax/showMessageSeconds"_q,
		.altIds = { u"ax/formatTimeWithSeconds"_q },
		.title = tr::ax_SettingsShowMessageSeconds(),
		.getter = &AxSettings::showMessageSeconds,
		.setter = &AxSettings::setShowMessageSeconds,
	});

	BuildShowPeerId(builder);

	ax.addSectionDivider();

	builder.addSubsectionTitle(rpl::single(QString("Webview")));

	ax.addSettingToggle({
		.id = u"ax/spoofWebviewAsAndroid"_q,
		.title = tr::ax_SettingsSpoofWebviewAsAndroid(),
		.getter = &AxSettings::spoofWebviewAsAndroid,
		.setter = &AxSettings::setSpoofWebviewAsAndroid,
	});

	ax.addCollapsibleToggle({
		.id = u"ax/biggerWindow"_q,
		.title = tr::ax_SettingsBiggerWindow(),
		.checkboxes = {
			NestedEntry{
				tr::ax_SettingsIncreaseWebviewHeight(tr::now),
				[] { return AxSettings::getInstance().increaseWebviewHeight(); },
				[](bool v) { AxSettings::getInstance().setIncreaseWebviewHeight(v); }
			},
			NestedEntry{
				tr::ax_SettingsIncreaseWebviewWidth(tr::now),
				[] { return AxSettings::getInstance().increaseWebviewWidth(); },
				[](bool v) { AxSettings::getInstance().setIncreaseWebviewWidth(v); }
			}
		},
		.toggledWhenAll = false,
	});

	ax.addSectionDivider();

	builder.addSubsectionTitle(tr::ax_ConfirmationsTitle());

	ax.addSettingToggle({
		.id = u"ax/stickerConfirmation"_q,
		.title = tr::ax_StickerConfirmation(),
		.getter = &AxSettings::stickerConfirmation,
		.setter = &AxSettings::setStickerConfirmation,
	});
	ax.addSettingToggle({
		.id = u"ax/gifConfirmation"_q,
		.title = tr::ax_GIFConfirmation(),
		.getter = &AxSettings::gifConfirmation,
		.setter = &AxSettings::setGifConfirmation,
	});
	ax.addSettingToggle({
		.id = u"ax/voiceConfirmation"_q,
		.title = tr::ax_VoiceConfirmation(),
		.getter = &AxSettings::voiceConfirmation,
		.setter = &AxSettings::setVoiceConfirmation,
	});
}

const auto kMeta = BuildHelper({
	.id = AxGeneral::Id(),
	.parentId = AxMain::Id(),
	.title = &tr::ax_CategoryGeneral,
	.icon = &st::menuIconShowAll,
}, [](SectionBuilder &builder) {
	auto ax = AxSectionBuilder(builder);

	builder.addSkip();
	BuildQoLToggles(builder, ax);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AxGeneral::title() {
	return tr::ax_CategoryGeneral();
}

AxGeneral::AxGeneral(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AxGeneral::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AxGeneralId() {
	return AxGeneral::Id();
}

} // namespace Settings
