// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/features/translator/html_parser.h"

namespace Ax::Translator::Html {

// yandex messes up HTML badly, so formatting removed for now

QString entitiesToHtml(const TextWithEntities &text) {
	return text.text;
}

TextWithEntities htmlToEntities(const QString &text) {
	TextWithEntities result = {.text = text};

	// links parsing doesn't work actually as it's not even accounted in ParseEntities
	// todo: find a way to parse links
	TextUtilities::ApplyServerCleaning(result);
	TextUtilities::ParseEntities(result, TextParseLinks | TextParseMentions | TextParseHashtags | TextParseBotCommands);
	TextUtilities::Trim(result);

	return result;
}

}