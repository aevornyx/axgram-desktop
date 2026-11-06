#include "ax/data/archive_database.h"

#include "ax/libs/sqlite/sqlite3.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace AxArchive {
namespace {

using Connection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

std::string PathString(const std::filesystem::path &path) {
	const auto encoded = path.u8string();
	return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

#ifdef _WIN32
bool HashFile(HANDLE file, std::array<unsigned char, 32> &hash) {
	auto context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
		EVP_MD_CTX_new(), EVP_MD_CTX_free);
	if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
		return false;
	}
	auto buffer = std::array<unsigned char, 65536>();
	auto count = DWORD();
	while (true) {
		if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) return false;
		if (!count) break;
		if (EVP_DigestUpdate(context.get(), buffer.data(), count) != 1) return false;
	}
	auto size = static_cast<unsigned int>(0);
	return EVP_DigestFinal_ex(context.get(), hash.data(), &size) == 1
		&& size == hash.size();
}

bool HashPath(const std::filesystem::path &path, std::array<unsigned char, 32> &hash) {
	const auto file = CreateFileW(path.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	const auto result = HashFile(file, hash);
	CloseHandle(file);
	return result;
}
#endif

bool Execute(sqlite3 *db, const char *sql) {
	return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Scalar(sqlite3 *db, const std::string &sql, std::string &value) {
	auto raw = static_cast<sqlite3_stmt*>(nullptr);
	const auto prepared = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);
	auto statement = Statement(raw, sqlite3_finalize);
	if (prepared != SQLITE_OK || sqlite3_step(raw) != SQLITE_ROW) {
		return false;
	}
	const auto text = sqlite3_column_text(raw, 0);
	value.assign(text ? reinterpret_cast<const char*>(text) : "",
		text ? sqlite3_column_bytes(raw, 0) : 0);
	return sqlite3_step(raw) == SQLITE_DONE;
}

std::string Quote(const std::string &name) {
	auto result = std::string("\"");
	for (const auto ch : name) {
		result += ch;
		if (ch == '"') result += '"';
	}
	return result + '"';
}

bool Verify(sqlite3 *db) {
	auto value = std::string();
	if (!Scalar(db, "PRAGMA protected.integrity_check", value) || value != "ok") {
		return false;
	}
	if (!Scalar(db, "SELECT count(*) FROM (SELECT type,name,tbl_name,sql FROM main.sqlite_master EXCEPT SELECT type,name,tbl_name,sql FROM protected.sqlite_master)", value)
		|| value != "0"
		|| !Scalar(db, "SELECT count(*) FROM (SELECT type,name,tbl_name,sql FROM protected.sqlite_master EXCEPT SELECT type,name,tbl_name,sql FROM main.sqlite_master)", value)
		|| value != "0") {
		return false;
	}
	auto raw = static_cast<sqlite3_stmt*>(nullptr);
	const auto prepared = sqlite3_prepare_v2(db,
		"SELECT name FROM main.sqlite_master WHERE type='table'", -1, &raw, nullptr);
	auto tables = Statement(raw, sqlite3_finalize);
	if (prepared != SQLITE_OK) return false;
	auto step = sqlite3_step(raw);
	while (step == SQLITE_ROW) {
		const auto text = sqlite3_column_text(raw, 0);
		if (!text) return false;
		const auto name = Quote(reinterpret_cast<const char*>(text));
		auto original = std::string();
		auto exported = std::string();
		if (!Scalar(db, "SELECT count(*) FROM main." + name, original)
			|| !Scalar(db, "SELECT count(*) FROM protected." + name, exported)
			|| original != exported) {
			return false;
		}
		step = sqlite3_step(raw);
	}
	return step == SQLITE_DONE;
}

}

