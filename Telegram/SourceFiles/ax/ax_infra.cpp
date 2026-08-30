// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ax_infra.h"

#include "ax/ax_lang.h"
#include "ax/ax_promo.h"
#include "ax/ax_settings.h"
#include "ax/ax_ui_settings.h"
#include "ax/ax_worker.h"
#include "ax/data/ax_database.h"
#include "ax/ui/ax_logo.h"
#include "features/translator/ax_translator.h"
#include "lang/lang_instance.h"
#include "ui/chat/chat_style_radius.h"
#include "utils/rc_manager.h"

#ifdef Q_OS_WIN
#include "ax/utils/windows_utils.h"
#endif

namespace AxInfra {

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
	const auto &settings = AxSettings::getInstance();

	AxUiSettings::setMonoFont(settings.monoFont());
	AxUiSettings::setWideMultiplier(settings.wideMultiplier());
	AxUiSettings::setMaterialSwitches(settings.materialSwitches());
	AxUiSettings::setAvatarCorners(settings.avatarCorners());
	Ui::SetAppliedBubbleRadius(settings.messageBubbleRadius());
}

void initDatabase() {
	AxDatabase::initialize();
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

	// Редкое напоминание о канале и боте поддержки. Показывается тостом
	// сильно позже запуска — сразу после старта окна с сессией ещё нет.
	Ax::Promo::Schedule();
}

}
