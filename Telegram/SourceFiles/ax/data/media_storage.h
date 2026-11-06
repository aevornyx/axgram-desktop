#pragma once

#include "ax/data/entities.h"
#include "ax/data/media_file.h"

class DocumentData;
class HistoryItem;
class PhotoData;

namespace Main {
class Session;
} // namespace Main

namespace AxMedia {

struct Restored {
	DocumentData *document = nullptr;
	PhotoData *photo = nullptr;
};

void Capture(not_null<HistoryItem*> item, AxMessageBase &message);
[[nodiscard]] Restored Restore(
	not_null<Main::Session*> session,
	const AxMessageBase &message);
void Remove(ID owner, const std::string &path);
void Flush();
void SetLimitMb(int value);
[[nodiscard]] State Status(const AxMessageBase &message);
[[nodiscard]] QString StatusText(const AxMessageBase &message);

} // namespace AxMedia
