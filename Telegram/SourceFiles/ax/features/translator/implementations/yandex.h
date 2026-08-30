// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include "ax/features/translator/implementations/base.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>

namespace Ax::Translator {

class YandexTranslator final : public MultiThreadTranslator
{
	Q_OBJECT

public:
	static YandexTranslator &instance();

	[[nodiscard]] QSet<QString> supportedLanguages() const override;

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const MultiThreadArgs &args
	) override;

private:
	explicit YandexTranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;
	QString _uuid;
};

}