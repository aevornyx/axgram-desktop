#pragma once

#include "ax/features/folders/folder_http.h"
#include "ax/features/folders/folder_login_store.h"
#include <QtCore/QDeadlineTimer>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <vector>

namespace AxFolders {

enum class LoginError {
	None,
	Network,
	Rejected,
	Invalid,
	Expired,
	Denied,
};

struct LoginFlow {
	qint64 account = 0;
	QUrl endpoint;
	QString id;
	QByteArray pollToken;
	QString confirmation;
	QString clientId;
	QString state;
	int interval = 0;
	QDeadlineTimer deadline;
};

struct LoginResult {
	LoginError error = LoginError::None;
	std::optional<LoginFlow> flow;
	std::optional<LoginCredential> credential;
};

struct SyncDevice {
	QString id;
	QString label;
	qint64 expires = 0;
};

struct SyncDevices {
	QString current;
	bool canManage = false;
	std::vector<SyncDevice> devices;
};

struct DevicesResult {
	LoginError error = LoginError::None;
	std::optional<SyncDevices> list;
};

[[nodiscard]] bool ValidConfirmation(const QString &url);
[[nodiscard]] std::optional<LoginFlow> ParseLoginFlow(
	const QJsonObject &object,
	const QUrl &endpoint,
	qint64 account);
[[nodiscard]] std::optional<LoginCredential> ParseLoginCredential(
	const QJsonObject &object,
	qint64 account);
[[nodiscard]] std::optional<QJsonObject> NativeCallback(
	const LoginFlow &flow,
	const QString &url,
	LoginError &error);

class FolderAuth final : public QObject {
public:
	using Callback = std::function<void(LoginResult)>;
	explicit FolderAuth(FolderHttp *http, QObject *parent = nullptr);
	quint64 start(qint64 account, const QString &label, Callback done);
	quint64 poll(const LoginFlow &flow, Callback done);
	quint64 complete(const LoginFlow &flow, const QString &accepted, Callback done);
	quint64 acknowledge(const LoginFlow &flow, Callback done);
	quint64 devices(const LoginCredential &credential, std::function<void(DevicesResult)> done);
	quint64 revoke(const LoginCredential &credential, const QString &device, Callback done);

private:
	using JsonCallback = std::function<void(LoginError, QJsonObject)>;
	quint64 request(HttpRequest request, JsonCallback done);
	quint64 fail(LoginError error, Callback done);
	[[nodiscard]] bool valid(const LoginFlow &flow) const;
	QPointer<FolderHttp> _http;

};

}
