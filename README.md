# AxGram Desktop

Неофициальный клиент Telegram для Windows и Linux. Сохраняет удалённые и
отредактированные сообщения, поддерживает режим призрака и синхронизацию
дополнительных папок между устройствами AxGram.

> **Изменённая версия.** AxGram Desktop — форк
> [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) (который сам является
> форком Telegram Desktop). Изменения внесены aevornyx в июле–августе 2026 года;
> перечень — в [NOTICE.md](NOTICE.md).
>
> AxGram не связан с Telegram FZ-LLC и не является официальным клиентом.

## Скачать

В [последнем релизе](https://github.com/aevornyx/axgram-desktop/releases/latest)
доступны обе сборки.

| Платформа | Файл | Запуск |
| --- | --- | --- |
| Windows x64 | `AxGram-Setup-x64-*.exe` | Запустите установщик. Удаление доступно в разделе «Установленные приложения» Windows. |
| Linux x86‑64 | `AxGram-*-x86_64.AppImage` | Разрешите выполнение файла и запустите его. Для обычного запуска нужны GTK 3 и FUSE 2. |

Установщик Windows 7.0.13 выпущен без цифровой подписи Authenticode.
AppImage 7.0.13 проверен на Ubuntu 24.04.4 LTS.
Контрольные суммы обоих файлов находятся в `SHA256SUMS` на странице релиза.

```sh
chmod +x AxGram-7.0.13-x86_64.AppImage
./AxGram-7.0.13-x86_64.AppImage
```

## Чем отличается от исходного форка

Собран на основе [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop).
Отличия — прежде всего в том, чего в нём **нет**.

**Инфраструктура AxGram.** Исходный форк раз в час обращался к
`update.ayugram.one` и подгружал переводы со своего CDN. И то, и другое
отключено. Дополнительные папки синхронизируются через сервис AxGram после подключения в настройках.

**Переименовано целиком** — не только название в заголовке окна: каталоги
исходников, файлы, классы, пространства имён, ресурсы Windows, идентификатор
приложения. Внутри не осталось следов исходного проекта, кроме путей к данным
пользователя — их переименование стёрло бы настройки и сохранённую историю.

## Возможности

- сохранение удалённых сообщений с пометкой и приглушением;
- история правок сообщений;
- режим призрака: не отмечать прочитанным, не показывать активность;
- сохранение медиа из чатов с запретом пересылки;
- фильтры сообщений по регулярным выражениям.

## Сборка

Инструкции по сборке и упаковке для Windows и Linux собраны в
[AXGRAM-RELEASE.md](Telegram/build/AXGRAM-RELEASE.md).

### Windows

Нужны Visual Studio 2022 с инструментами C++, CMake и подготовленные
`Libraries` — как для обычного Telegram Desktop
([инструкция](https://github.com/telegramdesktop/tdesktop/blob/dev/docs/building-win-x64.md)).

**Ключи Telegram API** получить на
[core.telegram.org](https://core.telegram.org/api/obtaining_api_id) и передать
при конфигурации — в репозитории они не хранятся.

Сборка запускается **только из PowerShell**, не из Git Bash: тот при вызове
`.bat` отдаёт лишь баннер cmd.

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" && cd /d <путь-к-репозиторию> && cmake --build out --config Release --target Telegram --parallel 1 -- /m:1 /p:CL_MPCount=3'
```

Перед сборкой закрыть запущенный клиент — компоновщик не перезапишет файл:

```powershell
Get-Process AxGram -ErrorAction SilentlyContinue | Stop-Process -Force
```

Результат — `out/Release/AxGram.exe`. Линковка долгая, несколько минут.

## Известные ограничения

- Иконка окна на панели задач кэшируется Windows по `AppUserModelID` на время
  работы приложения: выбранная в настройках иконка применяется к панели задач
  только после перезапуска.
- Иконка самого `.exe` и ярлыка вшита в бинарник и через настройки не меняется.

## Лицензия

GNU General Public License v3.0 с особым исключением для линковки с OpenSSL —
см. [LICENSE](LICENSE). Исключение унаследовано от Telegram Desktop и обязано
оставаться в силе.

Список авторов и цепочка форков — в [NOTICE.md](NOTICE.md).
