# Происхождение и авторские права

> **Уведомление об изменении (GNU GPL v3, пункт 5a).**
> Это **изменённая версия** программы [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop),
> которая, в свою очередь, является форком Telegram Desktop.
> Изменения внесены **aevornyx** в июле–августе 2026 года.
> Перечень изменений — ниже в этом файле.

**AxGram Desktop** — форк Telegram Desktop. Этот файл перечисляет всех, чей код
входит в сборку, и на каких условиях он используется.

Лицензия GPL-3.0 требует сохранять уведомления об авторстве и помечать
изменённые файлы. Удалять этот файл или строки копирайта в исходниках нельзя —
именно за это к форкам и предъявляют претензии.

## Цепочка форков

| Проект | Автор | Роль |
|---|---|---|
| [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) | Telegram FZ-LLC, John Preston | первоисточник |
| [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) | Radolyn Labs | прямой предок |
| **AxGram Desktop** | aevornyx | этот репозиторий |

## Лицензия

**GNU General Public License v3.0 с особым исключением** — тем же, под которым
опубликован Telegram Desktop. Полный текст в файле `LICENSE`.

Исключение касается линковки с OpenSSL и обязано оставаться в силе: его нельзя
ни убрать, ни переформулировать. Оно приведено в `LICENSE` дословно.

Из лицензии следует:

- исходный код обязан оставаться открытым;
- любой может форкнуть его на тех же условиях;
- уведомления об авторстве предыдущих участников должны сохраняться;
- изменённые файлы должны быть помечены как изменённые.

## Что изменено в AxGram Desktop

### Отключены обращения к серверам исходного проекта

- `ax_infra.cpp` — убран запуск `RCManager`, который раз в час ходил на
  `update.ayugram.one`;
- `ax_lang.cpp` — отключена догрузка переводов с CDN исходного проекта;
- `ui/settings/settings_main.cpp` — убран блок ссылок на каналы AyuGram.

### Убран сервер обновлений исходного проекта

Клиент принудительно прописывал адрес автообновления
`https://update.ayugram.one/`, затирая любое другое значение. В нашей сборке
автообновление отключено флагом `DESKTOP_APP_DISABLE_AUTOUPDATE`, поэтому
обращений туда не происходило, но адрес удалён и из кода: чужой сервер
обновлений — это возможность доставить пользователям произвольный бинарник.

Там же убран `@ayugramOwner` — аккаунт для пожертвований исходному проекту.

### Убран сбор пожертвований в пользу исходного проекта

Раздел «Поддержка» в настройках («Другое») содержал платёжные реквизиты автора
исходного проекта: ссылку на Boosty и пять криптокошельков (TON, Bitcoin,
Ethereum, Solana, Tron), каждый — с окном QR-кода. Отдельное окно объясняло,
какую сумму перевести и кому отправить подтверждение платежа, чтобы получить
значок. Всё это удалено целиком:

- раздел «Поддержка» и обработчики кнопок в `ui/settings/settings_other.cpp`;
- окна `donate_info_box` и `donate_qr_box` вместе с иконками платёжных систем;
- ссылка `axgram://donate` (она же `://support`), открывавшая то же окно;
- поля сумм и аккаунта для подтверждения в `RCManager`;
- соответствующие строки перевода.

Также убран **запасной сервер удалённой конфигурации** `api.exteragram.app`:
основной адрес был удалён раньше, но при недоступности основного клиент
переключался на него.

**Списки чужих аккаунтов очищены.** В коде были захардкожены 23 идентификатора
разработчиков и 23 канала исходных проектов — этим аккаунтам клиент рисовал
особый значок. Списки опустошены; механика значков сохранена и работает от
пустого списка.

Из набора альтернативных иконок приложения удалены логотипы исходного проекта
(`extera`, `extera2`), а идентификаторы `Extera*` в коде переименованы в `Ax*`.

### Заменены логотипы

Логотип исходного проекта (бумажный самолётик) использовался и как иконка
приложения, и во всех вариантах выбора иконки. Код распространяется по GPL, но
на логотип лицензия не распространяется — это товарный знак. Заменён везде:

- `art/icon16…512(@2x).png`, `art/icon256.ico` (иконка исполняемого файла),
  `art/icon_round512@2x.png`;
- все варианты в `art/ax/*` — та же буква «A», что в мобильной версии AxGram,
  в палитрах прежних вариантов; `default` совпадает с иконкой Android-сборки.

Варианты `chibi` и `chibi2` удалены: это растровые рисунки исходного проекта,
перерисовать их не получится. Варианты `extera`, `extera2` удалены как логотипы
исходного проекта.

### Адреса, испорченные переименованием

Сплошная замена `AyuGram` → `AxGram` превратила рабочие адреса исходного
проекта в правдоподобные, но чужие. Проверка показала, что часть из них
принадлежит посторонним людям:

