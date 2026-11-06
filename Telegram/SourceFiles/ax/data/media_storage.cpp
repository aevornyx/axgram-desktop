#include "ax/data/media_storage.h"

#include "ax/data/media_file.h"
#include "ax/data/ax_database.h"
#include "ax/ax_settings.h"
#include "ax/utils/ax_mapper.h"
#include "core/file_location.h"
#include "core/version.h"
#include "data/data_file_origin.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "storage/cache/storage_cache_database.h"
#include "storage/serialize_common.h"
#include "storage/serialize_document.h"
#include "storage/serialize_peer.h"
#include "ui/image/image_location.h"

#include <crl/crl_queue.h>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>
#include <QtCore/QStorageInfo>
#include <QtCore/QTemporaryDir>
#include <atomic>

namespace AxMedia {
namespace {

constexpr auto kMagic = quint32(0x41584D31);
constexpr auto kFormat = quint32(1);
constexpr auto kDocument = 1;
constexpr auto kPhoto = 2;
using Cancelled = std::shared_ptr<std::atomic<bool>>;

crl::queue Queue;
std::atomic<bool> Accepting = true;
constexpr auto MiB = qint64(1024 * 1024);
constexpr auto FreeSpaceReserve = 256 * MiB;
std::atomic<qint64> Limit = 5120 * MiB;
qint64 UsedMedia = -1;
bool RecoveryQueued = false;
std::map<QString, State> States;
std::map<QString, Cancelled> Pending;
std::set<QString> Again;
struct Retry {
	base::weak_ptr<Main::Session> session;
	Fn<void()> run;
	std::shared_ptr<QTemporaryDir> temporary;
	Fn<bool()> loading;
};
std::map<QString, Retry> Retries;
std::set<QString> VoiceLoadAttempts;
std::vector<base::weak_ptr<Main::Session>> Watching;

void Watch(not_null<Main::Session*> session) {
	Watching.erase(std::remove_if(Watching.begin(), Watching.end(), [](const auto &weak) {
		return !weak.get();
	}), Watching.end());
	for (const auto &weak : Watching) {
		if (weak.get() == session) {
			return;
		}
	}
	Watching.push_back(base::make_weak(session));
	session->downloaderTaskFinished() | rpl::on_next([=] {
		auto callbacks = std::vector<Fn<void()>>();
		for (auto i = Retries.begin(); i != Retries.end();) {
			if (!i->second.session.get()) {
				i = Retries.erase(i);
			} else {
				if (i->second.session.get() == session) {
					if (Pending.contains(i->first)) {
						Again.insert(i->first);
					} else {
						callbacks.push_back(i->second.run);
					}
				}
				++i;
			}
		}
		for (const auto &callback : callbacks) {
			callback();
		}
	}, session->lifetime());
}

QString OwnerRoot(ID owner) {
	return QDir(cWorkingDir()).absoluteFilePath(
		u"tdata/axmedia/%1/"_q.arg(owner));
}

QString Root() {
	return QDir(cWorkingDir()).absoluteFilePath(u"tdata/axmedia"_q);
}

bool OwnedPath(ID owner, const QString &path) {
	if (owner <= 0 || path.isEmpty() || QFileInfo(Root()).isSymLink() || QFileInfo(OwnerRoot(owner)).isSymLink()) {
		return false;
	}
	const auto root = QFileInfo(OwnerRoot(owner)).canonicalFilePath();
	const auto parent = QFileInfo(path).dir().canonicalPath();
	return !root.isEmpty()
		&& (parent == root)
		&& !QFileInfo(path).isSymLink();
}

void EnsureRecovery() {
	if (RecoveryQueued) return;
	const auto references = AxDatabase::mediaReferences();
	if (!references) return;
	auto paths = QSet<QString>();
	for (const auto &[owner, saved] : *references) {
		const auto path = QString::fromStdString(saved);
		if (OwnedPath(owner, path)) paths.insert(QFileInfo(path).absoluteFilePath());
	}
	for (const auto &[path, flag] : Pending) paths.insert(path);
	RecoveryQueued = true;
	const auto root = Root();
	Queue.async([=] {
		SweepMediaFiles(root, paths);
		UsedMedia = MediaFilesSize(root);
	});
}

bool Complete(const QString &path, qint64 expected) {
	const auto info = QFileInfo(path);
	return info.isFile()
		&& ((expected >= 0) ? (info.size() == expected) : (info.size() > 0));
}

void Ready(
		base::weak_ptr<Main::Session> weak,
		int kind,
		uint64 id,
		QString path) {
	if (!Accepting.load()) {
		return;
	}
	Queue.async([=] {
		auto bytes = QByteArray();
		auto image = QImage();
		if (kind == kPhoto) {
			auto file = QFile(path);
			if (file.open(QIODevice::ReadOnly)) {
				bytes = file.readAll();
				image = QImage::fromData(bytes);
			}
		}
		crl::on_main([=, bytes = std::move(bytes), image = std::move(image)]() mutable {
			const auto session = weak.get();
			if (!session || !QFileInfo::exists(path)) {
				return;
			}
			if (kind == kDocument) {
				const auto document = session->data().document(id);
				document->setLocation(Core::FileLocation(path));
				session->data().notifyDocumentLayoutChanged(document);
			} else if (!image.isNull()) {
				const auto photo = session->data().photo(id);
				photo->setLocation(Core::FileLocation(path));
				photo->createMediaView()->set(Data::PhotoSize::Large,
					Data::PhotoSize::Large, std::move(image), std::move(bytes));
				session->data().notifyPhotoLayoutChanged(photo);
			}
		});
	});
}

void Write(
		base::weak_ptr<Main::Session> weak,
		QString path,
		int kind,
		uint64 id,
		qint64 expected,
		Cancelled cancelled,
		std::shared_ptr<QFile> source,
		QByteArray bytes) {
	if (!Accepting.load()) {
		return;
	}
	Queue.async([=, bytes = std::move(bytes)] {
		const auto root = QFileInfo(path).dir().absolutePath() + u"/.."_q;
		if (UsedMedia < 0) UsedMedia = MediaFilesSize(QDir::cleanPath(root));
		const auto size = source ? source->size() : qint64(bytes.size());
		const auto oldSize = QFileInfo(path).isFile() ? QFileInfo(path).size() : 0;
		const auto free = QStorageInfo(QFileInfo(path).absolutePath()).bytesAvailable();
		auto state = State::Unavailable;
		if (!cancelled->load() && ((expected >= 0) ? size == expected : size > 0)) {
			state = CheckBudget(std::max(qint64(0), UsedMedia - oldSize), size, free, Limit.load(), FreeSpaceReserve);
			if (state == State::LimitReached) {
				UsedMedia = MediaFilesSize(QDir::cleanPath(root));
				state = CheckBudget(std::max(qint64(0), UsedMedia - oldSize), size, free, Limit.load(), FreeSpaceReserve);
			}
			if (state == State::Saved) {
				WriteMediaState(path, State::Saving);
				state = CopyFile(path, source.get(), bytes, expected, *cancelled) ? State::Saved : State::Error;
				if (state == State::Saved) UsedMedia = std::max(qint64(0), UsedMedia - oldSize) + size;
			}
		}
		const auto success = state == State::Saved;
		if (!cancelled->load()) WriteMediaState(path, state);
		if (state == State::Error) {
			LOG(("AxGram: could not preserve archive media."));
		}
		crl::on_main([=] {
			const auto i = Pending.find(path);
			if (i != Pending.end() && i->second == cancelled) {
				Pending.erase(i);
			}
			if (!cancelled->load()) States[path] = state;
			if (success && !cancelled->load()) {
				Retries.erase(path);
				Ready(weak, kind, id, path);
			} else if (Again.erase(path) && !cancelled->load() && state == State::Unavailable) {
				if (const auto retry = Retries.find(path); retry != Retries.end()) {
					const auto run = retry->second.run;
					run();
				}
			}
			Again.erase(path);
		});
	});
}

void Preserve(
		not_null<Main::Session*> session,
		const AxMessageBase &message,
		int kind,
		uint64 id,
		qint64 expected,
		QString sourcePath,
		QByteArray bytes,
		Storage::Cache::Key key) {
	const auto path = QString::fromStdString(message.mediaPath);
	SetLimitMb(AxSettings::getInstance().archiveMediaLimitMb());
	const auto name = QFileInfo(path).fileName();
	if (QFileInfo(path).absolutePath() == QDir::cleanPath(OwnerRoot(message.userId))
		&& name.endsWith(u".bin"_q) && !QUuid(name.chopped(4)).isNull()) {
		QDir().mkpath(OwnerRoot(message.userId));
	}
	EnsureRecovery();
	if (!Accepting.load() || !OwnedPath(message.userId, path) || Pending.contains(path)) {
		return;
	}
	const auto weak = base::make_weak(session);
	if (Complete(path, expected)) {
		Retries.erase(path);
		Ready(weak, kind, id, path);
		return;
	}
	Watch(session);
	const auto previous = Retries.find(path);
	const auto temporary = previous != Retries.end()
		? previous->second.temporary : nullptr;
	const auto loading = previous != Retries.end()
		? previous->second.loading : Fn<bool()>();
	Retries.insert_or_assign(path, Retry{ weak, [=] {
		if (const auto live = weak.get()) {
			if (kind == kDocument) {
				const auto document = live->data().document(id);
				const auto view = document->activeMediaView();
				Preserve(live, message, kind, id, expected, document->filepath(true),
					view ? view->bytes() : QByteArray(), document->cacheKey());
			} else {
				const auto photo = live->data().photo(id);
				const auto view = photo->activeMediaView();
				const auto full = view && (view->size(Data::PhotoSize::Large) == photo->size(Data::PhotoSize::Large));
				Preserve(live, message, kind, id, expected, photo->location(true).name(),
					full ? view->imageBytes(Data::PhotoSize::Large) : QByteArray(), key);
			}
		}
	}, temporary, loading });
	const auto cancelled = std::make_shared<std::atomic<bool>>(false);
	Pending.emplace(path, cancelled);
	auto source = std::make_shared<QFile>(sourcePath);
	if (!sourcePath.isEmpty() && source->open(QIODevice::ReadOnly)) {
		source->moveToThread(nullptr);
		Write(weak, path, kind, id, expected, cancelled, std::move(source), {});
	} else if (!bytes.isEmpty()) {
		Write(weak, path, kind, id, expected, cancelled, nullptr, std::move(bytes));
	} else {
		session->data().cache().get(key, [=](QByteArray &&cached) {
			if (!cancelled->load()) {
				Write(weak, path, kind, id, expected, cancelled, nullptr, std::move(cached));
			}
		});
	}
}

} // namespace

void Capture(not_null<HistoryItem*> item, AxMessageBase &message) {
	const auto media = item->media();
	if (!media || message.userId <= 0) {
		return;
	}
	const auto document = media->document();
	const auto photo = media->photo();
	if (!document && !photo) {
		return;
	}
	const auto root = OwnerRoot(message.userId);
	if (!QDir().mkpath(root)) {
		LOG(("AxGram: could not create archive media directory."));
		return;
	}
	message.mediaPath = QDir(root).filePath(
		QUuid::createUuid().toString(QUuid::WithoutBraces) + u".bin"_q).toStdString();
	message.documentType = document ? kDocument : kPhoto;
	auto writer = Serialize::ByteArrayWriter();
	writer << kMagic << kFormat << qint32(AppVersion);
	if (document) {
		writer << qint64(document->size);
		Serialize::Document::writeToStream(writer, document);
		message.documentAttributesSerialized = AxMapper::serializeDocumentAttributes(document);
		message.mimeType = document->mimeString().toStdString();
	} else {
		const auto inputPhoto = photo->mtpInput();
		const auto &input = inputPhoto.c_inputPhoto();
		writer << quint64(photo->id) << quint64(input.vaccess_hash().v)
			<< photo->fileReference() << qint32(photo->getDC())
			<< qint32(photo->date()) << photo->hasAttachedStickers()
			<< photo->inlineThumbnailBytes();
		for (const auto size : { Data::PhotoSize::Small, Data::PhotoSize::Thumbnail, Data::PhotoSize::Large }) {
			Serialize::writeImageLocation(writer, photo->location(size));
			writer << qint32(photo->imageByteSize(size));
		}
	}
	const auto blob = std::move(writer).result();
	message.documentSerialized.assign(blob.begin(), blob.end());
	const auto restored = Restore(&item->history()->session(), message);
}

Restored Restore(not_null<Main::Session*> session, const AxMessageBase &message) {
	auto result = Restored();
	const auto owner = ID(session->userId().bare & PeerId::kChatTypeMask);
	if (message.userId != owner || message.documentSerialized.empty()) {
		return result;
	}
	auto reader = Serialize::ByteArrayReader(QByteArray(
		message.documentSerialized.data(), message.documentSerialized.size()));
	auto magic = quint32(0), format = quint32(0);
	auto version = qint32(0);
	reader >> magic >> format >> version;
	if (magic != kMagic || format != kFormat || version <= 0 || version > AppVersion) {
		return result;
	}
	if (message.documentType == kDocument) {
		auto size = qint64(0);
		reader >> size;
		const auto document = Serialize::Document::readFromStream(session, version, reader);
		if (!document || !reader.ok() || !reader.atEnd() || size < 0) {
			return result;
		}
		document->size = size;
		if (!message.documentAttributesSerialized.empty()) {
			document->setattributes(AxMapper::deserializeDocumentAttributes(
				message.documentAttributesSerialized).v);
		}
		result.document = document;
		const auto view = document->activeMediaView();
		Preserve(session, message, kDocument, document->id, size,
			document->filepath(true), view ? view->bytes() : QByteArray(), document->cacheKey());
		const auto path = QString::fromStdString(message.mediaPath);
		if (document->isVoiceMessage() && Limit.load() != 0
			&& OwnedPath(message.userId, path) && !Complete(path, size)
			&& !document->loading() && Retries.contains(path)
			&& VoiceLoadAttempts.insert(path).second) {
			const auto temporary = std::make_shared<QTemporaryDir>();
			if (temporary->isValid()) {
				const auto weak = base::make_weak(session);
				const auto id = document->id;
				auto &retry = Retries.at(path);
				retry.temporary = temporary;
				retry.loading = [=] {
					const auto live = weak.get();
					return live && live->data().document(id)->loading();
				};
				LOG(("AxGram: preparing complete voice file for archive."));
				document->save(Data::FileOrigin(),
					temporary->filePath(u"voice.ogg"_q), LoadFromCloudOrLocal, true);
			}
		}
	} else if (message.documentType == kPhoto) {
		auto id = quint64(0), access = quint64(0);
		auto reference = QByteArray(), thumbnail = QByteArray();
		auto dc = qint32(0), date = qint32(0);
		auto stickers = false;
		reader >> id >> access >> reference >> dc >> date >> stickers >> thumbnail;
		auto images = std::array<ImageWithLocation, 3>();
		for (auto &image : images) {
			const auto location = Serialize::readImageLocation(version, reader);
			if (!location) {
				return result;
			}
			image.location = *location;
			reader >> image.bytesCount;
			if (image.bytesCount < 0) {
				return result;
			}
		}
		if (!reader.ok() || !reader.atEnd() || !id) {
			return result;
		}
		const auto photo = session->data().photo(id);
		photo->setRemoteLocation(dc, access, reference);
		photo->setFields(date, stickers);
		photo->updateImages(thumbnail, images[0], images[1], images[2], {}, {}, 0);
		result.photo = photo;
		const auto view = photo->activeMediaView();
		const auto bytes = view && (view->size(Data::PhotoSize::Large) == photo->size(Data::PhotoSize::Large))
			? view->imageBytes(Data::PhotoSize::Large) : QByteArray();
		Preserve(session, message, kPhoto, id,
			images[2].bytesCount > 0 ? images[2].bytesCount : -1,
			photo->location(true).name(), bytes, images[2].location.file().cacheKey());
	}
	return result;
}

void Remove(ID owner, const std::string &savedPath) {
	const auto path = QString::fromStdString(savedPath);
	if (!OwnedPath(owner, path)) {
		return;
	}
	Retries.erase(path);
	VoiceLoadAttempts.erase(path);
	States.erase(path);
	Again.erase(path);
	if (const auto i = Pending.find(path); i != Pending.end()) {
		i->second->store(true);
		Pending.erase(i);
	}
	Queue.async([=] {
		if (OwnedPath(owner, path)) {
			const auto size = QFileInfo(path).size();
			if (QFile::remove(path) && UsedMedia >= 0) UsedMedia = std::max(qint64(0), UsedMedia - size);
			QFile::remove(path + u".state"_q);
		}
	});
}

void Flush() {
	Accepting.store(false);
	Queue.sync([] {});
}

void SetLimitMb(int value) {
	Limit.store(value < 0 ? -1 : qint64(value) * MiB);
}

State Status(const AxMessageBase &message) {
	const auto path = QString::fromStdString(message.mediaPath);
	if (!OwnedPath(message.userId, path)) return State::Unavailable;
	if (QFileInfo(path).isFile()) return State::Saved;
	if (Pending.contains(path)) return State::Saving;
	if (const auto retry = Retries.find(path); retry != Retries.end()) {
		if (retry->second.loading && retry->second.loading()) {
			return State::Saving;
		}
	}
	const auto i = States.find(path);
	const auto state = i != States.end() ? i->second : ReadMediaState(path);
	return state == State::Saving || state == State::Saved ? State::Unavailable : state;
}

QString StatusText(const AxMessageBase &message) {
	switch (Status(message)) {
	case State::Saved: return u"Файл сохранён в архиве."_q;
	case State::Saving: return u"Сохраняю файл в архив…"_q;
	case State::LimitReached: return u"Файл не сохранён: достигнут лимит медиаархива. Лимит можно изменить в настройках AxGram. Старые файлы не удаляются."_q;
	case State::LowSpace: return u"Файл не сохранён: мало свободного места на диске."_q;
	case State::Disabled: return u"Сохранение файлов выключено в настройках медиаархива. Текст сообщений сохраняется."_q;
	case State::Error: return u"Не удалось сохранить файл. Проверь свободное место и доступ к папке архива, затем повтори сохранение."_q;
	default: return u"Файл не сохранён в архиве. Если исходник остался в кэше, сохранение можно повторить. Если оригинал уже удалён и локальной копии нет, восстановление может быть невозможно."_q;
	}
}

} // namespace AxMedia
