// Часть AxGram для Desktop.
//
// Распространяется по GNU GPL v3 — см. LICENSE и NOTICE.md.
#pragma once

#include <QString>

namespace Window {
class SessionController;
} // namespace Window

namespace Ax::Updates {

// Владелец и репозиторий на GitHub. Должны совпадать с тем, куда
// публикуются релизы, иначе проверка ничего не найдёт.
inline constexpr auto kGithubOwner = "aevornyx";
inline constexpr auto kGithubRepo = "axgram-desktop";

// Бот поддержки. Пока пусто — кнопка «Поддержать» в настройках не показывается,
// приложение никуда не ходит. Юзернейм без символа @.
inline constexpr auto kSupportBot = "axgram_support_bot";

[[nodiscard]] QString ReleasesPageUrl();

[[nodiscard]] bool SupportAvailable();
void OpenSupport();

// Спрашивает у GitHub последний релиз и показывает результат пользователем.
// Автообновление в сборке отключено (DESKTOP_APP_DISABLE_AUTOUPDATE),
// поэтому файл пользователь скачивает и ставит сам.
void CheckManually(not_null<Window::SessionController*> controller);

} // namespace Ax::Updates
