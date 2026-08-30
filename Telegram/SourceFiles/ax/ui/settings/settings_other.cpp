// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ui/settings/settings_other.h"

#include "lang_auto.h"
#include "ax/ax_settings.h"
#include "ax/utils/ax_update_check.h"
#include "ax/ui/settings/ax_builder.h"
#include "ax/ui/settings/settings_ax_utils.h"
#include "ax/ui/settings/settings_main.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;
using namespace AxBuilder;

namespace {

void BuildCrashReporting(SectionBuilder &builder, AxSectionBuilder &ax) {
#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	builder.addSkip();
	builder.addSubsectionTitle(tr::ax_CategoryOther());

	ax.addSettingToggle({
		.id = u"ax/crashReporting"_q,
		.altIds = { u"ax/crashlytics"_q },
		.title = tr::ax_CrashReporting(),
		.getter = &AxSettings::crashReporting,
		.setter = &AxSettings::setCrashReporting,
		.icon = { &st::menuIconReport },
	});
	builder.addSkip();
	builder.addDividerText(tr::ax_CrashReportingDescription());
#endif
}

void BuildOtherThings(SectionBuilder &builder) {
	const auto controller = builder.controller();

	builder.addSkip();
	if (Ax::Updates::SupportAvailable()) {
		builder.addButton({
			.id = u"ax/support"_q,
			.title = tr::ax_SupportAuthor(),
			.icon = { &st::menuIconStarRefShare },
			.onClick = [=] { Ax::Updates::OpenSupport(); },
		});
		builder.addSkip();
		builder.addDividerText(tr::ax_SupportAuthorAbout());
		builder.addSkip();
	}
	builder.addButton({
		.id = u"ax/checkUpdates"_q,
		.title = tr::ax_UpdateCheck(),
		.icon = { &st::menuIconDownload },
		.onClick = [=] {
			Ax::Updates::CheckManually(controller);
		},
	});
	builder.addButton({
		.id = u"ax/registerUrlScheme"_q,
		.title = tr::ax_RegisterURLScheme(),
		.icon = { &st::menuIconLink },
		.onClick = [=] {
			Core::Application::RegisterUrlScheme();
			controller->showToast(tr::lng_box_done(tr::now));
		},
	});
	builder.addButton({
		.id = u"ax/resetSettings"_q,
		.title = tr::ax_ResetSettings(),
		.icon = { &st::menuIconRestore },
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::ax_ResetSettingsConfirmation(tr::rich),
				.confirmed = [=](Fn<void()> &&close) {
					AxSettings::reset();
					controller->showToast(tr::lng_box_done(tr::now));
					close();
				},
				.confirmText = tr::lng_box_yes(),
			}));
		},
	});
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AxOther::Id(),
	.parentId = AxMain::Id(),
	.title = &tr::ax_CategoryOther,
	.icon = &st::menuIconFave,
}, [](SectionBuilder &builder) {
	auto ax = AxSectionBuilder(builder);

	builder.addSkip();
	BuildCrashReporting(builder, ax);
	BuildOtherThings(builder);
});

} // namespace

rpl::producer<QString> AxOther::title() {
	return tr::ax_CategoryOther();
}

AxOther::AxOther(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AxOther::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AxOtherId() {
	return AxOther::Id();
}

} // namespace Settings
