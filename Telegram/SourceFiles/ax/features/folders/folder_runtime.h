#pragma once

#include "ax/features/folders/folder_exchange.h"
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

namespace AxFolders {

enum class RuntimeState {
	Waiting,
	Paused,
	Exchanging,
	Retrying,
	NeedsAttention,
};

struct RuntimeStatus {
	RuntimeState state = RuntimeState::Waiting;
	ExchangeError error = ExchangeError::None;
	qint64 revision = 0;
};

class FolderRuntime final : public QObject {
public:
	using Status = std::function<void(RuntimeStatus)>;
	FolderRuntime(
		std::unique_ptr<FolderHttp> http,
		qint64 account,
		QString workspace,
		QByteArray token,
		QByteArray key,
		ExchangeStore store,
		Status status,
		QObject *parent = nullptr);
	~FolderRuntime() override;
	void wake();
	void retry();
	void setActive(bool active);
	void close();
	[[nodiscard]] RuntimeStatus status() const;

private:
	void arm(int delay);
	void tick();
	void completed(ExchangeResult result);
	void notify(RuntimeState state, ExchangeError error);

	std::unique_ptr<FolderHttp> _http;
	std::unique_ptr<FolderExchange> _exchange;
	Status _changed;
	QTimer _timer;
	QElapsedTimer _clock;
	RuntimeStatus _status;
	qint64 _notBefore = 0;
	int _failures = 0;
	bool _active = true;
	bool _requested = false;
	bool _suspended = false;
	bool _closed = false;

};

}