| Стало после замены | Что это |
|---|---|
| `github.com/AxGram/AxGramDesktop` | организация `AxGram` на GitHub существует и не наша |
| `cdn.jsdelivr.net/gh/AxGram/Languages` | оттуда грузились строки интерфейса |
| `t.me/AxGramFAQ`, `t.me/axgramchat` | оба юзернейма заняты посторонними |

Опасен был второй: CDN переводов мог бы подменять любые строки интерфейса,
и владелец организации `AxGram` получил бы такую возможность. Загрузка
переводов отключена, но и адрес удалён из кода. Остальные адреса переведены
на собственный репозиторий и канал.

Из `com.axgram.desktop.metainfo.xml` убраны почта и название компании автора
исходного проекта, стоявшие там как разработчик AxGram, и скриншот, ведущий
на превью Telegram Desktop.

### Что осталось обращаться наружу

Функция «дата регистрации» работает через телеграм-бота `ayugrambot`
(имя `exteraAuthBot` осталось в коде как реальное имя внешнего бота —
переименовывать его нельзя, иначе функция перестанет работать). Своего
источника у неё нет, так что функция сохранена как есть. Важно понимать: при её
вызове идентификатор запрашиваемого пользователя уходит на этого бота.
Срабатывает только по прямому действию пользователя, фоновых обращений нет.

Пользовательские функции, которые по своей природе ходят наружу и оставлены
без изменений: переводчики (DeepL, Google, Yandex, OpenAI и другие), карты
OpenStreetMap, встраивание музыки.

### Переименование

- `SourceFiles/ayu/` → `SourceFiles/ax/`, `lib_ui/ayu/` → `lib_ui/ax/`,
  каталоги ресурсов `art/ayu`, `icons/ayu` → `.../ax`;
- файлы `ayu_*.cpp/.h` → `ax_*.cpp/.h`, стили и `.qrc` — соответственно;
- идентификаторы `Ayu*` → `Ax*`: классы, пространства имён, функции;
- `core/version.h` — `AppName`/`AppFile` → AxGram, собственный GUID
  `{12FFD268-D3BF-43EB-8CC0-4CA437920205}`;
- `CMakeLists.txt` — `bundle_identifier` → `one.axgram.AxGramDesktop`,
  `output_name` → `AxGram`;
- ресурсы Windows (`Telegram.rc`, `Updater.rc`) — описание и название продукта.

**Осознанно не переименовано** — пути к данным пользователя. Их смена стёрла бы
настройки и всю сохранённую историю удалённых сообщений:

- `tdata/ayu_settings.json` — файл настроек;
- `tdata/ayudata.db` — база сохранённых сообщений.

Ключи переводов `ayu_*` в `Resources/langs/lang.strings` также сохранены:
их значения заменены на AxGram, а сами имена ключей завязаны на кодогенерацию
переводов.

## Товарные знаки

Telegram — товарный знак Telegram FZ-LLC. AxGram Desktop не связан с Telegram
FZ-LLC и не является официальным клиентом.

## Ключи API

Сборка требует собственные `api_id` и `api_hash` с
[core.telegram.org](https://core.telegram.org/api/obtaining_api_id). Они
передаются при конфигурации CMake и **в репозитории не хранятся**:

```
cmake -D TDESKTOP_API_ID=... -D TDESKTOP_API_HASH=... ...
```

Каталог сборки `out/` закрыт в `.gitignore` — вместе с `CMakeCache.txt`,
куда эти значения попадают.


## Изменение от 27 октября 2026: аккаунты

В AxGram снята зависимость дополнительных аккаунтов от Premium. Изменены Telegram/SourceFiles/main/main_domain.cpp, boxes/premium_limits_box.cpp и boxes/premium_preview_box.cpp. Предел хранения 200 аккаунтов сохранён.

## AxGram Windows glass folder rail (2026-11-01)

AxGram modified FiltersMenu, FolderFavoriteButton and optional SideBarButton painting; added a scaled native S-shaped rail, live appearance fallback, and shared Android-matched light/dark colours. Existing filter storage, synchronization, reordering, counters and accessibility routes are retained. The two control glyphs are static SVG frames from useAnimations (MIT); their source and full notice are in Telegram/Resources/icons/ax/useanimations. Motion for the contour is native Qt. This phase does not replace all desktop icons.

Windows appearance preparation (2026-11-01): added optional AxGram Glass light and dark native themes using the existing AxGram Android palette and wallpaper bytes. The themes are selectable in AxGram Appearance; existing themes remain available. No account, folder, sync, or message storage format was changed.


## Archive database protection (2026-11-04)

The bundled SQLite amalgamation is replaced by SQLCipher Community 4.19.0
(BSD 3-clause). Its unmodified source, license and reproducible generation
instructions are in Telegram/SourceFiles/ax/libs/sqlite. The optional Windows archive
database encryption uses OpenSSL and a per-profile random key wrapped by
Windows DPAPI. AXGRAM_ENCRYPT_ARCHIVE_DATABASE defaults to OFF.
Modified initialization preserves existing files on key/migration errors.
This change covers the AxGram archive database, not Telegram's ordinary cache
or, by itself, archived media files. Device-bound keys are not portable backups.
