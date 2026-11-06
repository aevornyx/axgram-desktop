#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <atomic>

class QFile;

namespace AxMedia {

enum class State {
	Unavailable,
	Saving,
	Saved,
	LimitReached,
	LowSpace,
	Error,
	Disabled,
};

[[nodiscard]] State CheckBudget(qint64 used, qint64 additional, qint64 free,
	qint64 limit, qint64 reserve);
[[nodiscard]] qint64 MediaFilesSize(const QString &root);
void SweepMediaFiles(const QString &root, const QSet<QString> &referenced);
[[nodiscard]] State ReadMediaState(const QString &path);
void WriteMediaState(const QString &path, State state);

[[nodiscard]] bool CopyFile(
	const QString &path,
	QFile *source,
	const QByteArray &bytes,
	qint64 expected,
	const std::atomic<bool> &cancelled);

} // namespace AxMedia
