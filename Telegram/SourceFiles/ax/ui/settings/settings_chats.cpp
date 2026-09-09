// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ui/settings/settings_chats.h"

#include "lang_auto.h"
#include "ax/ax_settings.h"
#include "ax/ui/boxes/edit_mark_box.h"
#include "ax/ui/components/message_preview.h"
#include "ax/ui/settings/ax_builder.h"
#include "ax/ui/settings/settings_ax_utils.h"
#include "ax/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ax_icons.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <memory>

namespace Settings {

using namespace Builder;
using namespace AxBuilder;

namespace {

struct PreviewState {
	MessagePreview *widget = nullptr;
};

void BuildStickersAndEmoji(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::lng_settings_stickers_emoji());

	ax.addSettingToggle({
		.id = u"ax/showOnlyAddedEmojisAndStickers"_q,
		.title = tr::ax_ShowOnlyAddedEmojisAndStickers(),
		.getter = &AxSettings::showOnlyAddedEmojisAndStickers,
		.setter = &AxSettings::setShowOnlyAddedEmojisAndStickers,
	});

	ax.addSettingToggle({
		.id = u"ax/unlimitedRecentStickers"_q,
		.altIds = { u"ax/recentStickersCount"_q },
		.title = tr::ax_SettingsUnlimitedRecentStickers(),
		.getter = &AxSettings::unlimitedRecentStickers,
		.setter = &AxSettings::setUnlimitedRecentStickers,
	});

	ax.addCollapsibleToggle({
		.id = u"ax/hideReactions"_q,
		.title = tr::ax_HideReactions(),
		.checkboxes = {
			NestedEntry{
				tr::ax_HideReactionsInChannels(tr::now),
				[] { return !AxSettings::getInstance().showChannelReactions(); },
				[](bool v) { AxSettings::getInstance().setShowChannelReactions(!v); }
			},
			NestedEntry{
				tr::ax_HideReactionsInGroups(tr::now),
				[] { return !AxSettings::getInstance().showGroupReactions(); },
				[](bool v) { AxSettings::getInstance().setShowGroupReactions(!v); }
			},
			NestedEntry{
				tr::ax_HideReactionsInPrivateChats(tr::now),
				[] { return !AxSettings::getInstance().showPrivateChatReactions(); },
				[](bool v) { AxSettings::getInstance().setShowPrivateChatReactions(!v); }
			}
		},
		.toggledWhenAll = false,
	});

