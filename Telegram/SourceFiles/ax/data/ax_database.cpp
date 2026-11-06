// Р§Р°СЃС‚СЊ AxGram РґР»СЏ Desktop. Р¤Р°Р№Р» РёР·РјРµРЅС‘РЅ.
//
// РћСЃРЅРѕРІР°РЅРѕ РЅР° РёСЃС…РѕРґРЅРѕРј РєРѕРґРµ AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Р Р°СЃРїСЂРѕСЃС‚СЂР°РЅСЏРµС‚СЃСЏ РїРѕ GNU GPL v3 вЂ” СЃРј. LICENSE Рё NOTICE.md.
#include "ax/data/ax_database.h"

#include "ax/data/entities.h"
#include "ax/libs/sqlite/sqlite_orm.h"
#include "ax/data/archive_database.h"
#include "ax/data/archive_key_store.h"

#include <openssl/crypto.h>
#include <atomic>
#include <mutex>
#include <fstream>
#include <stdexcept>

using namespace sqlite_orm;

namespace {

std::unique_ptr<AxArchive::DeviceKey> ArchiveKey;
std::atomic<bool> ArchiveReady = false;
std::recursive_mutex ArchiveDatabaseMutex;

auto CreateStorage() {
	return make_storage(
	"./tdata/axdata.db",
	make_table<SchemaVersion>(
		"SchemaVersion",
		make_column("id", &SchemaVersion::id, primary_key()),
		make_column("version", &SchemaVersion::version)
	),
	make_index("idx_deleted_message_userId_dialogId_topicId_messageId",
			   column<DeletedMessage>(&DeletedMessage::userId),
			   column<DeletedMessage>(&DeletedMessage::dialogId),
			   column<DeletedMessage>(&DeletedMessage::topicId),
			   column<DeletedMessage>(&DeletedMessage::messageId)),
	make_index("idx_edited_message_userId_dialogId_messageId",
			   column<EditedMessage>(&EditedMessage::userId),
			   column<EditedMessage>(&EditedMessage::dialogId),
			   column<EditedMessage>(&EditedMessage::messageId)),
	make_table<DeletedMessage>(
		"DeletedMessage",
		make_column("fakeId", &DeletedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedMessage::userId),
		make_column("dialogId", &DeletedMessage::dialogId),
		make_column("groupedId", &DeletedMessage::groupedId),
		make_column("peerId", &DeletedMessage::peerId),
		make_column("fromId", &DeletedMessage::fromId),
		make_column("topicId", &DeletedMessage::topicId),
		make_column("messageId", &DeletedMessage::messageId),
		make_column("date", &DeletedMessage::date),
		make_column("flags", &DeletedMessage::flags),
		make_column("editDate", &DeletedMessage::editDate),
		make_column("views", &DeletedMessage::views),
		make_column("fwdFlags", &DeletedMessage::fwdFlags),
		make_column("fwdFromId", &DeletedMessage::fwdFromId),
		make_column("fwdName", &DeletedMessage::fwdName),
		make_column("fwdDate", &DeletedMessage::fwdDate),
		make_column("fwdPostAuthor", &DeletedMessage::fwdPostAuthor),
		make_column("replyFlags", &DeletedMessage::replyFlags),
		make_column("replyMessageId", &DeletedMessage::replyMessageId),
		make_column("replyPeerId", &DeletedMessage::replyPeerId),
		make_column("replyTopId", &DeletedMessage::replyTopId),
		make_column("replyForumTopic", &DeletedMessage::replyForumTopic),
		make_column("replySerialized", &DeletedMessage::replySerialized),
		make_column("entityCreateDate", &DeletedMessage::entityCreateDate),
		make_column("text", &DeletedMessage::text),
		make_column("textEntities", &DeletedMessage::textEntities),
		make_column("mediaPath", &DeletedMessage::mediaPath),
		make_column("hqThumbPath", &DeletedMessage::hqThumbPath),
		make_column("documentType", &DeletedMessage::documentType),
		make_column("documentSerialized", &DeletedMessage::documentSerialized),
		make_column("thumbsSerialized", &DeletedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &DeletedMessage::documentAttributesSerialized),
		make_column("mimeType", &DeletedMessage::mimeType)
	),
	make_table<EditedMessage>(
		"EditedMessage",
		make_column("fakeId", &EditedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &EditedMessage::userId),
		make_column("dialogId", &EditedMessage::dialogId),
		make_column("groupedId", &EditedMessage::groupedId),
		make_column("peerId", &EditedMessage::peerId),
		make_column("fromId", &EditedMessage::fromId),
		make_column("topicId", &EditedMessage::topicId),
		make_column("messageId", &EditedMessage::messageId),
		make_column("date", &EditedMessage::date),
		make_column("flags", &EditedMessage::flags),
		make_column("editDate", &EditedMessage::editDate),
		make_column("views", &EditedMessage::views),
		make_column("fwdFlags", &EditedMessage::fwdFlags),
		make_column("fwdFromId", &EditedMessage::fwdFromId),
		make_column("fwdName", &EditedMessage::fwdName),
		make_column("fwdDate", &EditedMessage::fwdDate),
		make_column("fwdPostAuthor", &EditedMessage::fwdPostAuthor),
		make_column("replyFlags", &EditedMessage::replyFlags),
		make_column("replyMessageId", &EditedMessage::replyMessageId),
		make_column("replyPeerId", &EditedMessage::replyPeerId),
		make_column("replyTopId", &EditedMessage::replyTopId),
		make_column("replyForumTopic", &EditedMessage::replyForumTopic),
		make_column("replySerialized", &EditedMessage::replySerialized),
		make_column("entityCreateDate", &EditedMessage::entityCreateDate),
		make_column("text", &EditedMessage::text),
		make_column("textEntities", &EditedMessage::textEntities),
		make_column("mediaPath", &EditedMessage::mediaPath),
		make_column("hqThumbPath", &EditedMessage::hqThumbPath),
		make_column("documentType", &EditedMessage::documentType),
		make_column("documentSerialized", &EditedMessage::documentSerialized),
		make_column("thumbsSerialized", &EditedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &EditedMessage::documentAttributesSerialized),
		make_column("mimeType", &EditedMessage::mimeType)
	),
	make_table<DeletedDialog>(
		"DeletedDialog",
		make_column("fakeId", &DeletedDialog::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedDialog::userId),
		make_column("dialogId", &DeletedDialog::dialogId),
		make_column("peerId", &DeletedDialog::peerId),
		make_column("folderId", &DeletedDialog::folderId),
		make_column("topMessage", &DeletedDialog::topMessage),
		make_column("lastMessageDate", &DeletedDialog::lastMessageDate),
		make_column("flags", &DeletedDialog::flags),
		make_column("entityCreateDate", &DeletedDialog::entityCreateDate)
	),
	make_table<RegexFilter>(
		"RegexFilter",
		make_column("id", &RegexFilter::id, primary_key()),
		make_column("text", &RegexFilter::text),
		make_column("enabled", &RegexFilter::enabled),
		make_column("reversed", &RegexFilter::reversed),
		make_column("caseInsensitive", &RegexFilter::caseInsensitive),
		make_column("dialogId", &RegexFilter::dialogId)
	),
	make_table<RegexFilterGlobalExclusion>(
		"RegexFilterGlobalExclusion",
		make_column("fakeId", &RegexFilterGlobalExclusion::fakeId, primary_key().autoincrement()),
		make_column("dialogId", &RegexFilterGlobalExclusion::dialogId),
		make_column("filterId", &RegexFilterGlobalExclusion::filterId)
	),
	make_table<SpyMessageRead>(
		"SpyMessageRead",
		make_column("fakeId", &SpyMessageRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageRead::userId),
		make_column("dialogId", &SpyMessageRead::dialogId),
		make_column("messageId", &SpyMessageRead::messageId),
		make_column("entityCreateDate", &SpyMessageRead::entityCreateDate)
	),
	make_table<SpyMessageContentsRead>(
		"SpyMessageContentsRead",
		make_column("fakeId", &SpyMessageContentsRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageContentsRead::userId),
		make_column("dialogId", &SpyMessageContentsRead::dialogId),
		make_column("messageId", &SpyMessageContentsRead::messageId),
		make_column("entityCreateDate", &SpyMessageContentsRead::entityCreateDate)
	)
	);
}

using ArchiveStorage = decltype(CreateStorage());

ArchiveStorage &ArchiveDatabaseStorage() {
	static auto result = [] {
		if (OPENSSL_init_crypto(0, nullptr) != 1
			|| sqlite3_initialize() != SQLITE_OK) {
			throw std::runtime_error("Archive crypto initialization failed");
		}
		return CreateStorage();
	}();
	return result;
}

}

