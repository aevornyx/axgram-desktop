#pragma once

#include "ax/features/folders/folder_auth.h"
#include "ax/features/folders/folder_runtime.h"

namespace AxFolders {

enum class ConnectionPhase {
	Off,
	Restoring,
	Confirming,
	Connecting,
	Ready,
	Syncing,
	Retrying,
	NeedsLogin,
	Failure,
	ConfirmationExpired,
};

struct ConnectionStorage {
	std::function<LoginLoad()> load;
	std::function<bool(const LoginCredential &)> save;
	std::function<bool()> clear;
};

struct ConnectionRuntime {
	std::function<bool(const LoginCredential &, FolderRuntime::Status)> start;
	std::function<void()> stop;
	std::function<void()> retry;
};

struct ConnectionConsent {
	LoginFlow flow;
	std::function<bool()> active;
	std::function<void(QString)> accepted;
	std::function<void()> cancelled;
	std::function<void()> failed;
	std::function<void(std::function<void()>)> bindClose;
	std::function<void()> expired;
};

class FolderConnection final : public QObject {
public:
	using Changed = std::function<void(ConnectionPhase)>;
	using Consent = std::function<void(ConnectionConsent)>;
	FolderConnection(
		std::unique_ptr<FolderHttp> http,
		qint64 account,
		ConnectionStorage storage,
		ConnectionRuntime runtime,
		Changed changed,
		QObject *parent = nullptr);
	~FolderConnection() override;
	void restore();
	void connectAccount(Consent consent);
	void cancel();
	void disconnectAccount();
	void retry();
	void devices(std::function<void(DevicesResult)> done);
	void revoke(const QString &device, std::function<void(LoginError)> done);
	void close();
	[[nodiscard]] ConnectionPhase phase() const;

private:
	void poll();
	void schedule();
	void ready(LoginCredential credential);
	bool begin(const LoginCredential &credential);
	void runtimeStatus(RuntimeStatus status);
	void change(ConnectionPhase phase);
	void resetAttempt();
	void expireConfirmation();
	[[nodiscard]] bool active(quint64 generation) const;

	std::unique_ptr<FolderHttp> _http;
	FolderAuth _auth;
	qint64 _account = 0;
	ConnectionStorage _storage;
	ConnectionRuntime _runtime;
	Changed _changed;
	Consent _consent;
	QTimer _timer;
	std::optional<LoginFlow> _flow;
	QString _accepted;
	std::vector<std::function<void()>> _closers;
	ConnectionPhase _phase = ConnectionPhase::Off;
	quint64 _generation = 0;
	quint64 _request = 0;
	bool _restored = false;
	bool _disconnecting = false;
	bool _closed = false;
	bool _consentActive = false;
	bool _needsLogin = false;

};

}