ExportError ExportDatabase(
		const std::filesystem::path &source,
		const std::filesystem::path &destination,
		const std::array<unsigned char, 32> &key,
		std::array<unsigned char, 32> *sourceHash) {
#ifndef _WIN32
	return ExportError::Unsupported;
#else
	auto error = std::error_code();
	if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(source, error))
		|| error
		|| std::filesystem::exists(destination, error)
		|| error) {
		return ExportError::InvalidPath;
	}
	for (const auto suffix : { L"-journal", L"-wal", L"-shm" }) {
		auto sidecar = destination;
		sidecar += suffix;
		if (std::filesystem::exists(sidecar, error) || error) {
			return ExportError::InvalidPath;
		}
	}
	{
		auto input = std::ifstream(source, std::ios::binary);
		auto header = std::array<char, 16>();
		if (!input.read(header.data(), header.size())
			|| std::string(header.data(), header.size()) != std::string("SQLite format 3\0", 16)) {
			return ExportError::Source;
		}
	}
	const auto output = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (output == INVALID_HANDLE_VALUE) return ExportError::Destination;
	CloseHandle(output);
	auto result = ExportError::Source;
	{
		auto raw = static_cast<sqlite3*>(nullptr);
		const auto opened = sqlite3_open_v2(PathString(source).c_str(), &raw,
			SQLITE_OPEN_READWRITE, nullptr);
		auto db = Connection(raw, sqlite3_close);
		if (opened == SQLITE_OK) {
			auto version = std::string();
			auto application = std::string();
			auto cipher = std::string();
			if (!Scalar(raw, "PRAGMA cipher_version", cipher) || cipher.empty()) {
				result = ExportError::Cipher;
			} else if (sourceHash
				&& (!Scalar(raw, "PRAGMA journal_mode=DELETE", cipher)
					|| cipher != "delete"
					|| !Execute(raw, "PRAGMA locking_mode=EXCLUSIVE"))) {
				result = ExportError::Busy;
			} else if (!Execute(raw, sourceHash ? "BEGIN EXCLUSIVE" : "BEGIN IMMEDIATE")) {
				result = (sqlite3_errcode(raw) == SQLITE_BUSY || sqlite3_errcode(raw) == SQLITE_LOCKED)
					? ExportError::Busy : ExportError::Source;
			} else if (Scalar(raw, "PRAGMA user_version", version)
				&& Scalar(raw, "PRAGMA application_id", application)) {
				auto attachRaw = static_cast<sqlite3_stmt*>(nullptr);
				const auto prepared = sqlite3_prepare_v2(raw,
					"ATTACH DATABASE ?1 AS protected KEY ?2", -1, &attachRaw, nullptr);
				auto attach = Statement(attachRaw, sqlite3_finalize);
				const auto path = PathString(destination);
				if (prepared == SQLITE_OK
					&& sqlite3_bind_text(attachRaw, 1, path.c_str(), int(path.size()), SQLITE_TRANSIENT) == SQLITE_OK
					&& sqlite3_bind_blob(attachRaw, 2, key.data(), int(key.size()), SQLITE_TRANSIENT) == SQLITE_OK
					&& sqlite3_step(attachRaw) == SQLITE_DONE) {
					attach.reset();
					auto ignored = std::string();
					result = ExportError::Verification;
					if (Scalar(raw, "SELECT sqlcipher_export('protected')", ignored)
						&& Execute(raw, ("PRAGMA protected.user_version=" + version).c_str())
						&& Execute(raw, ("PRAGMA protected.application_id=" + application).c_str())
						&& Verify(raw)
						&& Execute(raw, "COMMIT")
						&& Execute(raw, "DETACH DATABASE protected")) {
						result = (!sourceHash || HashPath(source, *sourceHash))
							? ExportError::None : ExportError::Source;
					}
				} else {
					result = ExportError::Destination;
				}
			}
		}
	}
	if (result != ExportError::None) {
		DeleteFileW(destination.c_str());
		for (const auto suffix : { L"-journal", L"-wal", L"-shm" }) {
			auto sidecar = destination;
			sidecar += suffix;
			DeleteFileW(sidecar.c_str());
		}
	}
	return result;
#endif
}

ExportError ExportEncryptedDatabase(
		const std::filesystem::path &source,
		const std::filesystem::path &destination,
		const std::array<unsigned char, 32> &key) {
	return ExportDatabase(source, destination, key, nullptr);
}

