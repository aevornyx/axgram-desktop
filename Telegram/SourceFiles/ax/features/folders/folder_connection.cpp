#include "ax/features/folders/folder_connection.h"

namespace AxFolders {

FolderConnection::FolderConnection(
		std::unique_ptr<FolderHttp> http,
		qint64 account,
		ConnectionStorage storage,
		ConnectionRuntime runtime,
		Changed changed,
		QObject *parent)
:	QObject(parent)
,	_http(std::move(http))
,	_auth(_http.get())
,	_account(account)
,	_storage(std::move(storage))
,	_runtime(std::move(runtime))
,	_changed(std::move(changed))
,	_timer(this) {
	_timer.setSingleShot(true);
	QObject::connect(&_timer, &QTimer::timeout, this, [=] { poll(); });
}

FolderConnection::~FolderConnection() {
	close();
}

bool FolderConnection::active(quint64 generation) const {
	return !_closed && _generation == generation;
}

void FolderConnection::change(ConnectionPhase phase) {
	_phase = phase;
	const auto changed = _changed;
	if (changed) changed(phase);
}

ConnectionPhase FolderConnection::phase() const {
	return _phase;
}

void FolderConnection::resetAttempt() {
	++_generation;
	_consentActive = false;
	_timer.stop();
	_flow.reset();
	_accepted.clear();
	const auto request = _request;
	_request = 0;
	const auto closers = std::move(_closers);
	_closers.clear();
	if (_http && request) _http->cancel(request);
	for (const auto &close : closers) close();
}

void FolderConnection::restore() {
	if (_closed || _restored) return;
	_restored = true;
	const auto guard = QPointer<FolderConnection>(this);
	const auto generation = _generation;
	change(ConnectionPhase::Restoring);
	if (!guard || !active(generation)) return;
	const auto load = _storage.load;
	const auto saved = load();
	if (!guard || !active(generation)) return;
	if (saved.status == LoginStoreStatus::Missing) { _needsLogin = false; change(ConnectionPhase::Off); }
	else if (saved.status != LoginStoreStatus::Ready || !saved.credential
		|| saved.credential->account != _account) change(ConnectionPhase::Failure);
	else begin(*saved.credential);
}

bool FolderConnection::begin(const LoginCredential &credential) {
	const auto guard = QPointer<FolderConnection>(this);
	const auto generation = _generation;
	const auto start = _runtime.start;
	const auto started = start(credential, [guard, generation](RuntimeStatus status) {
		if (guard && guard->active(generation)) guard->runtimeStatus(status);
	});
	if (!guard || !active(generation)) return false;
	change(started ? ConnectionPhase::Ready : ConnectionPhase::Failure);
	return started;
}

void FolderConnection::connectAccount(Consent consent) {
	if (_closed || !consent || _phase == ConnectionPhase::Restoring
		|| _phase == ConnectionPhase::Confirming || _phase == ConnectionPhase::Connecting) return;
	const auto guard = QPointer<FolderConnection>(this);
	resetAttempt();
	if (!guard || _closed) return;
	_restored = true;
	_disconnecting = false;
	_consent = consent;
	const auto generation = _generation;
	const auto stop = _runtime.stop;
	stop();
	if (!guard || !active(generation)) return;
	change(ConnectionPhase::Connecting);
	if (!guard || !active(generation)) return;
	_request = _auth.start(_account, QStringLiteral("AxGram Windows"), [guard, generation, consent](LoginResult result) {
		if (!guard || !guard->active(generation)) return;
		guard->_request = 0;
		if (result.error != LoginError::None || !result.flow) { guard->change(ConnectionPhase::Failure); return; }
		guard->_flow = result.flow;
		guard->_consentActive = true;
		guard->change(ConnectionPhase::Confirming);
		if (!guard || !guard->active(generation)) return;
		guard->schedule();
		consent({ *result.flow,
			[guard, generation] { return guard && guard->active(generation) && guard->_consentActive; },
			[guard, generation](QString url) {
				if (!guard || !guard->active(generation) || !guard->_consentActive) return;
				guard->_consentActive = false;
				guard->_accepted = std::move(url);
				guard->change(ConnectionPhase::Connecting);
			},
			[guard, generation] { if (guard && guard->active(generation) && guard->_consentActive) guard->cancel(); },
			[guard, generation] {
				if (guard && guard->active(generation) && guard->_consentActive) {
					guard->resetAttempt(); if (guard && !guard->_closed) guard->change(ConnectionPhase::Failure);
				}
			},
			[guard, generation](std::function<void()> close) {
				if (guard && guard->active(generation)) guard->_closers.push_back(std::move(close));
				else close();
			},
			[guard, generation] {
				if (guard && guard->active(generation) && guard->_consentActive) guard->expireConfirmation();
			},
		});
	});
}

void FolderConnection::schedule() {
	if (_closed || !_flow || _request) return;
	_timer.start(_flow->interval * 1000);
}

void FolderConnection::poll() {
	if (_closed || !_flow || _request) return;
	const auto guard = QPointer<FolderConnection>(this);
	const auto generation = _generation;
	_request = _auth.poll(*_flow, [guard, generation](LoginResult result) {
		if (!guard || !guard->active(generation)) return;
		guard->_request = 0;
		if (result.error == LoginError::Network) { guard->schedule(); return; }
		if (result.error == LoginError::Expired) { guard->expireConfirmation(); return; }
		if (result.error != LoginError::None) { guard->resetAttempt(); if (guard && !guard->_closed) guard->change(ConnectionPhase::Failure); return; }
		if (result.credential) { guard->ready(*result.credential); return; }
		if (guard->_accepted.isEmpty()) { guard->schedule(); return; }
		guard->_request = guard->_auth.complete(*guard->_flow, guard->_accepted, [guard, generation](LoginResult result) {
			if (!guard || !guard->active(generation)) return;
			guard->_request = 0;
			if (result.error == LoginError::Denied) { guard->cancel(); return; }
			if (result.error == LoginError::Expired) { guard->expireConfirmation(); return; }
			if (result.error == LoginError::Invalid || result.error == LoginError::Rejected) {
				guard->resetAttempt(); if (guard && !guard->_closed) guard->change(ConnectionPhase::Failure); return;
			}
			guard->schedule();
		});
	});
}

void FolderConnection::ready(LoginCredential credential) {
	const auto guard = QPointer<FolderConnection>(this);
	const auto generation = _generation;
	const auto save = _storage.save;
	const auto saved = credential.account == _account && save(credential);
	if (!guard || !active(generation)) return;
	if (!saved) { change(ConnectionPhase::Failure); return; }
	_needsLogin = false;
	const auto flow = *_flow;
	_timer.stop();
	_request = _auth.acknowledge(flow, [guard, generation](LoginResult) {
		if (guard && guard->active(generation)) guard->_request = 0;
	});
	_flow.reset();
	_accepted.clear();
	_consentActive = false;
	const auto closers = std::move(_closers);
	_closers.clear();
	for (const auto &close : closers) close();
	if (!guard || !active(generation)) return;
	begin(credential);
}

void FolderConnection::runtimeStatus(RuntimeStatus status) {
	if (status.state == RuntimeState::NeedsAttention && status.error == ExchangeError::LoginRequired) {
		_needsLogin = true;
		change(ConnectionPhase::NeedsLogin);
		return;
	}
	if (_needsLogin) return;
	if (status.state == RuntimeState::Exchanging) change(ConnectionPhase::Syncing);
	else if (status.state == RuntimeState::Retrying) change(ConnectionPhase::Retrying);
	else if (status.state == RuntimeState::NeedsAttention) change(ConnectionPhase::Failure);
	else change(ConnectionPhase::Ready);
}

void FolderConnection::expireConfirmation() {
	const auto guard = QPointer<FolderConnection>(this);
	resetAttempt();
	if (guard && !_closed) change(ConnectionPhase::ConfirmationExpired);
}

void FolderConnection::cancel() {
	if (_closed || (!_flow && _phase != ConnectionPhase::Connecting
		&& _phase != ConnectionPhase::Confirming)) return;
	const auto guard = QPointer<FolderConnection>(this);
	resetAttempt();
	if (guard && !_closed) change(_needsLogin ? ConnectionPhase::NeedsLogin : ConnectionPhase::Off);
}

void FolderConnection::disconnectAccount() {
	if (_closed) return;
	const auto guard = QPointer<FolderConnection>(this);
	resetAttempt();
	if (!guard || _closed) return;
	_disconnecting = true;
	_needsLogin = false;
	_consent = {};
	const auto generation = _generation;
	const auto stop = _runtime.stop;
	stop();
	if (!guard || !active(generation)) return;
	const auto clear = _storage.clear;
	const auto cleared = clear();
	if (!guard || !active(generation)) return;
	change(cleared ? ConnectionPhase::Off : ConnectionPhase::Failure);
}

void FolderConnection::retry() {
	if (_closed || (_phase != ConnectionPhase::Failure && _phase != ConnectionPhase::Retrying
		&& _phase != ConnectionPhase::ConfirmationExpired)) return;
	if (_phase == ConnectionPhase::ConfirmationExpired && _consent) {
		const auto consent = _consent;
		connectAccount(consent);
		return;
	}
	if (_disconnecting) { disconnectAccount(); return; }
	if (_flow) { schedule(); return; }
	if (_phase == ConnectionPhase::Retrying) { const auto retry = _runtime.retry; retry(); return; }
	if (_needsLogin && _consent) { const auto consent = _consent; connectAccount(consent); return; }
	const auto guard = QPointer<FolderConnection>(this);
	const auto load = _storage.load;
	const auto saved = load();
	if (!guard || _closed) return;
	if (saved.status == LoginStoreStatus::Missing && _consent) {
		const auto consent = _consent;
		connectAccount(consent);
		return;
	}
	_restored = false;
	restore();
}

void FolderConnection::close() {
	if (_closed) return;
	_closed = true;
	_changed = {};
	_consent = {};
	resetAttempt();
	const auto stop = _runtime.stop;
	stop();
}

void FolderConnection::devices(std::function<void(DevicesResult)> done) {
	if (_closed) return;
	const auto saved = _storage.load();
	if (saved.status != LoginStoreStatus::Ready || !saved.credential || saved.credential->account != _account) {
		done({ LoginError::Expired }); return;
	}
	const auto guard = QPointer<FolderConnection>(this);
	const auto generation = _generation;
	_auth.devices(*saved.credential, [guard, generation, done = std::move(done)](DevicesResult result) {
		if (!guard || !guard->active(generation)) return;
		done(std::move(result));
	});
}

void FolderConnection::revoke(const QString &device, std::function<void(LoginError)> done) {
	if (_closed) return;
	const auto saved = _storage.load();
	if (saved.status != LoginStoreStatus::Ready || !saved.credential || saved.credential->account != _account) {
		done(LoginError::Expired); return;
	}
	const auto self = device == saved.credential->device;
	const auto guard = QPointer<FolderConnection>(this);
	const auto generation = _generation;
	_auth.revoke(*saved.credential, device, [guard, generation, self, done = std::move(done)](LoginResult result) {
		if (!guard || !guard->active(generation)) return;
		if (result.error == LoginError::None && self) guard->disconnectAccount();
		done(result.error);
	});
}

}
