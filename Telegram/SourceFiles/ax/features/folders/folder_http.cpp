#include "ax/features/folders/folder_http.h"

#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace AxFolders {
namespace {

constexpr auto MaxEnvelope = 1024 * 1024;
constexpr auto MaxJson = 16384;

bool ValidRequest(const HttpRequest &request) {
	const auto start = request.path == QLatin1String("/v1/auth/start")
		|| request.path == QLatin1String("/v1/auth/native/start")
		|| request.path == QLatin1String("/v1/auth/native/desktop/start");
	const auto snapshot = request.path == QLatin1String("/v1/snapshot");
	const auto devices = request.path == QLatin1String("/v1/devices");
	const auto revoke = QRegularExpression(QStringLiteral("\\A/v1/devices/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/revoke\\z")).match(request.path).hasMatch();
	const auto auth = QRegularExpression(
		QStringLiteral("\\A/v1/auth/(status|ack|native/complete)/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\z"));
	if (!start && !snapshot && !devices && !revoke && !auth.match(request.path).hasMatch()) {
		return false;
	}
	const auto get = snapshot || devices
		|| request.path.startsWith(QLatin1String("/v1/auth/status/"));
	if (request.method != (get ? "GET" : "POST")
		&& !(snapshot && request.method == "PUT")) {
		return false;
	}
	const auto token = QRegularExpression(
		QStringLiteral("\\A[A-Za-z0-9_-]{32,128}\\z"));
	if (start ? !request.token.isEmpty()
		: !token.match(QString::fromLatin1(request.token)).hasMatch()) {
		return false;
	}
	if (request.method == "GET") {
		return request.body.isEmpty() && request.revision == -1;
	} else if (request.method == "PUT") {
		return request.revision >= 0
			&& !request.body.isEmpty()
			&& request.body.size() <= MaxEnvelope;
	}
	return request.revision == -1 && request.body.size() <= 8192;
}

bool ValidBase(const QUrl &base, bool laboratory) {
	const auto loopback = base.scheme() == QLatin1String("http")
		&& (base.host() == QLatin1String("127.0.0.1")
			|| base.host() == QLatin1String("::1"));
	return base.isValid()
		&& (base.scheme() == QLatin1String("https")
			|| (laboratory && loopback))
		&& !base.host().isEmpty()
		&& base.userInfo().isEmpty()
		&& !base.hasQuery()
		&& !base.hasFragment()
		&& QRegularExpression(QStringLiteral("\\A(?:/[A-Za-z0-9_-]+)*\\z"))
			.match(base.path(QUrl::FullyEncoded)).hasMatch();
}

}

struct FolderHttp::Pending {
	QPointer<QNetworkReply> reply;
	Callback done;
	QByteArray body;
	int limit = 0;
};

bool IsServiceEndpoint(const QUrl &base) {
	return ValidBase(base, false);
}

FolderHttp::FolderHttp(QUrl base, QObject *parent)
:	FolderHttp(std::move(base), false, 20000, parent) {
}

FolderHttp::FolderHttp(
		QUrl base,
		bool laboratory,
		int timeout,
		QObject *parent)
:	QObject(parent)
,	_base(std::move(base))
,	_network(this)
,	_timeout(timeout)
,	_valid(ValidBase(_base, laboratory) && timeout > 0 && timeout <= 60000) {
}

FolderHttp::~FolderHttp() {
	_closing = true;
	for (const auto id : _pending.keys()) {
		finish(id, HttpError::Cancelled);
	}
}

bool FolderHttp::valid() const {
	return _valid;
}

QUrl FolderHttp::base() const {
	return _base;
}

#ifdef AXGRAM_FOLDER_SYNC_LAB
std::unique_ptr<FolderHttp> FolderHttp::Loopback(QUrl base, int timeout) {
	const auto loopback = base.scheme() == QLatin1String("http")
		&& (base.host() == QLatin1String("127.0.0.1")
			|| base.host() == QLatin1String("::1"));
	auto result = std::unique_ptr<FolderHttp>(new FolderHttp(
		std::move(base), loopback, timeout, nullptr));
	result->_valid = result->_valid && loopback;
	return result;
}
#endif

