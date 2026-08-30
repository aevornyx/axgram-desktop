// Часть AxGram для Desktop. Файл изменён.
//
// Основано на исходном коде AyuGram for Desktop.
// Copyright @Radolyn, 2026
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include <QtGui/QPixmap>

namespace Ax::Ui::Itunes {

QPixmap FetchCover(const QString &performer,
                   const QString &title,
                   int sizeHintPx = 300,
                   int timeoutMs = 5000);

}