namespace AxMigrations {

void migrateToV1(ArchiveStorage &storage) {
	// drop RegexFilter table as we've added primary_key()
	try {
		storage.drop_table_if_exists("RegexFilter");
		LOG(("Migration to V1 successful."));
	} catch (const std::exception &ex) {
		LOG(("Migration to V1 failed: %1").arg(ex.what()));
		throw;
	}
}

}

void runMigrations(ArchiveStorage &storage) {
	constexpr int kLatestVersion = 1;

	const std::map<int, Fn<void(ArchiveStorage &)>> migrations = {
		{1, AxMigrations::migrateToV1},
	};

	int currentVersion = 0;
	try {
		if (auto versionRow = storage.get_pointer<SchemaVersion>(1)) {
			currentVersion = versionRow->version;
		} else {
			storage.insert(SchemaVersion{1, 0});
		}
	} catch (...) {
		LOG(("No SchemaVersion, assuming 0"));
		storage.insert(SchemaVersion{1, 0});
	}

	if (currentVersion >= kLatestVersion) {
		LOG(("Database is ok"));
		return;
	}

	LOG(("Database version: %1. Latest version: %2.").arg(currentVersion).arg(kLatestVersion));

	for (int v = currentVersion + 1; v <= kLatestVersion; ++v) {
		if (migrations.contains(v)) {
			try {
				LOG(("Migration for version: %1").arg(v));
				storage.begin_transaction();

				migrations.at(v)(storage);

				storage.update_all(set(c(&SchemaVersion::version) = v), where(c(&SchemaVersion::id) == 1));
				storage.commit();
				LOG(("Applied migration for version: %1.").arg(v));
			} catch (...) {
				storage.rollback();
				LOG(("Failed to apply migration for version: %1.").arg(v));
				throw;
			}
		}
	}
}