quint64 FolderHttp::send(HttpRequest request, Callback done) {
	Q_ASSERT(thread() == QThread::currentThread());
	if (!_valid || _closing || !done || !ValidRequest(request)
		|| _pending.size() >= 16) {
		if (done && !_closing) {
			QTimer::singleShot(0, this, [done = std::move(done)]() mutable {
				done({ HttpError::InvalidRequest });
			});
		}
		return 0;
	}
	const auto url = QUrl(_base.toString(QUrl::FullyEncoded) + request.path);
	auto wire = QNetworkRequest(url);
	wire.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::ManualRedirectPolicy);
	wire.setAttribute(
		QNetworkRequest::CacheLoadControlAttribute,
		QNetworkRequest::AlwaysNetwork);
	wire.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
	wire.setAttribute(
		QNetworkRequest::CookieLoadControlAttribute,
		QNetworkRequest::Manual);
	wire.setAttribute(
		QNetworkRequest::CookieSaveControlAttribute,
		QNetworkRequest::Manual);
	wire.setRawHeader("Accept-Encoding", "identity");
	if (!request.token.isEmpty()) {
		wire.setRawHeader("Authorization", "Bearer " + request.token);
	}
	if (request.method != "GET") {
		wire.setRawHeader("Content-Type", request.method == "PUT"
			? "application/octet-stream" : "application/json");
	}
	if (request.revision >= 0) {
		wire.setRawHeader("If-Match", '"' + QByteArray::number(request.revision) + '"');
	}
	auto pending = std::make_shared<Pending>();
	pending->limit = request.method == "GET"
		&& request.path == QLatin1String("/v1/snapshot")
		? MaxEnvelope : MaxJson;
	pending->done = std::move(done);
	if (request.method == "GET") {
		pending->reply = _network.get(wire);
	} else if (request.method == "PUT") {
		pending->reply = _network.put(wire, request.body);
	} else {
		pending->reply = _network.post(wire, request.body);
	}
	pending->reply->setReadBufferSize(pending->limit + 1);
	const auto id = ++_next;
	_pending.insert(id, pending);
	connect(pending->reply, &QNetworkReply::readyRead, this, [=] {
		consume(id);
	});
	connect(pending->reply, &QNetworkReply::metaDataChanged, this, [=] {
		const auto length = pending->reply->header(
			QNetworkRequest::ContentLengthHeader).toLongLong();
		if (length > pending->limit) {
			finish(id, HttpError::TooLarge);
		}
	});
	connect(pending->reply, &QNetworkReply::finished, this, [=] {
		const auto guard = QPointer<FolderHttp>(this);
		consume(id);
		if (guard) {
			guard->finish(id);
		}
	});
	const auto timer = new QTimer(pending->reply);
	timer->setSingleShot(true);
	connect(timer, &QTimer::timeout, this, [=] {
		finish(id, HttpError::Timeout);
	});
	timer->start(_timeout);
	return id;
}

void FolderHttp::consume(quint64 id) {
	const auto pending = _pending.value(id);
	if (!pending || !pending->reply) {
		return;
	}
	pending->body += pending->reply->read(pending->limit - pending->body.size() + 1);
	if (pending->body.size() > pending->limit) {
		finish(id, HttpError::TooLarge);
	}
}

void FolderHttp::finish(quint64 id, HttpError forced) {
	const auto pending = _pending.take(id);
	if (!pending || !pending->reply) {
		return;
	}
	const auto reply = pending->reply.data();
	auto result = HttpResponse();
	result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	result.etag = reply->rawHeader("ETag");
	result.digest = reply->rawHeader("X-Content-SHA256");
	if (forced != HttpError::None) {
		result.error = forced;
	} else if (result.status && result.status != 200 && result.status != 204) {
		result.error = HttpError::Status;
	} else if (reply->error() != QNetworkReply::NoError || !result.status) {
		result.error = HttpError::Network;
	}
	if (result.error == HttpError::None) {
		result.body = std::move(pending->body);
	}
	disconnect(reply, nullptr, this, nullptr);
	if (!reply->isFinished()) {
		reply->abort();
	}
	reply->deleteLater();
	if (!_closing && pending->done) {
		pending->done(std::move(result));
	}
}

void FolderHttp::cancel(quint64 id) {
	Q_ASSERT(thread() == QThread::currentThread());
	finish(id, HttpError::Cancelled);
}

}
