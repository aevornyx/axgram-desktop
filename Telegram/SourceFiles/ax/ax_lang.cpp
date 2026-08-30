// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#include "ax/ax_lang.h"

#include "qjsondocument.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_instance.h"
#include "storage/localstorage.h"

#include <QDir>
#include <QFile>

// hard-coded languages
std::map<QString, QString> langMapping = {
	{"pt-br", "pt"},
	{"zh-hans-beta", "zh-hans"},
	{"zh-hant-beta", "zh-hant"},
	{"zh-hans-raw", "zh-hans"},
	{"zh-hant-raw", "zh-hant"},
};

constexpr auto postfixes = {
	"zero",
	"one",
	"two",
	"few",
	"many",
	"other"
};

AxLanguage *AxLanguage::instance = nullptr;

AxLanguage::AxLanguage() = default;

void AxLanguage::init() {
	if (!instance) instance = new AxLanguage;
	instance->loadCachedLanguage();
}

AxLanguage *AxLanguage::currentInstance() {
	return instance;
}

QString AxLanguage::getCacheDir() const {
	return cWorkingDir() + u"tdata/ax/languages/"_q;
}

QString AxLanguage::getCachePath(const QString &langId) const {
	return getCacheDir() + langId + u".json"_q;
}

void AxLanguage::loadCachedLanguage() {
	const auto langPackId = Lang::GetInstance().id();
	const auto langPackBaseId = Lang::GetInstance().baseId();
	auto finalLangPackId = langMapping.contains(langPackId) ? langMapping[langPackId] : langPackId;

	if (finalLangPackId.isEmpty()) {
		finalLangPackId = langPackBaseId;
	}
	if (finalLangPackId.isEmpty()) {
		return;
	}

	// Порядок: файл в tdata (если пользователь положил свой), затем перевод,
	// вшитый в бинарник. Второе важно: раньше переводы приносила загрузка
	// с чужого CDN, и без неё у скачавшего сборку перевода бы не было вовсе.
	auto path = getCachePath(finalLangPackId);
	if (!QFile::exists(path)) {
		path = getCachePath(langPackBaseId);
	}
	if (!QFile::exists(path)) {
		path = qsl(":/gui/langs/ax/%1.json").arg(finalLangPackId);
	}
	if (!QFile::exists(path)) {
		path = qsl(":/gui/langs/ax/%1.json").arg(langPackBaseId);
	}
	if (!QFile::exists(path)) {
		return;
	}
	QFile file(path);

	if (file.open(QIODevice::ReadOnly)) {
		const auto data = file.readAll();
		file.close();

		QJsonParseError error{};
		const auto doc = QJsonDocument::fromJson(data, &error);
		if (error.error == QJsonParseError::NoError) {
			LOG(("AxGram language loaded: %1 (%2)").arg(finalLangPackId).arg(path));
			applyLanguageJson(doc);
		}
	}
}

void AxLanguage::saveCachedLanguage(const QByteArray &json, const QString &langId) {
	const auto cacheDir = getCacheDir();
	QDir().mkpath(cacheDir);

	const auto cachePath = getCachePath(langId);
	QFile file(cachePath);
	if (file.open(QIODevice::WriteOnly)) {
		file.write(json);
		file.close();
		LOG(("Cached AxGram language: %1").arg(langId));
	}
}

void AxLanguage::fetchLanguage(const QString &id, const QString &baseId) {
	auto finalLangPackId = langMapping.contains(id) ? langMapping[id] : id;
	_currentLangId = finalLangPackId.isEmpty() ? baseId : finalLangPackId;

	// AxGram: no remote lang fetch from AxGram CDN; embedded English defaults are used.
	return;

	if (Core::App().settings().proxy().isEnabled()) {
		const auto proxy = Core::App().settings().proxy().selected();
		if (proxy.type == MTP::ProxyData::Type::Socks5 || proxy.type == MTP::ProxyData::Type::Http) {
			const auto networkProxy = ToNetworkProxy(ToDirectIpProxy(Core::App().settings().proxy().selected()));
			networkManager.setProxy(networkProxy);
		}
	}

	// AxGram: адрес CDN переводов удалён. После переименования он указывал
	// на организацию github.com/AxGram, которая принадлежит постороннему —
	// то есть чужой человек мог бы подменять строки интерфейса. Загрузка
	// отключена выше; переводы берутся из tdata/ax/languages/<язык>.json.
	QUrl url;
	_chkReply = networkManager.get(QNetworkRequest(url));
	connect(_chkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(fetchError(QNetworkReply::NetworkError)));
	connect(_chkReply, SIGNAL(finished()), this, SLOT(fetchFinished()));
}

