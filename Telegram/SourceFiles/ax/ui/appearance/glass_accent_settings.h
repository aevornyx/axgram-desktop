#pragma once

#include <QtGui/QColor>
#include <rpl/producer.h>

namespace AxGlass {

[[nodiscard]] QColor SavedAccent();
void SaveAccent(QColor color);
void InitializeAccent();
[[nodiscard]] rpl::producer<QColor> AccentChanges();

}
