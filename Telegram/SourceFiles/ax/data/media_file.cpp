#include "ax/data/media_file.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QRegularExpression>
#include <algorithm>
#include <limits>

namespace AxMedia {
namespace {

const QRegularExpression OwnerName(QStringLiteral("^[1-9][0-9]*$"));
const QRegularExpression MediaName(QStringLiteral(
	"^\\.?([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\.bin)(\\.state)?(\\.[A-Za-z0-9]{6})?$"));

bool DirectDirectory(const QFileInfo &info) {
	return !info.isSymLink() && info.canonicalFilePath()
		== QDir(info.absolutePath()).canonicalPath() + '/' + info.fileName();
}

} // namespace

State CheckBudget(qint64 used, qint64 additional, qint64 free, qint64 limit, qint64 reserve) {
	if (!limit) return State::Disabled;
	if (used < 0 || additional < 0 || free < 0 || reserve < 0 || limit < -1) return State::Error;
	if (limit >= 0 && (used > limit || additional > limit - used)) return State::LimitReached;
	if (free < reserve || additional > free - reserve) return State::LowSpace;
	return State::Saved;
}

qint64 MediaFilesSize(const QString &root) {
	auto result = qint64(0);
	if (!DirectDirectory(QFileInfo(root))) return result;
	for (const auto &owner : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
		if (!OwnerName.match(owner.fileName()).hasMatch() || !DirectDirectory(owner)) continue;
		for (const auto &file : QDir(owner.absoluteFilePath()).entryInfoList(QDir::Files | QDir::NoSymLinks)) {
			const auto match = MediaName.match(file.fileName());
			if (match.hasMatch() && match.captured(2).isEmpty() && match.captured(3).isEmpty()) {
				if (file.size() > std::numeric_limits<qint64>::max() - result) return std::numeric_limits<qint64>::max();
				result += file.size();
			}
		}
	}
	return result;
}

void SweepMediaFiles(const QString &root, const QSet<QString> &referenced) {
	if (!DirectDirectory(QFileInfo(root))) return;
	for (const auto &owner : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
		if (!OwnerName.match(owner.fileName()).hasMatch() || !DirectDirectory(owner)) continue;
		for (const auto &file : QDir(owner.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks)) {
			const auto match = MediaName.match(file.fileName());
			if (!match.hasMatch()) continue;
			const auto base = QDir(owner.absoluteFilePath()).filePath(match.captured(1));
			if (!match.captured(3).isEmpty() || !referenced.contains(base)) {
				QFile::remove(file.absoluteFilePath());
			}
		}
	}
}

State ReadMediaState(const QString &path) {
	auto file = QFile(path + QStringLiteral(".state"));
	if (!file.open(QIODevice::ReadOnly)) return State::Unavailable;
	const auto bytes = file.read(16);
	auto okay = false;
	const auto value = bytes.toInt(&okay);
	return okay && value >= int(State::Unavailable) && value <= int(State::Disabled)
		? State(value) : State::Unavailable;
}

void WriteMediaState(const QString &path, State state) {
	const auto name = path + QStringLiteral(".state");
	if (state == State::Saved) {
		QFile::remove(name);
		return;
	}
	auto file = QSaveFile(name);
	file.setDirectWriteFallback(false);
	const auto bytes = QByteArray::number(int(state));
	if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()) {
		file.commit();
	}
}

bool CopyFile(
		const QString &path,
		QFile *source,
		const QByteArray &bytes,
		qint64 expected,
		const std::atomic<bool> &cancelled) {
	const auto size = source ? source->size() : qint64(bytes.size());
	const auto modified = source ? QFileInfo(*source).lastModified() : QDateTime();
	if (cancelled.load()
		|| ((expected >= 0) ? (size != expected) : (size <= 0))) {
		return false;
	}
	auto target = QSaveFile(path);
	target.setDirectWriteFallback(false);
	if (!target.open(QIODevice::WriteOnly)) {
		return false;
	}
	auto success = true;
	if (source) {
		auto buffer = QByteArray(64 * 1024, Qt::Uninitialized);
		auto copied = qint64(0);
		while (success && copied < size && !cancelled.load()) {
			const auto count = source->read(buffer.data(), buffer.size());
			success = (count > 0) && (target.write(buffer.constData(), count) == count);
			copied += std::max(count, qint64(0));
		}
		success = success && (copied == size) && (source->size() == size)
			&& (QFileInfo(*source).lastModified() == modified);
	} else {
		success = (target.write(bytes) == bytes.size());
	}
	if (success && !cancelled.load()) {
		return target.commit();
	}
	target.cancelWriting();
	return false;
}

} // namespace AxMedia
