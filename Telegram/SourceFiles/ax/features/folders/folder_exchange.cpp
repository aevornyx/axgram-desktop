#include "ax/features/folders/folder_exchange.h"

#include "ax/features/folders/folder_envelope.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <limits>

namespace AxFolders {
namespace {

std::optional<qint64> Revision(const QByteArray &tag) {
	if (!QRegularExpression(QStringLiteral("\\A\"(0|[1-9][0-9]{0,18})\"\\z"))
		.match(QString::fromLatin1(tag)).hasMatch()) {
		return std::nullopt;
	}
	auto ok = false;
	const auto result = tag.mid(1, tag.size() - 2).toLongLong(&ok);
	return ok ? std::optional<qint64>(result) : std::nullopt;
}

}

FolderExchange::FolderExchange(
		FolderHttp *http,
		qint64 account,
		QString workspace,
		QByteArray token,
		QByteArray key,
		ExchangeStore store,
		QObject *parent)
:	QObject(parent)
,	_http(http)
,	_account(account)
,	_workspace(std::move(workspace))
,	_token(std::move(token))
,	_key(key.constData(), key.size())
,	_store(std::move(store)) {
	if (http) {
		connect(http, &QObject::destroyed, this, [=] {
			if (_running) {
				finish({ ExchangeError::Transport });
			}
		});
	}
}

FolderExchange::~FolderExchange() {
	close();
}

bool FolderExchange::run(Callback done) {
	Q_ASSERT(thread() == QThread::currentThread());
	if (_closed || _running || !done) {
		return false;
	}
	_done = std::move(done);
	_running = true;
	_attempt = 0;
	_device.clear();
	const auto operation = ++_operation;
	if (!_http || !_http->valid() || _account <= 0 || _key.size() != 32
		|| _workspace != QUuid(_workspace).toString(QUuid::WithoutBraces)
		|| !_store.load || !_store.compareAndSave
		|| !QRegularExpression(QStringLiteral("\\A[A-Za-z0-9_-]{32,128}\\z"))
			.match(QString::fromLatin1(_token)).hasMatch()) {
		QTimer::singleShot(0, this, [=] {
			if (active(operation)) {
				finish({ ExchangeError::Configuration });
			}
		});
		return true;
	}
	attempt();
	return true;
}

std::optional<Replica> FolderExchange::local() const {
	const auto load = _store.load;
	const auto account = _account;
	const auto device = _device;
	const auto state = load();
	return state && state->account == account
		&& (device.isEmpty() || state->device == device)
		&& EncodeState(*state) ? state : std::nullopt;
}

void FolderExchange::attempt() {
	if (++_attempt > 4) {
		finish({ ExchangeError::RetryRequired });
		return;
	}
	const auto guard = QPointer<FolderExchange>(this);
	const auto operation = _operation;
	const auto state = local();
	if (!guard || !guard->active(operation)) {
		return;
	}
	if (!state || !_http) {
		finish({ ExchangeError::Storage });
		return;
	}
	_device = state->device;
	_request = _http->send({ "GET", QStringLiteral("/v1/snapshot"), _token },
		[guard, operation](HttpResponse response) {
			if (guard && guard->active(operation)) {
				guard->_request = 0;
				guard->received(std::move(response));
			}
		});
}

ExchangeError FolderExchange::responseError(const HttpResponse &response) const {
	return response.status == 401 || response.status == 403
		? ExchangeError::LoginRequired : ExchangeError::Transport;
}

void FolderExchange::received(HttpResponse response) {
	if (response.error != HttpError::None) {
		finish({ responseError(response) });
		return;
	}
	const auto revision = Revision(response.etag);
	if (response.status != 200 || !revision
		|| (*revision > 0 && response.digest != QCryptographicHash::hash(
			response.body, QCryptographicHash::Sha256).toHex())
		|| (*revision == 0 && !response.body.isEmpty())) {
		finish({ ExchangeError::InvalidSnapshot });
		return;
	}
	const auto remote = *revision == 0
		? std::optional<QJsonObject>(QJsonObject{
			{ QStringLiteral("v"), 1 },
			{ QStringLiteral("account"), QString::number(_account) },
			{ QStringLiteral("folders"), QJsonArray() },
		})
		: Open(response.body, _key, _workspace, _account);
	if (!remote) {
		finish({ ExchangeError::InvalidSnapshot });
		return;
	}
	const auto guard = QPointer<FolderExchange>(this);
	const auto operation = _operation;
	const auto before = local();
	if (!guard || !guard->active(operation)) {
		return;
	}
	const auto merged = before ? Combine(*before, *remote) : std::nullopt;
	if (!before || !merged) {
		finish({ ExchangeError::Storage });
		return;
	}
	const auto apply = _store.compareAndSave;
	const auto applied = apply(*before, *merged);
	if (!guard || !guard->active(operation)) {
		return;
	}
	const auto latest = local();
	if (!guard || !guard->active(operation)) {
		return;
	}
	if (!latest) {
		finish({ ExchangeError::Storage });
		return;
	} else if (!applied) {
		if (EncodeState(*latest) == EncodeState(*before)) {
			finish({ ExchangeError::Storage });
		} else {
			attempt();
		}
		return;
	}
	const auto changed = EncodeState(*latest) != EncodeState(*merged);
	if (*revision > 0 && Encode(merged->document) == Encode(*remote)) {
		finish({ ExchangeError::None, *revision, changed });
		return;
	} else if (changed) {
		attempt();
		return;
	}
	const auto sealed = Seal(merged->document, _key, _workspace);
	if (!sealed) {
		finish({ ExchangeError::InvalidSnapshot });
		return;
	}
	_request = _http->send(
		{ "PUT", QStringLiteral("/v1/snapshot"), _token, *sealed, *revision },
		[guard, operation, saved = *merged, revision = *revision](HttpResponse response) {
			if (guard && guard->active(operation)) {
				guard->_request = 0;
				guard->uploaded(std::move(response), saved, revision);
			}
		});
}

void FolderExchange::uploaded(
		HttpResponse response,
		const Replica &saved,
		qint64 revision) {
	if (response.status == 409 && response.error == HttpError::Status) {
		attempt();
		return;
	} else if (response.error != HttpError::None) {
		finish({ responseError(response) });
		return;
	}
	const auto written = Revision(response.etag);
	if (response.status != 204 || !response.body.isEmpty() || !written
		|| revision == std::numeric_limits<qint64>::max()
		|| *written != revision + 1) {
		finish({ ExchangeError::InvalidSnapshot });
		return;
	}
	const auto guard = QPointer<FolderExchange>(this);
	const auto operation = _operation;
	const auto latest = local();
	if (!guard || !guard->active(operation)) {
		return;
	}
	if (!latest) {
		finish({ ExchangeError::Storage });
		return;
	}
	finish({ ExchangeError::None, *written,
		EncodeState(*latest) != EncodeState(saved) });
}

void FolderExchange::finish(ExchangeResult result) {
	_running = false;
	_request = 0;
	auto done = std::move(_done);
	_done = {};
	if (done) {
		done(result);
	}
}

bool FolderExchange::active(quint64 operation) const {
	return !_closed && _running && _operation == operation;
}

bool FolderExchange::running() const {
	return _running;
}

void FolderExchange::close() {
	Q_ASSERT(thread() == QThread::currentThread());
	if (_closed) {
		return;
	}
	_closed = true;
	_running = false;
	++_operation;
	_done = {};
	_key.fill('\0');
	_key.clear();
	if (_http && _request) {
		_http->cancel(_request);
	}
	_request = 0;
}

}
