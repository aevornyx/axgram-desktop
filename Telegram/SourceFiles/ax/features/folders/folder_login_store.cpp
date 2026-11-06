#include "ax/features/folders/folder_login_store.h"

#include "ax/features/folders/folder_document.h"
#include "ax/features/folders/folder_http.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QUuid>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <dpapi.h>
#elif defined(Q_OS_LINUX)
#include "storage/details/storage_file_utilities.h"
#endif

namespace AxFolders {
namespace {

QString LoginPath(const QString &root, qint64 account) {
	return QDir(root).filePath(QString::number(account) + QStringLiteral("/login.bin"));
}

QByteArray Binding(const QUrl &endpoint, qint64 account) {
	return QCryptographicHash::hash(
		"AxGramFolders/login/v1\n" + endpoint.toEncoded() + '\n' + QByteArray::number(account),
		QCryptographicHash::Sha256);
}

std::optional<QByteArray> EncodeCredential(const LoginCredential &credential) {
	if (credential.account <= 0 || credential.key.size() != 32
		|| credential.workspace != QUuid(credential.workspace).toString(QUuid::WithoutBraces)
		|| credential.device != QUuid(credential.device).toString(QUuid::WithoutBraces)
		|| !QRegularExpression(QStringLiteral("\\A[A-Za-z0-9_-]{32,128}\\z"))
			.match(QString::fromLatin1(credential.token)).hasMatch()) {
		return std::nullopt;
	}
	return CanonicalJson({
		{ QStringLiteral("account"), QString::number(credential.account) },
		{ QStringLiteral("workspace"), credential.workspace },
		{ QStringLiteral("device"), credential.device },
		{ QStringLiteral("token"), QString::fromLatin1(credential.token) },
		{ QStringLiteral("key"), QString::fromLatin1(credential.key.toBase64(
			QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)) },
	}, 4096);
}

std::optional<LoginCredential> DecodeCredential(const QByteArray &plain, qint64 account) {
	const auto object = ParseCanonicalJson(plain, 4096);
	if (!object || object->size() != 5
		|| object->value(QStringLiteral("account")) != QString::number(account)) {
		return std::nullopt;
	}
	const auto encoded = object->value(QStringLiteral("key")).toString().toLatin1();
	const auto key = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding);
	auto credential = LoginCredential{
		account,
		object->value(QStringLiteral("workspace")).toString(),
		object->value(QStringLiteral("device")).toString(),
		object->value(QStringLiteral("token")).toString().toLatin1(),
		key,
	};
	if (key.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals) != encoded
		|| EncodeCredential(credential) != std::optional<QByteArray>(plain)) {
		return std::nullopt;
	}
	return credential;
}

std::optional<QByteArray> Protect(
		const QByteArray &bytes,
		const QByteArray &binding,
		bool seal,
		[[maybe_unused]] const std::shared_ptr<MTP::AuthKey> &profileKey) {
#ifdef Q_OS_WIN
	auto input = DATA_BLOB{ DWORD(bytes.size()), reinterpret_cast<BYTE*>(const_cast<char*>(bytes.constData())) };
	auto entropy = DATA_BLOB{ DWORD(binding.size()), reinterpret_cast<BYTE*>(const_cast<char*>(binding.constData())) };
	auto output = DATA_BLOB{ 0, nullptr };
	const auto succeeded = seal
		? CryptProtectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)
		: CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
	if (!succeeded) {
		return std::nullopt;
	}
	auto result = QByteArray();
	if (output.cbData <= 16384) {
		result = QByteArray(reinterpret_cast<const char*>(output.pbData), int(output.cbData));
	}
	SecureZeroMemory(output.pbData, output.cbData);
	LocalFree(output.pbData);
	return result.isEmpty() ? std::nullopt : std::optional<QByteArray>(std::move(result));
#elif defined(Q_OS_LINUX)
	if (!profileKey) {
		return std::nullopt;
	}
	if (seal) {
		auto descriptor = Storage::details::EncryptedDescriptor(binding.size() + bytes.size());
		descriptor.stream.writeRawData(binding.constData(), binding.size());
		descriptor.stream.writeRawData(bytes.constData(), bytes.size());
		auto result = Storage::details::PrepareEncrypted(descriptor, profileKey);
		descriptor.data.fill('\0');
		return result;
	}
	auto descriptor = Storage::details::EncryptedDescriptor();
	if (!Storage::details::DecryptLocal(descriptor, bytes, profileKey)) {
		return std::nullopt;
	}
	const auto plain = descriptor.buffer.readAll();
	descriptor.data.fill('\0');
	return plain.startsWith(binding)
		? std::make_optional(plain.mid(binding.size()))
		: std::nullopt;
#else
	return std::nullopt;
#endif
}

}

bool SaveLogin(
		const QString &root,
		const QUrl &endpoint,
		const LoginCredential &credential,
		const std::shared_ptr<MTP::AuthKey> &profileKey) {
	if (root.isEmpty() || !IsServiceEndpoint(endpoint)) {
		return false;
	}
	auto plain = EncodeCredential(credential);
	if (!plain) {
		return false;
	}
	const auto sealed = Protect(*plain, Binding(endpoint, credential.account), true, profileKey);
	plain->fill('\0');
	if (!sealed) {
		return false;
	}
	const auto path = LoginPath(root, credential.account);
	if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
		return false;
	}
	auto file = QSaveFile(path);
	return file.open(QIODevice::WriteOnly)
		&& file.write(*sealed) == sealed->size()
		&& file.commit();
}

LoginLoad LoadLogin(
		const QString &root,
		const QUrl &endpoint,
		qint64 account,
		const std::shared_ptr<MTP::AuthKey> &profileKey) {
	if (root.isEmpty() || account <= 0 || !IsServiceEndpoint(endpoint)) {
		return { LoginStoreStatus::Error };
	}
	auto file = QFile(LoginPath(root, account));
	if (!file.exists()) {
		return {};
	}
	if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 16384) {
		return { LoginStoreStatus::Error };
	}
	const auto sealed = file.read(16385);
	if (sealed.size() > 16384 || !file.atEnd()) {
		return { LoginStoreStatus::Error };
	}
	auto plain = Protect(sealed, Binding(endpoint, account), false, profileKey);
	if (!plain) {
		return { LoginStoreStatus::Error };
	}
	const auto credential = DecodeCredential(*plain, account);
	plain->fill('\0');
	return credential ? LoginLoad{ LoginStoreStatus::Ready, credential }
		: LoginLoad{ LoginStoreStatus::Error };
}

bool ClearLogin(const QString &root, qint64 account) {
	if (root.isEmpty() || account <= 0) {
		return false;
	}
	const auto path = LoginPath(root, account);
	return !QFile::exists(path) || QFile::remove(path);
}

}
