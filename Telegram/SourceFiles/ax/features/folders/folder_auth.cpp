#include "ax/features/folders/folder_auth.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QSet>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <algorithm>
#include <cmath>

namespace AxFolders {
namespace {

bool Matches(const QString &value, const QString &pattern) {
	return QRegularExpression(pattern).match(value).hasMatch();
}

bool Uuid(const QString &value) {
	return value == QUuid(value).toString(QUuid::WithoutBraces);
}

bool Secret(const QString &value, int minimum = 32, int maximum = 128) {
	return value.size() >= minimum && value.size() <= maximum
		&& Matches(value, QStringLiteral("\\A[A-Za-z0-9_-]+\\z"));
}

std::optional<QMap<QString, QString>> Query(const QString &encoded) {
	if (encoded.isEmpty() || encoded.size() > 8192
		|| Matches(encoded, QStringLiteral("%(?![0-9A-Fa-f]{2})"))) {
		return std::nullopt;
	}
	auto result = QMap<QString, QString>();
	for (auto field : encoded.toUtf8().split('&')) {
		const auto at = field.indexOf('=');
		if (at < 0) return std::nullopt;
		const auto decode = [](QByteArray bytes) -> std::optional<QString> {
			bytes.replace('+', ' ');
			const auto decoded = QByteArray::fromPercentEncoding(bytes);
			const auto text = QString::fromUtf8(decoded);
			return text.toUtf8() == decoded ? std::optional<QString>(text) : std::nullopt;
		};
		const auto key = decode(field.left(at));
		const auto value = decode(field.mid(at + 1));
		if (!key || !value || result.contains(*key)) return std::nullopt;
		result.insert(*key, *value);
	}
	return result;
}

bool Ascii(const QString &value) {
	return std::all_of(value.begin(), value.end(), [](QChar c) {
		return c.unicode() >= 33 && c.unicode() <= 126;
	});
}

bool Duration(const QJsonValue &value, int maximum) {
	const auto number = value.toDouble(-1);
	return value.isDouble() && number >= 1 && number <= maximum
		&& std::floor(number) == number;
}

}

bool ValidConfirmation(const QString &url) {
	if ((!url.startsWith(QLatin1String("tg://oauth?"))
		&& !url.startsWith(QLatin1String("tg://resolve?")))
		|| url.size() > 8192 || url.contains('#') || !Ascii(url)) {
		return false;
	}
	const auto parsed = QUrl(url, QUrl::StrictMode);
	const auto query = Query(parsed.query(QUrl::FullyEncoded));
	if (!parsed.isValid() || !parsed.path().isEmpty() || !query) {
		return false;
	}
	const auto direct = parsed.authority() == QLatin1String("oauth")
		&& query->size() == 1 && query->contains(QStringLiteral("token"));
	const auto resolved = parsed.authority() == QLatin1String("resolve")
		&& query->size() == 2
		&& query->value(QStringLiteral("domain")) == QLatin1String("oauth")
		&& query->contains(QStringLiteral("startapp"));
	if (!direct && !resolved) {
		return false;
	}
	const auto token = query->value(direct ? QStringLiteral("token") : QStringLiteral("startapp"));
	return token.size() >= 16 && token.size() <= 4096 && Ascii(token);
}

std::optional<LoginFlow> ParseLoginFlow(
		const QJsonObject &object,
		const QUrl &endpoint,
		qint64 account) {
	const auto id = object.value(QStringLiteral("requestId")).toString();
	const auto token = object.value(QStringLiteral("pollToken")).toString();
	const auto link = object.value(QStringLiteral("confirmationUrl")).toString();
	const auto authorize = QUrl(object.value(QStringLiteral("authorizeUrl")).toString(), QUrl::StrictMode);
	const auto parameters = Query(authorize.query(QUrl::FullyEncoded));
	if (account <= 0 || object.size() != 6 || !Uuid(id) || !Secret(token)
		|| !ValidConfirmation(link) || !Duration(object.value(QStringLiteral("expiresIn")), 300)
		|| !Duration(object.value(QStringLiteral("pollInterval")), 30)
		|| !authorize.isValid() || authorize.scheme() != QLatin1String("https")
		|| authorize.authority() != QLatin1String("oauth.telegram.org")
		|| authorize.path(QUrl::FullyEncoded) != QLatin1String("/auth")
		|| authorize.hasFragment() || !parameters || parameters->size() != 8) {
		return std::nullopt;
	}
	const auto get = [&](const char *key) { return parameters->value(QString::fromLatin1(key)); };
	if (!Matches(get("client_id"), QStringLiteral("\\A[1-9][0-9]{0,18}\\z"))
		|| get("response_type") != QLatin1String("code")
		|| get("scope") != QLatin1String("openid profile")
		|| get("code_challenge_method") != QLatin1String("S256")
		|| !Secret(get("state"), 43, 43) || !Secret(get("nonce"), 43, 43)
		|| !Secret(get("code_challenge"), 43, 43)) {
		return std::nullopt;
	}
	const auto callback = endpoint.toString(QUrl::FullyEncoded) + QStringLiteral("/v1/auth/callback");
	if (get("redirect_uri") != callback) {
#ifdef AXGRAM_FOLDER_SYNC_LAB
		if (endpoint.scheme() != QLatin1String("http")
			|| (endpoint.host() != QLatin1String("127.0.0.1")
				&& endpoint.host() != QLatin1String("::1"))) return std::nullopt;
#else
		return std::nullopt;
#endif
	}
	return LoginFlow{
		account, endpoint, id, token.toLatin1(), link, get("client_id"), get("state"),
		object.value(QStringLiteral("pollInterval")).toInt(),
		QDeadlineTimer(object.value(QStringLiteral("expiresIn")).toInt() * 1000),
	};
}

std::optional<LoginCredential> ParseLoginCredential(const QJsonObject &object, qint64 account) {
	const auto workspace = object.value(QStringLiteral("workspace")).toString();
	const auto device = object.value(QStringLiteral("device")).toString();
	const auto token = object.value(QStringLiteral("token")).toString();
	const auto encoded = object.value(QStringLiteral("encryptionKey")).toString();
	const auto key = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::Base64UrlEncoding);
	if (account <= 0 || object.size() != 6 || object.value(QStringLiteral("status")) != QLatin1String("ready")
		|| object.value(QStringLiteral("account")) != QString::number(account)
		|| !Uuid(workspace) || !Uuid(device) || !Secret(token) || !Secret(encoded, 43, 43)
		|| key.size() != 32 || key.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals) != encoded.toLatin1()) {
		return std::nullopt;
	}
	return LoginCredential{ account, workspace, device, token.toLatin1(), key };
}