void AxLanguage::fetchFinished() {
	if (!_chkReply) return;

	QString langPackBaseId = Lang::GetInstance().baseId();
	QString langPackId = Lang::GetInstance().id();
	auto statusCode = _chkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	if (statusCode == 404 && !langPackId.isEmpty() && !langPackBaseId.isEmpty() && !needFallback) {
		LOG(("AxGram Language not found! Fallback to main language: %1...").arg(langPackBaseId));
		needFallback = true;
		_chkReply->disconnect();
		fetchLanguage("", langPackBaseId);
	} else {
		const auto result = _chkReply->readAll().trimmed();
		QJsonParseError error{};
		const auto doc = QJsonDocument::fromJson(result, &error);
		if (error.error == QJsonParseError::NoError) {
			saveCachedLanguage(result, _currentLangId);
			applyLanguageJson(doc);
		} else {
			LOG(("Incorrect language JSON File."));
		}

		_chkReply = nullptr;
	}
}

void AxLanguage::fetchError(QNetworkReply::NetworkError e) {
	LOG(("Network error: %1").arg(e));

	if (e == QNetworkReply::NetworkError::ContentNotFoundError) {
		const auto baseId = Lang::GetInstance().baseId();
		const auto id = Lang::GetInstance().id();

		if (!id.isEmpty() && !baseId.isEmpty() && !needFallback) {
			LOG(("AxGram Language not found! Fallback to main language: %1...").arg(baseId));
			needFallback = true;
			_chkReply->disconnect();
			fetchLanguage("", baseId);
		} else {
			LOG(("AxGram Language not found!"));
			_chkReply = nullptr;
		}
	}
}

void AxLanguage::applyLanguageJson(QJsonDocument doc) {
	const auto json = doc.object();
	for (const QString &brokenKey : json.keys()) {
		auto key = qsl("ax_") + brokenKey;
		auto val = json.value(brokenKey).toString().replace(qsl("&amp;"), qsl("&"));

		if (key.endsWith("_Android")) {
			continue;
		}

		for (const auto &postfix : postfixes) {
			if (key.endsWith(qsl("_") + postfix)) {
				key = key.replace(qsl("_") + postfix, qsl("#") + postfix);
				break;
			}
		}

		if (key.endsWith("_PC")) {
			key = key.replace("_PC", "");
		}

		if (val.contains(qsl("%1$d")) && !val.contains(qsl("%2$d"))) {
			val = val.replace(qsl("%1$d"), qsl("{count}"));
		} else if (val.contains(qsl("%1$d")) && val.contains(qsl("%2$d"))) {
			val = val.replace(qsl("%1$d"), qsl("{count1}")).replace(qsl("%2$d"), qsl("{count2}"));
		} else if (val.contains(qsl("%1$s")) && !val.contains(qsl("%2$s"))) {
			val = val.replace(qsl("%1$s"), qsl("{item}"));
		} else if (val.contains(qsl("%1$s")) && val.contains(qsl("%2$s"))) {
			val = val.replace(qsl("%1$s"), qsl("{item1}")).replace(qsl("%2$s"), qsl("{item2}"));
		}

		Lang::GetInstance().resetValue(key.toUtf8());
		Lang::GetInstance().applyValue(key.toUtf8(), val.toUtf8());
	}
	Lang::GetInstance().updatePluralRules();
}
