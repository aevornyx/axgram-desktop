#pragma once

#include "ax/features/folders/folder_http.h"
#include "ax/features/folders/folder_replica.h"
#include <QtCore/QPointer>

namespace AxFolders {

enum class ExchangeError {
	None,
	Configuration,
	Transport,
	LoginRequired,
	InvalidSnapshot,
	Storage,
	RetryRequired,
};

struct ExchangeResult {
	ExchangeError error = ExchangeError::None;
	qint64 revision = 0;
	bool pending = false;
};

struct ExchangeStore {
	std::function<std::optional<Replica>()> load;
	std::function<bool(const Replica &, const Replica &)> compareAndSave;
};

class FolderExchange final : public QObject {
public:
	using Callback = std::function<void(ExchangeResult)>;
	FolderExchange(
		FolderHttp *http,
		qint64 account,
		QString workspace,
		QByteArray token,
		QByteArray key,
		ExchangeStore store,
		QObject *parent = nullptr);
	~FolderExchange() override;
	bool run(Callback done);
	void close();
	[[nodiscard]] bool running() const;

private:
	void attempt();
	void received(HttpResponse response);
	void uploaded(HttpResponse response, const Replica &saved, qint64 revision);
	void finish(ExchangeResult result);
	[[nodiscard]] std::optional<Replica> local() const;
	[[nodiscard]] bool active(quint64 operation) const;
	[[nodiscard]] ExchangeError responseError(const HttpResponse &response) const;

	QPointer<FolderHttp> _http;
	qint64 _account = 0;
	QString _workspace;
	QString _device;
	QByteArray _token;
	QByteArray _key;
	ExchangeStore _store;
	Callback _done;
	quint64 _operation = 0;
	quint64 _request = 0;
	int _attempt = 0;
	bool _running = false;
	bool _closed = false;

};

}
