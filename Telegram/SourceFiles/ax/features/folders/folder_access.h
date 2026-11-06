#pragma once

namespace AxFolders {

template <typename Range>
[[nodiscard]] bool IsPremiumLocked(
		const Range &folders,
		int id,
		int serverLimit) {
	if (id <= 0 || id >= 1000) {
		return false;
	}
	auto serverCount = 0;
	for (const auto &folder : folders) {
		const auto current = folder.id();
		if (current > 0 && current < 1000) {
			++serverCount;
		}
		if (current == id) {
			return serverCount > serverLimit;
		}
	}
	return false;
}

} // namespace AxFolders
