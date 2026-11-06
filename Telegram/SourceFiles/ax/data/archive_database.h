#pragma once

#include <array>
#include <filesystem>

namespace AxArchive {

enum class ExportError { None, InvalidPath, Source, Busy, Destination, Cipher, Verification, Unsupported };
enum class ProtectError { None, Missing, Invalid, Busy, Cipher, Changed, Io, Unsupported };
enum class DatabaseKind { Missing, Plain, Protected, Invalid };

[[nodiscard]] DatabaseKind InspectDatabase(const std::filesystem::path &path);

[[nodiscard]] ExportError ExportEncryptedDatabase(
	const std::filesystem::path &source,
	const std::filesystem::path &destination,
	const std::array<unsigned char, 32> &key);

[[nodiscard]] ProtectError ProtectDatabase(
	const std::filesystem::path &path,
	const std::array<unsigned char, 32> &key);

}