DatabaseKind InspectDatabase(const std::filesystem::path &path) {
	auto error = std::error_code();
	const auto status = std::filesystem::symlink_status(path, error);
	if (status.type() == std::filesystem::file_type::not_found) {
		for (const auto suffix : { L"-journal", L"-wal", L"-shm" }) {
			auto sidecar = path;
			sidecar += suffix;
			const auto related = std::filesystem::symlink_status(sidecar, error);
			if (related.type() != std::filesystem::file_type::not_found) return DatabaseKind::Invalid;
		}
		auto parent = path.parent_path();
		if (parent.empty()) parent = ".";
		if (std::filesystem::exists(parent, error)) {
			const auto prefix = PathString(path.filename()) + ".encrypting-";
			const auto files = std::filesystem::directory_iterator(parent, error);
			if (error) return DatabaseKind::Invalid;
			for (const auto &file : files) {
				if (PathString(file.path().filename()).find(prefix) == 0) return DatabaseKind::Invalid;
			}
		}
		return error ? DatabaseKind::Invalid : DatabaseKind::Missing;
	}
	if (error || !std::filesystem::is_regular_file(status)) return DatabaseKind::Invalid;
	auto header = std::array<char, 16>();
	auto file = std::ifstream(path, std::ios::binary);
	if (!file.read(header.data(), header.size())) return DatabaseKind::Invalid;
	return (std::string(header.data(), header.size()) == std::string("SQLite format 3\0", 16))
		? DatabaseKind::Plain : DatabaseKind::Protected;
}

ProtectError ProtectDatabase(
		const std::filesystem::path &path,
		const std::array<unsigned char, 32> &key) {
#ifndef _WIN32
	return ProtectError::Unsupported;
#else
	const auto kind = InspectDatabase(path);
	if (kind == DatabaseKind::Missing) return ProtectError::Missing;
	if (kind == DatabaseKind::Invalid) return ProtectError::Invalid;
	if (kind == DatabaseKind::Protected) {
		auto raw = static_cast<sqlite3*>(nullptr);
		const auto opened = sqlite3_open_v2(PathString(path).c_str(), &raw,
			SQLITE_OPEN_READONLY, nullptr);
		auto db = Connection(raw, sqlite3_close);
		auto result = std::string();
		return (opened == SQLITE_OK
			&& sqlite3_key(raw, key.data(), int(key.size())) == SQLITE_OK
			&& Scalar(raw, "PRAGMA integrity_check", result)
			&& result == "ok") ? ProtectError::None : ProtectError::Cipher;
	}
	auto nonce = std::array<unsigned char, 16>();
	if (RAND_bytes(nonce.data(), int(nonce.size())) != 1) return ProtectError::Cipher;
	const auto alphabet = "0123456789abcdef";
	auto suffix = std::string(".encrypting-");
	for (const auto byte : nonce) {
		suffix += alphabet[byte >> 4];
		suffix += alphabet[byte & 15];
	}
	auto temporary = path;
	temporary += suffix;
	auto hash = std::array<unsigned char, 32>();
	const auto exported = ExportDatabase(path, temporary, key, &hash);
	if (exported != ExportError::None) {
		return (exported == ExportError::Busy) ? ProtectError::Busy : ProtectError::Io;
	}
	if (ProtectDatabase(temporary, key) != ProtectError::None) {
		DeleteFileW(temporary.c_str());
		return ProtectError::Cipher;
	}
	auto result = ProtectError::Io;
	const auto source = CreateFileW(path.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (source != INVALID_HANDLE_VALUE) {
		auto current = std::array<unsigned char, 32>();
		auto info = BY_HANDLE_FILE_INFORMATION();
		if (GetFileInformationByHandle(source, &info)
			&& !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			&& HashFile(source, current)) {
			if (current != hash) {
				result = ProtectError::Changed;
			} else if (ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr)) {
				result = ProtectError::None;
			}
		}
		CloseHandle(source);
	} else if (GetLastError() == ERROR_SHARING_VIOLATION) {
		result = ProtectError::Busy;
	}
	if (result != ProtectError::None) DeleteFileW(temporary.c_str());
	return result;
#endif
}

}
