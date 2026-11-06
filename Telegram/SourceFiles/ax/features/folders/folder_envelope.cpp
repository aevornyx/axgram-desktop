#include "ax/features/folders/folder_envelope.h"

#include <QtCore/QUuid>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace AxFolders {
namespace {

const auto Header = QByteArray("AXFS\x01", 5);

std::optional<QByteArray> Context(const QString &workspace, qint64 account, const QByteArray &key) {
	if (account <= 0 || key.size() != 32 || workspace != QUuid(workspace).toString(QUuid::WithoutBraces)) {
		return std::nullopt;
	}
	return QByteArrayLiteral("AxGramFolders/v1/") + workspace.toUtf8() + '/' + QByteArray::number(account);
}

}

std::optional<QByteArray> Seal(const QJsonObject &document, const QByteArray &key, const QString &workspace) {
	const auto plaintext = Encode(document);
	const auto aad = Context(workspace, document.value(QStringLiteral("account")).toString().toLongLong(), key);
	if (!plaintext || !aad) {
		return std::nullopt;
	}
	auto nonce = QByteArray(12, '\0');
	if (RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), nonce.size()) != 1) {
		return std::nullopt;
	}
	const auto context = EVP_CIPHER_CTX_new();
	if (!context) {
		return std::nullopt;
	}
	auto ciphertext = QByteArray(plaintext->size() + 16, '\0');
	auto length = 0;
	auto written = 0;
	const auto ok = EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
		&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) == 1
		&& EVP_EncryptInit_ex(context, nullptr, nullptr,
			reinterpret_cast<const unsigned char*>(key.constData()), reinterpret_cast<const unsigned char*>(nonce.constData())) == 1
		&& EVP_EncryptUpdate(context, nullptr, &length, reinterpret_cast<const unsigned char*>(aad->constData()), aad->size()) == 1
		&& EVP_EncryptUpdate(context, reinterpret_cast<unsigned char*>(ciphertext.data()), &written,
			reinterpret_cast<const unsigned char*>(plaintext->constData()), plaintext->size()) == 1
		&& EVP_EncryptFinal_ex(context, reinterpret_cast<unsigned char*>(ciphertext.data()) + written, &length) == 1
		&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, 16, ciphertext.data() + plaintext->size()) == 1;
	EVP_CIPHER_CTX_free(context);
	return (ok && written + length == plaintext->size()) ? std::make_optional(Header + nonce + ciphertext) : std::nullopt;
}

std::optional<QJsonObject> Open(const QByteArray &envelope, const QByteArray &key, const QString &workspace, qint64 account) {
	const auto aad = Context(workspace, account, key);
	if (!aad || envelope.size() < 33 || envelope.size() > 1024 * 1024 || !envelope.startsWith(Header)) {
		return std::nullopt;
	}
	const auto context = EVP_CIPHER_CTX_new();
	if (!context) {
		return std::nullopt;
	}
	const auto size = envelope.size() - 33;
	auto plaintext = QByteArray(size + 16, '\0');
	auto length = 0;
	auto written = 0;
	auto tag = envelope.right(16);
	const auto ok = EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
		&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) == 1
		&& EVP_DecryptInit_ex(context, nullptr, nullptr,
			reinterpret_cast<const unsigned char*>(key.constData()), reinterpret_cast<const unsigned char*>(envelope.constData() + 5)) == 1
		&& EVP_DecryptUpdate(context, nullptr, &length, reinterpret_cast<const unsigned char*>(aad->constData()), aad->size()) == 1
		&& EVP_DecryptUpdate(context, reinterpret_cast<unsigned char*>(plaintext.data()), &written,
			reinterpret_cast<const unsigned char*>(envelope.constData() + 17), size) == 1
		&& EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) == 1
		&& EVP_DecryptFinal_ex(context, reinterpret_cast<unsigned char*>(plaintext.data()) + written, &length) == 1;
	EVP_CIPHER_CTX_free(context);
	auto result = std::optional<QJsonObject>();
	if (ok && written + length == size) {
		plaintext.resize(size);
		result = Decode(plaintext, account);
	}
	OPENSSL_cleanse(plaintext.data(), plaintext.size());
	return result;
}

}
