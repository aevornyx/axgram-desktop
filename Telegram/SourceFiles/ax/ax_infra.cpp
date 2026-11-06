// Р§Р°СЃС‚СЊ AxGram РґР»СЏ Desktop. Р¤Р°Р№Р» РёР·РјРµРЅС‘РЅ.
//
// РћСЃРЅРѕРІР°РЅРѕ РЅР° РёСЃС…РѕРґРЅРѕРј РєРѕРґРµ AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Р Р°СЃРїСЂРѕСЃС‚СЂР°РЅСЏРµС‚СЃСЏ РїРѕ GNU GPL v3 вЂ” СЃРј. LICENSE Рё NOTICE.md.
#include "ax/ax_infra.h"

#include "ax/ax_lang.h"
#include "ax/ax_promo.h"
#include "ax/ax_settings.h"
#include "ax/ax_ui_settings.h"
#include "ax/ui/appearance/glass_accent_settings.h"
#include "ax/ax_worker.h"
#include "ax/data/ax_database.h"
#include "core/application.h"
#include "ui/boxes/confirm_box.h"
#include "window/window_controller.h"
#include "lang_auto.h"

#include <QTimer>
#include "ax/ui/ax_logo.h"
#include "features/translator/ax_translator.h"
#include "lang/lang_instance.h"
#include "ui/chat/chat_style_radius.h"
#include "ui/power_saving.h"
#include "utils/rc_manager.h"

#ifdef Q_OS_WIN
#include "ax/utils/windows_utils.h"
#endif

namespace AxInfra {
namespace {

constexpr auto kArchiveWarningDelay = crl::time(3000);

void ShowArchiveWarning() {
	if (AxDatabase::available()) return;
	const auto window = Core::App().activeWindow();
	if (!window || window->locked()) {
		QTimer::singleShot(kArchiveWarningDelay, [] { ShowArchiveWarning(); });
		return;
	}
	window->show(Ui::MakeInformBox(tr::ax_ArchiveProtectionFailed()));
}

}

void initLang() {
	QString id = Lang::GetInstance().id();
	QString baseId = Lang::GetInstance().baseId();
	if (id.isEmpty()) {
		LOG(("Language is not loaded"));
		return;
	}
	AxLanguage::init();
	AxLanguage::currentInstance()->fetchLanguage(id, baseId);
}

void initUiSettings() {
	AxGlass::InitializeAccent();
	const auto &settings = AxSettings::getInstance();

	AxUiSettings::setAnimatedIconsPolicy([] { return !On(PowerSaving::kAnimations); });
	AxUiSettings::setAnimatedIconsEnabled(settings.glassFolders());
	AxUiSettings::setMonoFont(settings.monoFont());
	AxUiSettings::setWideMultiplier(settings.wideMultiplier());
	AxUiSettings::setMaterialSwitches(
		settings.glassFolders() && settings.materialSwitches());
	AxUiSettings::setAvatarCorners(settings.avatarCorners());
	Ui::SetAppliedBubbleRadius(settings.messageBubbleRadius());
}

void initDatabase() {
	AxDatabase::initialize();
	if (!AxDatabase::available()) {
		QTimer::singleShot(kArchiveWarningDelay, [] { ShowArchiveWarning(); });
	}
}

void initWorker() {
	AxWorker::initialize();
}

void initRCManager() {
	// AxGram: no phone-home to AxGram servers; badge lists stay at built-in defaults.
}

void initTranslator() {
	Ax::Translator::TranslateManager::init();
}

void initIcon() {
#ifdef Q_OS_WIN
	AxAssets::loadAppIco();
	reloadAppIconFromTaskBar();
#endif
}

void init() {
	initLang();
	initDatabase();
	initUiSettings();
	initIcon();
	initWorker();
	initRCManager();
	initTranslator();

	// Р РµРґРєРѕРµ РЅР°РїРѕРјРёРЅР°РЅРёРµ Рѕ РєР°РЅР°Р»Рµ Рё Р±РѕС‚Рµ РїРѕРґРґРµСЂР¶РєРё. РџРѕРєР°Р·С‹РІР°РµС‚СЃСЏ С‚РѕСЃС‚РѕРј
	// СЃРёР»СЊРЅРѕ РїРѕР·Р¶Рµ Р·Р°РїСѓСЃРєР° вЂ” СЃСЂР°Р·Сѓ РїРѕСЃР»Рµ СЃС‚Р°СЂС‚Р° РѕРєРЅР° СЃ СЃРµСЃСЃРёРµР№ РµС‰С‘ РЅРµС‚.
	Ax::Promo::Schedule();
}

}
