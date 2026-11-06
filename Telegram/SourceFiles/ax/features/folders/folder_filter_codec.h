#pragma once

#include "data/data_chat_filters.h"
#include <QtCore/QJsonObject>
#include <optional>

namespace AxFolders {

[[nodiscard]] std::optional<QJsonObject> ExportFilter(const Data::ChatFilter &filter);
[[nodiscard]] std::optional<Data::ChatFilter> ImportFilter(
	not_null<Data::Session*> owner,
	FilterId localId,
	const QJsonObject &values);

}
