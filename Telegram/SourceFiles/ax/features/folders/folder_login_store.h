#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <memory>
#include <optional>

namespace MTP {
class AuthKey;
}

namespace AxFolders {

struct LoginCredential {
	qint64 account = 0;
	QString workspace;
	QString device;
	QByteArray token;
	QByteArray key;
};

enum class LoginStoreStatus {
	Missing,
	Ready,
	Error,
};

struct LoginLoad {
	LoginStoreStatus status = LoginStoreStatus::Missing;
	std::optional<LoginCredential> credential;
};

[[nodiscard]] bool SaveLogin(
	const QString &root,
	const QUrl &endpoint,
	const LoginCredential &credential,
	const std::shared_ptr<MTP::AuthKey> &profileKey = {});
[[nodiscard]] LoginLoad LoadLogin(
	const QString &root,
	const QUrl &endpoint,
	qint64 account,
	const std::shared_ptr<MTP::AuthKey> &profileKey = {});
[[nodiscard]] bool ClearLogin(const QString &root, qint64 account);

}
