#include "ax/data/media_file.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <iostream>

namespace {

bool Expect(bool value, const char *name) {
	std::cout << (value ? "PASS " : "FAIL ") << name << '\n';
	return value;
}

QByteArray Read(const QString &path) {
	auto file = QFile(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

class InterruptedFile final : public QFile {
public:
	InterruptedFile(const QString &path, std::atomic<bool> &cancelled)
		: QFile(path), _cancelled(cancelled) {
	}

protected:
	qint64 readData(char *data, qint64 size) override {
		const auto read = QFile::readData(data, size);
		_cancelled.store(true);
		return read;
	}

private:
	std::atomic<bool> &_cancelled;
};

} // namespace

int main(int argc, char *argv[]) {
	auto application = QCoreApplication(argc, argv);
	auto directory = QTemporaryDir();
	if (!directory.isValid()) {
		return 1;
	}
	const auto sourcePath = directory.filePath("source");
	const auto targetPath = directory.filePath("archive");
	const auto bytes = QByteArray(200003, 'x');
	auto source = QFile(sourcePath);
	if (!source.open(QIODevice::WriteOnly) || source.write(bytes) != bytes.size()) {
		return 1;
	}
	source.close();
	if (!source.open(QIODevice::ReadOnly)) {
		return 1;
	}
	auto cancelled = std::atomic<bool>(false);
	auto okay = Expect(AxMedia::CopyFile(targetPath, &source, {}, bytes.size(), cancelled), "copy complete file");
	source.close();
	QFile::remove(sourcePath);
	okay &= Expect(Read(targetPath) == bytes, "archive survives source removal");
	okay &= Expect(!AxMedia::CopyFile(targetPath, nullptr, "short", bytes.size(), cancelled)
		&& Read(targetPath) == bytes, "incomplete media preserves previous archive");
	cancelled.store(true);
	okay &= Expect(!AxMedia::CopyFile(targetPath, nullptr, "replacement", -1, cancelled)
		&& Read(targetPath) == bytes, "cancelled copy never replaces archive");
	cancelled.store(false);
	okay &= Expect(AxMedia::CopyFile(directory.filePath("empty"), nullptr, {}, 0, cancelled), "known empty document");
	okay &= Expect(!AxMedia::CopyFile(directory.filePath("unknown"), nullptr, {}, -1, cancelled)
		&& !QFile::exists(directory.filePath("unknown")), "missing media is not an empty archive");
	source.open(QIODevice::WriteOnly);
	source.write(bytes);
	source.close();
	auto interrupted = InterruptedFile(sourcePath, cancelled);
	interrupted.open(QIODevice::ReadOnly);
	okay &= Expect(!AxMedia::CopyFile(targetPath, &interrupted, {}, bytes.size(), cancelled)
		&& Read(targetPath) == bytes, "mid-copy cancellation is atomic");
	interrupted.close();
	okay &= Expect(QDir(directory.path()).entryList(QDir::Files).size() == 3, "no abandoned temporary file");
	okay &= Expect(AxMedia::CheckBudget(7, 3, 100, 10, 0) == AxMedia::State::Saved
		&& AxMedia::CheckBudget(7, 4, 100, 10, 0) == AxMedia::State::LimitReached,
		"exact archive limit and one-byte overflow");
	okay &= Expect(AxMedia::CheckBudget(100, 1, 1000, 0, 0) == AxMedia::State::Disabled
		&& AxMedia::CheckBudget(100, 1, 1000, 50, 0) == AxMedia::State::LimitReached,
		"disabled and reduced limit stop new copies");
	okay &= Expect(AxMedia::CheckBudget(10000, 11, 110, -1, 100) == AxMedia::State::LowSpace,
		"unlimited archive still reserves disk space");
	const auto root = directory.filePath("media");
	const auto owner = root + "/1001";
	QDir().mkpath(owner);
	const auto archived = owner + "/00000000-0000-4000-a000-000000000001.bin";
	const auto orphan = owner + "/00000000-0000-4000-a000-000000000002.bin";
	cancelled.store(false);
	okay &= Expect(AxMedia::CopyFile(archived, nullptr, "abc", 3, cancelled)
		&& AxMedia::CopyFile(orphan, nullptr, "defg", 4, cancelled)
		&& AxMedia::MediaFilesSize(root) == 7, "archive byte accounting");
	AxMedia::WriteMediaState(archived, AxMedia::State::LimitReached);
	okay &= Expect(AxMedia::ReadMediaState(archived) == AxMedia::State::LimitReached, "failure state survives reread");
	const auto pending = owner + "/.00000000-0000-4000-a000-000000000001.bin.ABC123";
	okay &= AxMedia::CopyFile(pending, nullptr, "partial", -1, cancelled);
	const auto note = owner + "/notes.txt";
	okay &= AxMedia::CopyFile(note, nullptr, "keep", -1, cancelled);
	AxMedia::SweepMediaFiles(root, {archived});
	okay &= Expect(QFile::exists(archived) && !QFile::exists(orphan) && !QFile::exists(pending)
		&& QFile::exists(note) && AxMedia::ReadMediaState(archived) == AxMedia::State::LimitReached,
		"recovery keeps references and unknown files, removes orphan and incomplete copy");
	AxMedia::WriteMediaState(archived, AxMedia::State::Saved);
	okay &= Expect(!QFile::exists(archived + ".state") && AxMedia::MediaFilesSize(root) == 3,
		"successful copy clears failure without deleting media");
	return okay ? 0 : 1;
}
