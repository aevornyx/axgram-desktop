#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <functional>
#include <memory>

namespace AxFolders {

[[nodiscard]] bool IsServiceEndpoint(const QUrl &base);

enum class HttpError {
	None,
	InvalidRequest,
	Network,
	Status,
	TooLarge,
	Timeout,
	Cancelled,
};

struct HttpRequest {
	QByteArray method;
	QString path;
	QByteArray token;
	QByteArray body;
	qint64 revision = -1;
};

struct HttpResponse {
	HttpError error = HttpError::None;
	int status = 0;
	QByteArray body;
	QByteArray etag;
	QByteArray digest;
};

class FolderHttp final : public QObject {
public:
	using Callback = std::function<void(HttpResponse)>;
	explicit FolderHttp(QUrl base, QObject *parent = nullptr);
	~FolderHttp() override;
	[[nodiscard]] bool valid() const;
	[[nodiscard]] QUrl base() const;
	quint64 send(HttpRequest request, Callback done);
	void cancel(quint64 id);

#ifdef AXGRAM_FOLDER_SYNC_LAB
	[[nodiscard]] static std::unique_ptr<FolderHttp> Loopback(
		QUrl base,
		int timeout = 20000);
#endif

private:
	struct Pending;
	FolderHttp(QUrl base, bool laboratory, int timeout, QObject *parent);
	void consume(quint64 id);
	void finish(quint64 id, HttpError forced = HttpError::None);

	QUrl _base;
	QNetworkAccessManager _network;
	QMap<quint64, std::shared_ptr<Pending>> _pending;
	quint64 _next = 0;
	int _timeout = 20000;
	bool _valid = false;
	bool _closing = false;

};

}
