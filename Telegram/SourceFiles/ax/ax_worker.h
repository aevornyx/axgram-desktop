// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "window/window_session_controller.h"

namespace AxWorker {

void markAsOnline(not_null<Main::Session*> session);
void initialize();

}
