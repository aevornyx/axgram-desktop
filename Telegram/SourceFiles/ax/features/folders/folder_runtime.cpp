#include "ax/features/folders/folder_runtime.h"

#include <QtCore/QThread>
#include <algorithm>

namespace AxFolders {

FolderRuntime::FolderRuntime(
		std::unique_ptr<FolderHttp> http,
		qint64 account,
		QString workspace,
		QByteArray token,
		QByteArray key,
		ExchangeStore store,
		Status status,
		QObject *parent)
:	QObject(parent)
,	_http(std::move(http))
,	_exchange(std::make_unique<FolderExchange>(
		_http.get(),
		account,
		std::move(workspace),
		std::move(token),
		std::move(key),
		std::move(store)))
,	_changed(std::move(status))
,	_timer(this) {
	_clock.start();
	_timer.setSingleShot(true);
	connect(&_timer, &QTimer::timeout, this, [=] { tick(); });
}

FolderRuntime::~FolderRuntime() {
	close();
}

void FolderRuntime::wake() {
	Q_ASSERT(thread() == QThread::currentThread());
	if (_closed || _suspended) {
		return;
	}
	_requested = true;
	if (!_exchange->running()) {
		arm(200);
	}
}

void FolderRuntime::retry() {
	if (_closed || _status.error == ExchangeError::LoginRequired) {
		return;
	}
	_suspended = false;
	_failures = 0;
	_notBefore = 0;
	wake();
}

void FolderRuntime::setActive(bool active) {
	if (_closed) {
		return;
	}
	_active = active;
	if (active) {
		wake();
	} else {
		_timer.stop();
		if (!_exchange->running() && !_suspended) {
			notify(RuntimeState::Paused, _status.error);
		}
	}
}

void FolderRuntime::arm(int delay) {
	if (_closed || !_active || _suspended) {
		return;
	}
	const auto remaining = std::max(qint64(0), _notBefore - _clock.elapsed());
	_timer.start(int(std::max(qint64(delay), remaining)));
}

void FolderRuntime::tick() {
	if (_closed || !_active || _suspended || _exchange->running()) {
		return;
	}
	_requested = false;
	const auto guard = QPointer<FolderRuntime>(this);
	notify(RuntimeState::Exchanging, ExchangeError::None);
	if (!guard || _closed || !_active) {
		return;
	}
	_exchange->run([guard](ExchangeResult result) {
		if (guard && !guard->_closed) {
			guard->completed(result);
		}
	});
}

void FolderRuntime::completed(ExchangeResult result) {
	const auto guard = QPointer<FolderRuntime>(this);
	if (result.error != ExchangeError::None) {
		if (result.error != ExchangeError::Transport
			&& result.error != ExchangeError::RetryRequired) {
			_suspended = true;
			_timer.stop();
			notify(RuntimeState::NeedsAttention, result.error);
			return;
		}
		_failures = std::min(7, _failures + 1);
		_notBefore = _clock.elapsed() + std::min(60000, 1000 << (_failures - 1));
		notify(_active ? RuntimeState::Retrying : RuntimeState::Paused, result.error);
		if (guard && !_closed) {
			arm(0);
		}
		return;
	}
	_status.revision = result.revision;
	_failures = 0;
	_notBefore = 0;
	notify(_active ? RuntimeState::Waiting : RuntimeState::Paused, ExchangeError::None);
	if (guard && !_closed) {
		arm(result.pending || _requested ? 200 : 30000);
	}
}

void FolderRuntime::notify(RuntimeState state, ExchangeError error) {
	_status.state = state;
	_status.error = error;
	const auto changed = _changed;
	const auto value = _status;
	if (changed) {
		changed(value);
	}
}

RuntimeStatus FolderRuntime::status() const {
	return _status;
}

void FolderRuntime::close() {
	if (_closed) {
		return;
	}
	_closed = true;
	_timer.stop();
	_changed = {};
	_exchange->close();
}

}