	ax.addSectionDivider();
}

void BuildGroupsAndChannels(SectionBuilder &builder, AxSectionBuilder &ax) {
	auto *settings = &AxSettings::getInstance();

	builder.addSubsectionTitle(tr::lng_premium_double_limits_subtitle_channels());

	ax.addChooseButton({
		.id = u"ax/channelBottomButton"_q,
		.altIds = { u"ax/bottomButton"_q },
		.title = tr::ax_ChannelBottomButton(),
		.boxTitle = tr::ax_ChannelBottomButton(),
		.initialSelection = static_cast<int>(settings->channelBottomButton()),
		.options = {
			tr::ax_ChannelBottomButtonHide(tr::now),
			tr::ax_ChannelBottomButtonMute(tr::now),
			tr::ax_ChannelBottomButtonDiscuss(tr::now),
		},
		.setter = [](int index) {
			AxSettings::getInstance().setChannelBottomButton(
				static_cast<ChannelBottomButton>(index));
		},
	});

	ax.addSettingToggle({
		.id = u"ax/quickAdminShortcuts"_q,
		.title = tr::ax_QuickAdminShortcuts(),
		.getter = &AxSettings::quickAdminShortcuts,
		.setter = &AxSettings::setQuickAdminShortcuts,
	});
	ax.addSettingToggle({
		.id = u"ax/disableGreetingSticker"_q,
		.title = tr::ax_DisableGreetingSticker(),
		.getter = &AxSettings::disableGreetingSticker,
		.setter = &AxSettings::setDisableGreetingSticker,
	});
	ax.addSettingToggle({
		.id = u"ax/showMessageShot"_q,
		.title = tr::ax_SettingsShowMessageShot(),
		.getter = &AxSettings::showMessageShot,
		.setter = &AxSettings::setShowMessageShot,
	});

	builder.addSkip();
	builder.addDividerText(tr::ax_SettingsShowMessageShotDescription());
	builder.addSkip();
}

void BuildMarks(
		SectionBuilder &builder,
		AxSectionBuilder &ax,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &AxSettings::getInstance();
	const auto controller = builder.controller();

	builder.addSubsectionTitle(tr::lng_settings_messages());

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<MessagePreview>(ctx.container, controller);
		previewState->widget = preview.data();
		return {
			.widget = std::move(preview),
			.margin = style::margins(
				0,
				st::defaultVerticalListSkip,
				0,
				st::settingsPrivacySkipTop),
		};
	});

	ax.addSettingToggle({
		.id = u"ax/replaceBottomInfoWithIcons"_q,
		.altIds = { u"ax/replaceEditedWithIcon"_q },
		.title = tr::ax_ReplaceMarksWithIcons(),
		.getter = &AxSettings::replaceBottomInfoWithIcons,
		.setter = &AxSettings::setReplaceBottomInfoWithIcons,
	});

	builder.scope([&] {
		builder.addButton({
			.id = u"ax/deletedMark"_q,
			.title = tr::ax_DeletedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = AxSettings::getInstance().deletedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::ax_DeletedMarkText(),
					settings->deletedMark(),
					QString("🧹"),
					[=](const QString &value) {
						AxSettings::getInstance().setDeletedMark(value);
					});
				Ui::show(std::move(box));
			},
		});

		builder.addButton({
			.id = u"ax/editedMark"_q,
			.title = tr::ax_EditedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = AxSettings::getInstance().editedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::ax_EditedMarkText(),
					settings->editedMark(),
					tr::lng_edited(tr::now),
					[=](const QString &value) {
						AxSettings::getInstance().setEditedMark(value);
					});
				Ui::show(std::move(box));
			},
		});
	}, AxSettings::getInstance().replaceBottomInfoWithIconsValue()
		| rpl::map([](bool v) { return !v; }));

	ax.addSettingToggle({
		.id = u"ax/removeMessageTail"_q,
		.title = tr::ax_RemoveMessageTail(),
		.getter = &AxSettings::removeMessageTail,
		.setter = &AxSettings::setRemoveMessageTail,
	});

	ax.addSettingToggle({
		.id = u"ax/hideFastShare"_q,
		.altIds = { u"ax/hideShareButton"_q },
		.title = tr::ax_HideShareButton(),
		.getter = &AxSettings::hideFastShare,
		.setter = &AxSettings::setHideFastShare,
	});
	ax.addSettingToggle({
		.id = u"ax/simpleQuotesAndReplies"_q,
		.altIds = { u"ax/disableColorfulReplies"_q, u"ax/replyElements"_q },
		.title = tr::ax_SimpleQuotesAndReplies(),
		.getter = &AxSettings::simpleQuotesAndReplies,
		.setter = &AxSettings::setSimpleQuotesAndReplies,
	});

	const auto semiTransparent = ax.addSettingToggle({
		.id = u"ax/semiTransparentDeletedMessages"_q,
		.altIds = { u"ax/translucentDeletedMessages"_q },
		.title = tr::ax_SemiTransparentDeletedMessages(),
		.getter = &AxSettings::semiTransparentDeletedMessages,
		.setter = &AxSettings::setSemiTransparentDeletedMessages,
	});
	if (semiTransparent) {
		ax.addBetaBadge(semiTransparent);
	}

	ax.addSectionDivider();
}

