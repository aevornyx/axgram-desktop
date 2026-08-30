// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/utils/taptic_engine/taptic_engine.h"

#if defined Q_OS_MAC
#include "ax/utils/taptic_engine/platform/taptic_engine_mac.h"
#else
#include "ax/utils/taptic_engine/platform/taptic_engine_dummy.h"
#endif

namespace TapticEngine {

void generateGeneric() {
	Impl::generateGeneric();
}

void generateAlignment() {
	Impl::generateAlignment();
}

void generateLevelChange() {
	Impl::generateLevelChange();
}

}
