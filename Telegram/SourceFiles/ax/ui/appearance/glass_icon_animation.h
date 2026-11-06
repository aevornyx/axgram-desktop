// AxGram decorative icon replay. Modified 2026-11-03. GPLv3.
#pragma once

#include "lottie/lottie_icon.h"

namespace AxGlass {

inline void ReplayIcon(
		not_null<Lottie::Icon*> icon,
		Fn<void()> repaint,
		crl::time duration) {
	if (icon->valid() && icon->framesCount() > 1) {
		icon->animate(std::move(repaint), 0, icon->framesCount() - 1, duration);
	}
}

} // namespace AxGlass
