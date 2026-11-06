#pragma once

#include <array>
#include <filesystem>
#include <memory>

namespace AxArchive {

enum class KeyError { None, Missing, Invalid, Io, Unsupported };

class DeviceKey final {
public:
	DeviceKey() = default;
	DeviceKey(const DeviceKey&) = delete;
	DeviceKey &operator=(const DeviceKey&) = delete;
	~DeviceKey();
	[[nodiscard]] const std::array<unsigned char, 32> &bytes() const { return _bytes; }
private:
	friend std::unique_ptr<DeviceKey> LoadDeviceKey(const std::filesystem::path&, bool, KeyError&);
	std::array<unsigned char, 32> _bytes = {};
};

[[nodiscard]] std::unique_ptr<DeviceKey> LoadDeviceKey(
	const std::filesystem::path &path, bool allowCreate, KeyError &error);

}