namespace AxDatabase {

void migrateLegacyDatabase() {
	// AxGram: Р±Р°Р·Р° РЅР°Р·С‹РІР°Р»Р°СЃСЊ ayudata.db РїРѕ РёСЃС…РѕРґРЅРѕРјСѓ РїСЂРѕРµРєС‚Сѓ. РџРµСЂРµРЅРѕСЃРёРј РµС‘
	// РѕРґРёРЅ СЂР°Р·, РёРЅР°С‡Рµ РЅР°РєРѕРїР»РµРЅРЅС‹Рµ СѓРґР°Р»С‘РЅРЅС‹Рµ СЃРѕРѕР±С‰РµРЅРёСЏ РѕСЃС‚Р°Р»РёСЃСЊ Р±С‹ РІ С„Р°Р№Р»Рµ,
	// РєРѕС‚РѕСЂС‹Р№ РїСЂРёР»РѕР¶РµРЅРёРµ Р±РѕР»СЊС€Рµ РЅРµ РѕС‚РєСЂС‹РІР°РµС‚.
	if (QFile::exists("./tdata/axdata.db")) {
		return;
	}
	if (!QFile::exists("./tdata/ayudata.db")) {
		return;
	}
	auto raw = static_cast<sqlite3*>(nullptr);
	const auto opened = sqlite3_open_v2("./tdata/ayudata.db", &raw,
		SQLITE_OPEN_READWRITE, nullptr);
	auto journal = static_cast<sqlite3_stmt*>(nullptr);
	const auto prepared = (opened == SQLITE_OK)
		? sqlite3_prepare_v2(raw, "PRAGMA journal_mode=DELETE", -1, &journal, nullptr)
		: SQLITE_ERROR;
	const auto consolidated = prepared == SQLITE_OK
		&& sqlite3_step(journal) == SQLITE_ROW
		&& std::string(reinterpret_cast<const char*>(sqlite3_column_text(journal, 0))) == "delete";
	sqlite3_finalize(journal);
	sqlite3_close(raw);
	if (!consolidated || !QFile::rename("./tdata/ayudata.db", "./tdata/axdata.db")) {
		throw std::runtime_error("Legacy archive could not be moved safely");
	}
}

bool available() {
	return ArchiveReady.load();
}

void initialize() {
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	if (ArchiveReady.load()) return;
	try {
		migrateLegacyDatabase();
#if defined(Q_OS_WIN) && defined(AXGRAM_ENCRYPT_ARCHIVE_DATABASE)
		const auto path = std::filesystem::path("./tdata/axdata.db");
		const auto kind = AxArchive::InspectDatabase(path);
		const auto allowCreate = kind == AxArchive::DatabaseKind::Missing
			|| kind == AxArchive::DatabaseKind::Plain;
		if (kind == AxArchive::DatabaseKind::Invalid) {
			throw std::runtime_error("Archive files require recovery");
		}
		auto error = AxArchive::KeyError::None;
		ArchiveKey = AxArchive::LoadDeviceKey("./tdata/axarchive.key", allowCreate, error);
		if (!ArchiveKey) throw std::runtime_error("Archive device key unavailable");
		const auto protectedDatabase = AxArchive::ProtectDatabase(path, ArchiveKey->bytes());
		if (protectedDatabase != AxArchive::ProtectError::None
			&& protectedDatabase != AxArchive::ProtectError::Missing) {
			throw std::runtime_error("Archive encryption or verification failed");
		}
		ArchiveDatabaseStorage().on_open = [](sqlite3 *db) {
			if (!ArchiveKey || sqlite3_key(db, ArchiveKey->bytes().data(), 32) != SQLITE_OK
				|| sqlite3_exec(db, "SELECT count(*) FROM sqlite_master", nullptr, nullptr, nullptr) != SQLITE_OK) {
				throw std::runtime_error("Archive key rejected");
			}
		};
#endif
		ArchiveDatabaseStorage().open_forever();
		ArchiveDatabaseStorage().sync_schema(true);
		runMigrations(ArchiveDatabaseStorage());
		ArchiveDatabaseStorage().sync_schema(true);
		ArchiveReady.store(true);
	} catch (const std::exception &ex) {
		LOG(("Archive unavailable; existing files preserved: %1").arg(ex.what()));
	}
}

bool addEditedMessage(const EditedMessage &message) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().begin_transaction();
		ArchiveDatabaseStorage().insert(message);
		ArchiveDatabaseStorage().commit();
		return true;
	} catch (std::exception &ex) {
		try {
			ArchiveDatabaseStorage().rollback();
		} catch (...) {
		}
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
		return false;
	}
}

