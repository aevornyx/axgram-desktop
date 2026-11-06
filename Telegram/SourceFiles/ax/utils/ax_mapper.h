// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

class DocumentData;

namespace AxMapper {

template<typename MTPObject>
[[nodiscard]] MTPObject deserializeObject(std::vector<char> serialized);

template<typename MTPObject>
[[nodiscard]] std::vector<char> serializeObject(MTPObject object);

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item);
[[nodiscard]] MTPVector<MTPMessageEntity> deserializeTextWithEntities(std::vector<char> serialized);
[[nodiscard]] std::vector<char> serializeDocumentAttributes(
	not_null<DocumentData*> document);
[[nodiscard]] MTPVector<MTPDocumentAttribute> deserializeDocumentAttributes(
	std::vector<char> serialized);
int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item);

} // namespace AxMapper
