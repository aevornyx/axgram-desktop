#pragma once

namespace AxFolders {

[[nodiscard]] constexpr bool MatchesArchiveRule(
		bool onlyArchived,
		bool excludeArchived,
		bool known,
		bool archived) {
	return (!onlyArchived && !excludeArchived)
		|| (known && (onlyArchived != excludeArchived)
			&& (onlyArchived == archived));
}

}
