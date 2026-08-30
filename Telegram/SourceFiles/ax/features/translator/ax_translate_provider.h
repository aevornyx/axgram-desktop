// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ax/ax_settings.h"
#include "translate_provider.h"

#include <memory>

namespace Main {
class Session;
} // namespace Main

namespace Ui {

[[nodiscard]] std::unique_ptr<TranslateProvider> CreateAxTranslateProvider(
	not_null<Main::Session*> session,
	TranslationProvider provider);

} // namespace Ui
