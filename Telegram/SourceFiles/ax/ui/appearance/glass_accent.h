#pragma once

#include <QtGui/QColor>
#include <QtCore/QString>

namespace style { class palette; }

namespace AxGlass {

[[nodiscard]] double Contrast(QColor first, QColor second);
[[nodiscard]] QColor Accent(QColor selected, bool dark);
[[nodiscard]] QColor CurrentAccent(bool dark);
void ConfigureAccent(QColor selected);
[[nodiscard]] bool IsGlassTheme(const QString &path);
void TintPalette(style::palette &palette, QColor selected, bool dark);

}