std::optional<std::vector<std::pair<ID, std::string>>> mediaReferences() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		auto result = std::vector<std::pair<ID, std::string>>();
		for (const auto &[owner, path] : ArchiveDatabaseStorage().select(columns(
			column<DeletedMessage>(&DeletedMessage::userId),
			column<DeletedMessage>(&DeletedMessage::mediaPath)))) {
			result.emplace_back(owner, path);
		}
		for (const auto &[owner, path] : ArchiveDatabaseStorage().select(columns(
			column<EditedMessage>(&EditedMessage::userId),
			column<EditedMessage>(&EditedMessage::mediaPath)))) {
			result.emplace_back(owner, path);
		}
		return result;
	} catch (const std::exception &error) {
		LOG(("AxGram: media references could not be read: %1").arg(error.what()));
		return std::nullopt;
	}
}

std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	return ArchiveDatabaseStorage().get_all<EditedMessage>(
		where(
			column<EditedMessage>(&EditedMessage::userId) == userId and
			column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
			column<EditedMessage>(&EditedMessage::messageId) == messageId and
			(column<EditedMessage>(&EditedMessage::fakeId) > minId or minId == 0) and
			(column<EditedMessage>(&EditedMessage::fakeId) < maxId or maxId == 0)
		),
		order_by(column<EditedMessage>(&EditedMessage::fakeId)).desc(),
		limit(totalLimit)
	);
}

