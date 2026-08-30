// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "base/qt/qt_key_modifiers.h"

namespace base {

[[nodiscard]] inline bool IsExtendedContextMenuModifierPressed() {
	return IsShiftPressed() || IsCtrlPressed();
}

} // namespace base