std::optional<QJsonObject> NativeCallback(const LoginFlow &flow, const QString &url, LoginError &error) {
	error = LoginError::Invalid;
	const auto parsed = QUrl(url, QUrl::StrictMode);
	if (url.isEmpty() || url.size() > 8192 || !Ascii(url) || !parsed.isValid()
		|| parsed.hasFragment() || !Secret(flow.state, 43, 43)) return std::nullopt;
	const auto values = Query(parsed.query(QUrl::FullyEncoded));
	if (parsed.scheme() == QLatin1String("tgauth")
		&& parsed.authority(QUrl::FullyEncoded) == QLatin1String("confirm")
		&& parsed.path(QUrl::FullyEncoded).isEmpty()) {
		if (!values || values->size() != 1 || !values->contains(QStringLiteral("token"))) return std::nullopt;
		const auto token = values->value(QStringLiteral("token"));
		if (token.size() < 16 || token.size() > 4096 || !Ascii(token)) return std::nullopt;
		error = LoginError::None;
		return QJsonObject{ { QStringLiteral("state"), flow.state }, { QStringLiteral("code"), token } };
	}
	return std::nullopt;
}

FolderAuth::FolderAuth(FolderHttp *http, QObject *parent)
:	QObject(parent)
,	_http(http) {
}

bool FolderAuth::valid(const LoginFlow &flow) const {
	return _http && _http->valid() && flow.account > 0 && Uuid(flow.id)
		&& Secret(QString::fromLatin1(flow.pollToken))
		&& flow.endpoint == _http->base() && !flow.deadline.hasExpired();
}

quint64 FolderAuth::fail(LoginError error, Callback done) {
	QTimer::singleShot(0, this, [done = std::move(done), error] { done({ error }); });
	return 0;
}

quint64 FolderAuth::request(HttpRequest request, JsonCallback done) {
	if (!_http) {
		QTimer::singleShot(0, this, [done = std::move(done)] { done(LoginError::Network, {}); });
		return 0;
	}
	const auto guard = QPointer<FolderAuth>(this);
	return _http->send(std::move(request), [guard, done = std::move(done)](HttpResponse response) {
		if (!guard) return;
		if (response.error != HttpError::None) {
			const auto rejected = response.status >= 400 && response.status < 500
				&& response.status != 408 && response.status != 429;
			done(rejected ? LoginError::Rejected : LoginError::Network, {});
			return;
		}
		const auto text = QString::fromUtf8(response.body);
		const auto document = QJsonDocument::fromJson(response.body);
		if (response.status != 200 || !document.isObject() || text.toUtf8() != response.body) {
			done(LoginError::Invalid, {});
			return;
		}
		done(LoginError::None, document.object());
	});
}