bool hasRevisions(ID userId, ID dialogId, ID messageId) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return !ArchiveDatabaseStorage().select(
			columns(column<EditedMessage>(&EditedMessage::messageId)),
			where(
				column<EditedMessage>(&EditedMessage::userId) == userId and
				column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
				column<EditedMessage>(&EditedMessage::messageId) == messageId
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if message has revisions: %1").arg(ex.what()));
		return false;
	}
}

bool addDeletedMessage(const DeletedMessage &message) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().begin_transaction();
		ArchiveDatabaseStorage().insert(message);
		ArchiveDatabaseStorage().commit();
		return true;
	} catch (std::exception &ex) {
		try {
			ArchiveDatabaseStorage().rollback();
		} catch (...) {
		}
		LOG(("Failed to save deleted message: %1").arg(ex.what()));
		return false;
	}
}

std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit, const std::string &searchQuery) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	if (searchQuery.empty()) {
		return ArchiveDatabaseStorage().get_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0)
			),
			order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
			limit(totalLimit)
		);
	}

	std::string escaped;
	escaped.reserve(searchQuery.size());
	for (const auto c : searchQuery) {
		if (c == '%' || c == '_' || c == '\\') {
			escaped += '\\';
		}
		escaped += c;
	}
	const auto pattern = "%" + escaped + "%";
	return ArchiveDatabaseStorage().get_all<DeletedMessage>(
		where(
			column<DeletedMessage>(&DeletedMessage::userId) == userId and
			column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
			(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0) and
			like(column<DeletedMessage>(&DeletedMessage::text), pattern, "\\")
		),
		order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
		limit(totalLimit)
	);
}

bool hasDeletedMessages(ID userId, ID dialogId, ID topicId) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return !ArchiveDatabaseStorage().select(
			columns(column<DeletedMessage>(&DeletedMessage::dialogId)),
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if dialog has deleted message: %1").arg(ex.what()));
		return false;
	}
}

std::vector<std::string> removeDeletedMessage(ID userId, ID dialogId, ID messageId) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		const auto paths = ArchiveDatabaseStorage().select(
			column<DeletedMessage>(&DeletedMessage::mediaPath),
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				column<DeletedMessage>(&DeletedMessage::messageId) == messageId));
		ArchiveDatabaseStorage().remove_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				column<DeletedMessage>(&DeletedMessage::messageId) == messageId
			)
		);
		return paths;
	} catch (std::exception &ex) {
		LOG(("Failed to remove deleted message: %1").arg(ex.what()));
		return {};
	}
}

std::vector<std::string> clearDeletedMessages(ID userId, ID dialogId, ID topicId) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		const auto paths = ArchiveDatabaseStorage().select(
			column<DeletedMessage>(&DeletedMessage::mediaPath),
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)));
		ArchiveDatabaseStorage().remove_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			)
		);
		return paths;
	} catch (std::exception &ex) {
		LOG(("Failed to clear deleted messages: %1").arg(ex.what()));
		return {};
	}
}

template<typename T>
std::vector<T> getAllT() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return ArchiveDatabaseStorage().get_all<T>();
	} catch (std::exception &ex) {
		LOG(("Failed to get all: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getAllRegexFilters() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	return getAllT<RegexFilter>();
}

std::vector<RegexFilterGlobalExclusion> getAllFiltersExclusions() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	return getAllT<RegexFilterGlobalExclusion>();
}

std::vector<RegexFilter> getExcludedByDialogId(ID dialogId) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return ArchiveDatabaseStorage().get_all<RegexFilter>(
			where(in(&RegexFilter::id,
					 ArchiveDatabaseStorage().select(columns(&RegexFilterGlobalExclusion::filterId),
									where(is_equal(&RegexFilterGlobalExclusion::dialogId, dialogId))
					 )
			))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get excluded by dialog id: %1").arg(ex.what()));
		return {};
	}
}