void BuildWideMessagesMultiplier(
		SectionBuilder &builder,
		AxSectionBuilder &ax,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &AxSettings::getInstance();

	constexpr auto kMinSize = 1.00;
	constexpr auto kStep = 0.05;

	const auto valueToIndex = [=](double value) {
		return static_cast<int>(std::round((value - kMinSize) / kStep));
	};

	const auto controller = builder.controller();
	ax.addSlider({
		.id = u"ax/messageBubbleRadius"_q,
		.title = tr::ax_MessageBubbleRadius(),
		.steps = 17,
		.current = settings->messageBubbleRadius(),
		.indexToValue = [](int index) { return index; },
		.onChanged = [=](int index) {
			if (previewState->widget) {
				previewState->widget->setBubbleRadius(index);
			}
		},
		.onFinalChanged = [=](int index) {
			if (previewState->widget) {
				previewState->widget->setBubbleRadius(index);
			}
			AxSettings::getInstance().setMessageBubbleRadius(index);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [](int index) {
			return QString::number(index);
		},
	});

	ax.addSectionDivider();

	ax.addSlider({
		.id = u"ax/wideMultiplier"_q,
		.title = tr::ax_SettingsWideMultiplier(),
		.steps = 61, // (4.00 - 1.00) / 0.05 + 1
		.current = valueToIndex(settings->wideMultiplier()),
		.indexToValue = [](int index) { return index; },
		.onChanged = nullptr,
		.onFinalChanged = [=](int index) {
			AxSettings::getInstance().setWideMultiplier(
				kMinSize + index * kStep);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [=](int index) {
			return QString::number(kMinSize + index * kStep, 'f', 2);
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::ax_SettingsWideMultiplierDescription());
	builder.addSkip();
}

void BuildContextMenuElements(SectionBuilder &builder, AxSectionBuilder &ax) {
	auto *settings = &AxSettings::getInstance();

	builder.addSubsectionTitle(tr::ax_ContextMenuElementsHeader());

	const auto options = std::vector{
		tr::ax_SettingsContextMenuItemHidden(tr::now),
		tr::ax_SettingsContextMenuItemShown(tr::now),
		tr::ax_SettingsContextMenuItemExtended(tr::now),
	};

	ax.addChooseButton({
		.id = u"ax/showReactionsPanelInContextMenu"_q,
		.title = tr::ax_SettingsContextMenuReactionsPanel(),
		.boxTitle = tr::ax_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showReactionsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { AxSettings::getInstance().setShowReactionsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconReactions },
	});
	ax.addChooseButton({
		.id = u"ax/showViewsPanelInContextMenu"_q,
		.title = tr::ax_SettingsContextMenuViewsPanel(),
		.boxTitle = tr::ax_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showViewsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { AxSettings::getInstance().setShowViewsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconShowInChat },
	});
	ax.addChooseButton({
		.id = u"ax/showHideMessageInContextMenu"_q,
		.title = tr::ax_ContextHideMessage(),
		.boxTitle = tr::ax_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showHideMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { AxSettings::getInstance().setShowHideMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconClear },
	});
	ax.addChooseButton({
		.id = u"ax/showUserMessagesInContextMenu"_q,
		.title = tr::ax_UserMessagesMenuText(),
		.boxTitle = tr::ax_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showUserMessagesInContextMenu()),
		.options = options,
		.setter = [](int i) { AxSettings::getInstance().setShowUserMessagesInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconTTL },
	});
	ax.addChooseButton({
		.id = u"ax/showMessageDetailsInContextMenu"_q,
		.title = tr::ax_MessageDetailsPC(),
		.boxTitle = tr::ax_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showMessageDetailsInContextMenu()),
		.options = options,
		.setter = [](int i) { AxSettings::getInstance().setShowMessageDetailsInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconInfo },
	});
	ax.addChooseButton({
		.id = u"ax/showRepeatMessageInContextMenu"_q,
		.title = tr::ax_RepeatMessage(),
		.boxTitle = tr::ax_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showRepeatMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { AxSettings::getInstance().setShowRepeatMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::axRepeatMenuIcon },
	});
	if (settings->filtersEnabled()) {
		ax.addChooseButton({
			.id = u"ax/showAddFilterInContextMenu"_q,
			.title = tr::ax_RegexFilterQuickAdd(),
			.boxTitle = tr::ax_SettingsContextMenuTitle(),
			.initialSelection = static_cast<int>(settings->showAddFilterInContextMenu()),
			.options = options,
			.setter = [](int i) { AxSettings::getInstance().setShowAddFilterInContextMenu(static_cast<ContextMenuVisibility>(i)); },
			.icon = { &st::menuIconAddToFolder },
		});
	}

	builder.addSkip();
	builder.addDividerText(tr::ax_SettingsContextMenuDescription());
	builder.addSkip();
}

void BuildMessageFieldElements(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::ax_MessageFieldElementsHeader());

	ax.addSettingToggle({
		.id = u"ax/showAttachButtonInMessageField"_q,
		.title = tr::ax_MessageFieldElementAttach(),
		.getter = &AxSettings::showAttachButtonInMessageField,
		.setter = &AxSettings::setShowAttachButtonInMessageField,
		.icon = { &st::messageFieldAttachIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showCommandsButtonInMessageField"_q,
		.title = tr::ax_MessageFieldElementCommands(),
		.getter = &AxSettings::showCommandsButtonInMessageField,
		.setter = &AxSettings::setShowCommandsButtonInMessageField,
		.icon = { &st::messageFieldCommandsIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showAutoDeleteButtonInMessageField"_q,
		.title = tr::ax_MessageFieldElementTTL(),
		.getter = &AxSettings::showAutoDeleteButtonInMessageField,
		.setter = &AxSettings::setShowAutoDeleteButtonInMessageField,
		.icon = { &st::messageFieldTTLIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showEmojiButtonInMessageField"_q,
		.title = tr::ax_MessageFieldElementEmoji(),
		.getter = &AxSettings::showEmojiButtonInMessageField,
		.setter = &AxSettings::setShowEmojiButtonInMessageField,
		.icon = { &st::messageFieldEmojiIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showMicrophoneButtonInMessageField"_q,
		.title = tr::ax_MessageFieldElementVoice(),
		.getter = &AxSettings::showMicrophoneButtonInMessageField,
		.setter = &AxSettings::setShowMicrophoneButtonInMessageField,
		.icon = { &st::messageFieldVoiceIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showGiftButtonInMessageField"_q,
		.title = tr::lng_profile_action_short_gift(),
		.getter = &AxSettings::showGiftButtonInMessageField,
		.setter = &AxSettings::setShowGiftButtonInMessageField,
		.icon = { &st::settingsButtonIconGift },
	});
	ax.addSettingToggle({
		.id = u"ax/showAiEditorButtonInMessageField"_q,
		.title = tr::lng_ai_compose_title(),
		.getter = &AxSettings::showAiEditorButtonInMessageField,
		.setter = &AxSettings::setShowAiEditorButtonInMessageField,
		.icon = { &st::messageFieldCocoonAiIcon },
	});

	ax.addSectionDivider();
}

void BuildMessageFieldPopups(SectionBuilder &builder, AxSectionBuilder &ax) {
	builder.addSubsectionTitle(tr::ax_MessageFieldPopupsHeader());

	ax.addSettingToggle({
		.id = u"ax/showAttachPopup"_q,
		.title = tr::ax_MessageFieldElementAttach(),
		.getter = &AxSettings::showAttachPopup,
		.setter = &AxSettings::setShowAttachPopup,
		.icon = { &st::messageFieldAttachIcon },
	});
	ax.addSettingToggle({
		.id = u"ax/showEmojiPopup"_q,
		.title = tr::ax_MessageFieldElementEmoji(),
		.getter = &AxSettings::showEmojiPopup,
		.setter = &AxSettings::setShowEmojiPopup,
		.icon = { &st::messageFieldEmojiIcon },
	});
}

const auto kMeta = BuildHelper({
	.id = AxChats::Id(),
	.parentId = AxMain::Id(),
	.title = &tr::ax_CategoryChats,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	auto ax = AxSectionBuilder(builder);
	const auto previewState = std::make_shared<PreviewState>();

	builder.addSkip();
	BuildStickersAndEmoji(builder, ax);
	BuildGroupsAndChannels(builder, ax);
	BuildMarks(builder, ax, previewState);
	BuildWideMessagesMultiplier(builder, ax, previewState);
	BuildContextMenuElements(builder, ax);
	BuildMessageFieldElements(builder, ax);
	BuildMessageFieldPopups(builder, ax);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AxChats::title() {
	return tr::ax_CategoryChats();
}

AxChats::AxChats(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AxChats::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AxChatsId() {
	return AxChats::Id();
}

} // namespace Settings