quint64 FolderAuth::devices(const LoginCredential &credential, std::function<void(DevicesResult)> done) {
	const auto current = credential.device;
	return request({ "GET", QStringLiteral("/v1/devices"), credential.token },
		[done = std::move(done), current](LoginError error, QJsonObject object) {
			if (error != LoginError::None) { done({ error }); return; }
			const auto array = object.value(QStringLiteral("devices")).toArray();
			if (object.size() != 3 || object.value(QStringLiteral("currentDevice")) != current
				|| !object.value(QStringLiteral("canManage")).isBool()
				|| array.isEmpty() || array.size() > 32) { done({ LoginError::Invalid }); return; }
			auto result = SyncDevices{ current, object.value(QStringLiteral("canManage")).toBool() };
			auto seen = QSet<QString>();
			for (const auto entry : array) {
				const auto item = entry.toObject();
				const auto id = item.value(QStringLiteral("id")).toString();
				const auto label = item.value(QStringLiteral("label")).toString();
				const auto expires = item.value(QStringLiteral("expires")).toDouble(-1);
				if (item.size() != 3 || !Uuid(id) || seen.contains(id)
					|| label.trimmed().isEmpty() || label.size() > 80
					|| std::any_of(label.begin(), label.end(), [](QChar c) { return c.unicode() < 32 || c.unicode() == 127; })
					|| expires < 1 || expires > 4102444800. || std::floor(expires) != expires) {
					done({ LoginError::Invalid }); return;
				}
				seen.insert(id);
				result.devices.push_back({ id, label, qint64(expires) });
			}
			if (!seen.contains(current)) { done({ LoginError::Invalid }); return; }
			done({ LoginError::None, std::move(result) });
		});
}

quint64 FolderAuth::revoke(const LoginCredential &credential, const QString &device, Callback done) {
	if (!Uuid(device)) return fail(LoginError::Invalid, std::move(done));
	return request({ "POST", QStringLiteral("/v1/devices/") + device + QStringLiteral("/revoke"), credential.token, "{}" },
		[done = std::move(done)](LoginError error, QJsonObject object) {
			done({ error != LoginError::None ? error : object.size() == 1
				&& object.value(QStringLiteral("status")) == QLatin1String("revoked")
				? LoginError::None : LoginError::Invalid });
		});
}

quint64 FolderAuth::start(qint64 account, const QString &label, Callback done) {
	if (!_http || account <= 0 || label.trimmed().isEmpty() || label.size() > 80
		|| std::any_of(label.begin(), label.end(), [](QChar c) { return c.unicode() < 32; })) return fail(LoginError::Invalid, std::move(done));
	const auto endpoint = _http->base();
	const auto body = QJsonDocument(QJsonObject{
		{ QStringLiteral("account"), QString::number(account) }, { QStringLiteral("device"), label },
	}).toJson(QJsonDocument::Compact);
	return request({ "POST", QStringLiteral("/v1/auth/native/desktop/start"), {}, body },
		[=](LoginError error, QJsonObject object) {
			if (error != LoginError::None) { done({ error }); return; }
			const auto flow = ParseLoginFlow(object, endpoint, account);
			done({ flow ? LoginError::None : LoginError::Invalid, flow });
		});
}

quint64 FolderAuth::poll(const LoginFlow &flow, Callback done) {
	if (!valid(flow)) return fail(LoginError::Expired, std::move(done));
	return request({ "GET", QStringLiteral("/v1/auth/status/") + flow.id, flow.pollToken },
		[=](LoginError error, QJsonObject object) {
			if (error != LoginError::None) { done({ error }); return; }
			const auto status = object.value(QStringLiteral("status")).toString();
			if (status == QLatin1String("pending") && object.size() == 1) { done({}); return; }
			if (status == QLatin1String("error")) { done({ LoginError::Rejected }); return; }
			const auto credential = ParseLoginCredential(object, flow.account);
			done({ credential ? LoginError::None : LoginError::Invalid, {}, credential });
		});
}

quint64 FolderAuth::complete(const LoginFlow &flow, const QString &accepted, Callback done) {
	if (!valid(flow)) return fail(LoginError::Expired, std::move(done));
	auto error = LoginError::None;
	const auto body = NativeCallback(flow, accepted, error);
	if (!body) return fail(error, std::move(done));
	return request({ "POST", QStringLiteral("/v1/auth/native/complete/") + flow.id,
		flow.pollToken, QJsonDocument(*body).toJson(QJsonDocument::Compact) },
		[done = std::move(done)](LoginError error, QJsonObject object) {
			done({ error != LoginError::None ? error : object.size() == 1
				&& object.value(QStringLiteral("status")) == QLatin1String("confirmed")
				? LoginError::None : LoginError::Invalid });
		});
}

quint64 FolderAuth::acknowledge(const LoginFlow &flow, Callback done) {
	if (!valid(flow)) return fail(LoginError::Expired, std::move(done));
	return request({ "POST", QStringLiteral("/v1/auth/ack/") + flow.id, flow.pollToken },
		[done = std::move(done)](LoginError error, QJsonObject object) {
			done({ error != LoginError::None ? error : object.size() == 1
				&& object.value(QStringLiteral("status")) == QLatin1String("acknowledged")
				? LoginError::None : LoginError::Invalid });
		});
}

}