int getCount() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return ArchiveDatabaseStorage().count<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to get count: %1").arg(ex.what()));
		return 0;
	}
}

RegexFilter getById(std::vector<char> id) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return ArchiveDatabaseStorage().get<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == std::move(id))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by id: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getShared() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return ArchiveDatabaseStorage().get_all<RegexFilter>(
			where(is_null(column<RegexFilter>(&RegexFilter::dialogId)))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get shared filters: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getByDialogId(ID dialogId) {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return ArchiveDatabaseStorage().get_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::dialogId) == dialogId)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by dialog id: %1").arg(ex.what()));
		return {};
	}
}

void addRegexFilter(const RegexFilter &filter) {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().begin_transaction();
		ArchiveDatabaseStorage().replace(filter); // we're using replace as we set std::vector<char> as primary key
		ArchiveDatabaseStorage().commit();
	} catch (std::exception &ex) {
		try {
			ArchiveDatabaseStorage().rollback();
		} catch (...) {
		}
		LOG(("Failed to save regex filter for some reason: %1").arg(ex.what()));
	}
}

void addRegexExclusion(const RegexFilterGlobalExclusion &exclusion) {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().begin_transaction();
		ArchiveDatabaseStorage().insert(exclusion);
		ArchiveDatabaseStorage().commit();
	} catch (std::exception &ex) {
		try {
			ArchiveDatabaseStorage().rollback();
		} catch (...) {
		}
		LOG(("Failed to save regex filter exclusion for some reason: %1").arg(ex.what()));
	}
}

void updateRegexFilter(const RegexFilter &filter) {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().update_all(
			set(
				c(&RegexFilter::text) = filter.text,
				c(&RegexFilter::enabled) = filter.enabled,
				c(&RegexFilter::reversed) = filter.reversed,
				c(&RegexFilter::caseInsensitive) = filter.caseInsensitive,
				c(&RegexFilter::dialogId) = filter.dialogId
			),
			where(c(&RegexFilter::id) == filter.id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to update regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteFilter(const std::vector<char> &id) {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().remove_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteExclusionsByFilterId(const std::vector<char> &id) {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().remove_all<RegexFilterGlobalExclusion>(
			where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter exclusion by filter id for some reason: %1").arg(ex.what()));
	}
}

void deleteExclusion(ID dialogId, std::vector<char> filterId) {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().remove_all<RegexFilterGlobalExclusion>(
			where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == filterId and
				column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::dialogId) == dialogId
			)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter exclusion for some reason: %1").arg(ex.what()));
	}
}

void deleteAllFilters() {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().remove_all<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to delete all regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteAllExclusions() {
	if (!ArchiveReady.load()) return;
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		ArchiveDatabaseStorage().remove_all<RegexFilterGlobalExclusion>();
	} catch (std::exception &ex) {
		LOG(("Failed to delete all regex filter exclusions for some reason: %1").arg(ex.what()));
	}
}

bool hasFilters() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return !ArchiveDatabaseStorage().select(
			columns(column<RegexFilter>(&RegexFilter::id)),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

bool hasPerDialogFilters() {
	if (!ArchiveReady.load()) return {};
	const auto lock = std::lock_guard(ArchiveDatabaseMutex);
	try {
		return
			!ArchiveDatabaseStorage().select(
				columns(column<RegexFilter>(&RegexFilter::id)),
				where(is_not_null(column<RegexFilter>(&RegexFilter::dialogId))),
				limit(1)
			).empty() ||
			!ArchiveDatabaseStorage().select(
				columns(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::fakeId)),
				limit(1)
			).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

}
