#include "ax/data/archive_key_store.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#endif

namespace AxArchive {
namespace {

constexpr auto kHeader = "AXAK1";
constexpr auto kHeaderSize = 5;
constexpr char kBinding[] = "AxGram/archive/device-key/v1";
constexpr auto kMaximumFile = 65536;

#ifdef _WIN32
bool Transform(const unsigned char *input, size_t size, bool seal, std::vector<unsigned char> &output) {
	auto data = DATA_BLOB{ DWORD(size), const_cast<BYTE*>(input) };
	auto entropy = DATA_BLOB{ DWORD(sizeof(kBinding) - 1),
		reinterpret_cast<BYTE*>(const_cast<char*>(kBinding)) };
	auto result = DATA_BLOB{};
	const auto okay = seal
		? CryptProtectData(&data, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &result)
		: CryptUnprotectData(&data, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &result);
	if (!okay) return false;
	const auto bounded = result.cbData <= kMaximumFile;
	if (bounded) output.assign(result.pbData, result.pbData + result.cbData);
	SecureZeroMemory(result.pbData, result.cbData);
	LocalFree(result.pbData);
	return bounded;
}

bool Publish(const std::filesystem::path &path, const std::vector<unsigned char> &ciphertext) {
	std::array<unsigned char, 16> random{};
	if (RAND_bytes(random.data(), int(random.size())) != 1) return false;
	const auto alphabet = "0123456789abcdef";
	auto suffix = std::string();
	for (const auto value : random) {
		suffix += alphabet[value >> 4];
		suffix += alphabet[value & 15];
	}
	auto temporary = path;
	temporary += std::filesystem::path(".pending-" + suffix);
	const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	auto written = DWORD();
	const auto header = WriteFile(file, kHeader, kHeaderSize, &written, nullptr)
		&& written == kHeaderSize;
	const auto body = header && WriteFile(file, ciphertext.data(), DWORD(ciphertext.size()), &written, nullptr)
		&& written == ciphertext.size();
	const auto flushed = body && FlushFileBuffers(file);
	CloseHandle(file);
	const auto moved = flushed && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH);
	if (!moved) DeleteFileW(temporary.c_str());
	return moved;
}
#endif

}

DeviceKey::~DeviceKey() {
	OPENSSL_cleanse(_bytes.data(), _bytes.size());
}

std::unique_ptr<DeviceKey> LoadDeviceKey(
		const std::filesystem::path &path, bool allowCreate, KeyError &error) {
	error = KeyError::None;
#ifndef _WIN32
	error = KeyError::Unsupported;
	return nullptr;
#else
	try {
		if (std::filesystem::exists(path)) {
			if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path)) {
				error = KeyError::Invalid;
				return nullptr;
			}
			const auto size = std::filesystem::file_size(path);
			if (size <= kHeaderSize || size > kMaximumFile) {
				error = KeyError::Invalid;
				return nullptr;
			}
			auto file = std::ifstream(path, std::ios::binary);
			auto bytes = std::vector<unsigned char>(size);
			if (!file.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(size))
				|| file.peek() != std::ifstream::traits_type::eof()
				|| std::string(bytes.begin(), bytes.begin() + kHeaderSize) != kHeader) {
				error = KeyError::Invalid;
				return nullptr;
			}
			auto plain = std::vector<unsigned char>();
			const auto okay = Transform(bytes.data() + kHeaderSize, bytes.size() - kHeaderSize, false, plain);
			if (!okay || plain.size() != 32) {
				OPENSSL_cleanse(plain.data(), plain.size());
				error = KeyError::Invalid;
				return nullptr;
			}
			auto result = std::make_unique<DeviceKey>();
			std::copy(plain.begin(), plain.end(), result->_bytes.begin());
			OPENSSL_cleanse(plain.data(), plain.size());
			return result;
		}
		if (!allowCreate) {
			error = KeyError::Missing;
			return nullptr;
		}
		std::filesystem::create_directories(path.parent_path());
		auto result = std::make_unique<DeviceKey>();
		if (RAND_bytes(result->_bytes.data(), int(result->_bytes.size())) != 1) {
			error = KeyError::Io;
			return nullptr;
		}
		auto ciphertext = std::vector<unsigned char>();
		if (!Transform(result->_bytes.data(), result->_bytes.size(), true, ciphertext)) {
			error = KeyError::Io;
			return nullptr;
		}
		if (!Publish(path, ciphertext) && !std::filesystem::exists(path)) {
			error = KeyError::Io;
			return nullptr;
		}
		return LoadDeviceKey(path, false, error);
	} catch (...) {
		error = KeyError::Io;
		return nullptr;
	}
#endif
}

}
