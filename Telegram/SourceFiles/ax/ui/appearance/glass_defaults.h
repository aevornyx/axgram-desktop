// AxGram first-profile appearance policy. Modified 2026-11-03. GPLv3.
#pragma once

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <algorithm>

namespace AxGlass {

[[nodiscard]] inline bool IsFreshProfile(const QString &workingDirectory) {
	const auto root = QDir(workingDirectory);
	if (QFile::exists(root.filePath(QStringLiteral("data")))
		|| QFile::exists(root.filePath(QStringLiteral("data_config")))) {
		return false;
	}
	const auto entries = QDir(root.filePath(QStringLiteral("tdata"))).entryList(
		QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
	return std::all_of(entries.begin(), entries.end(), [](const QString &name) {
		return name == QStringLiteral("working")
			|| name == QStringLiteral("usertag")
			|| name == QStringLiteral("dumps");
	});
}

} // namespace AxGlass
