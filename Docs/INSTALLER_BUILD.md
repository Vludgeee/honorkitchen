# Как собрать демо для куратора: Shipping + установщик

## Часть A. Упакованная игра (обязательно)

### Где вообще лежит папка `WindowsNoEditor`?

Она **не встроена в проект заранее**. Появляется **после успешной упаковки** в каталоге, который задаётся при сборке:

| Способ | Где искать |
|--------|------------|
| **Редактор UE:** File → Package Project → Windows (64-bit) | Та папка, которую ты выбрал в мастере (**Browse**). Часто внутри неё создаётся подпапка **`WindowsNoEditor`** с `MyProject.exe`. Точный путь — в **Output Log** после сборки (строки про archive / output). |
| **Скрипт:** `.\Tools\PackageWindowsShipping.ps1` из корня проекта | По умолчанию: **`MyProject\Build\Packaged\`**, внутри — **`WindowsNoEditor`** или **`Windows`** (см. последнюю строку консоли: `Done. Game folder: ...`). |

Проверка: в выбранной папке должен быть **`MyProject.exe`** (имя совпадает с проектом).

---

1. Открой проект в **Unreal Engine 5.3**.
2. **File → Package Project → Windows (64-bit)** *или* запусти **`Tools\PackageWindowsShipping.ps1`** (Shipping через RunUAT).
3. Конфигурация: **Shipping** (в диалоге или в скрипте уже задано).
4. Запомни полный путь к папке, где лежит **`MyProject.exe`** — для установщика это обычно **`...\WindowsNoEditor`** (или `...\Windows`).

Пример пути при использовании скрипта по умолчанию:

`D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject\Build\Packaged\WindowsNoEditor`

---

## Часть B. Установщик (автоматически, рекомендуется)

1. Установи **Inno Setup 6**: https://jrsoftware.org/isdl.php  
2. В корне проекта выполни в PowerShell:

```powershell
.\Tools\BuildInstaller.ps1 -GameSource "D:\Путь\К\WindowsNoEditor"
```

Либо один раз создай файл **`Tools\installer_game_source_path.txt`**: одна строка — тот же путь (без `\*` в конце). Можно скопировать пример:

```text
copy Tools\installer_game_source_path.example.txt Tools\installer_game_source_path.txt
```

затем отредактируй путь и запусти:

```powershell
.\Tools\BuildInstaller.ps1
```

3. Готовый установщик появится в **`Dist\HonorKitchen_Demo_Setup_0.3.0.exe`** (версия задаётся в `Tools\InnoSetup_MyProject_Demo.iss`, поле `MyAppVersion`).

### Одной командой (упаковка + установщик)

```powershell
.\Tools\PackageWindowsShipping.ps1 -BuildInstaller
```

Требуется закрытый редактор UE и установленный Inno Setup 6.

В установку автоматически копируется **`Docs\README_DEMO.txt`** как `README_DEMO.txt` в папку игры (флаг «показать readme» после установки).

### Ручная сборка в Inno Setup GUI

1. Открой **`Tools\InnoSetup_MyProject_Demo.iss`**.  
2. В блоке `#ifndef GameSourceDir` задай `#define GameSourceDir "D:\...\WindowsNoEditor\*"` (обязательно **`\*`** в конце).  
3. **Build → Compile**.  
4. Результат также в **`Dist\`**.

### VC++ Redistributable

Если на чужом ПК игра не стартует — дай куратору **VC_redist.x64.exe** отдельно или положи в **`Tools\redist\`** и раскомментируй строки в `[Files]` и `[Run]` в `.iss`.

---

## Часть C. Что положить куратору на флешку / в облако

1. **`Dist\HonorKitchen_Demo_Setup_*.exe`** (или целиком папка `WindowsNoEditor`, если без установщика).
2. **`VC_redist.x64.exe`** — ссылка в `README_DEMO.txt`.
3. (Опционально) **`Docs\CURATOR_COLLEGE_PRESENTATION.pptx`**.

---

## Быстрая проверка перед выходом из дома

- Установка в **пустую** папку, запуск exe.
- 2–3 минуты: меню → игра → выход.

---

## «Установилась старая версия / без генерации»

Установщик **не собирает** игру заново — он **копирует** то, что уже лежит в папке `WindowsNoEditor`.

1. **Пакуй в UE в ту же папку**, откуда читает Inno: по умолчанию это **`MyProject\Build\Packaged\WindowsNoEditor`** (скрипт `PackageWindowsShipping.ps1`) или путь в **`Tools\installer_game_source_path.txt`**. Если в редакторе ты выбрал **другой диск/папку** при Package — Inno всё ещё копирует **старую** `Build\Packaged\...`.

2. Перед новым Package имеет смысл **удалить** `Build\Packaged\WindowsNoEditor` (или всю `Build\Packaged`), чтобы не остался кэш.

3. Собирай установщик через **`BuildInstaller.ps1`** — в консоли будет жёлтый блок «ПРОВЕРЬ ИСТОЧНИК» с датой **`MyProject.exe`**. После установки в папке игры открой **`BUILD_FROM_INSTALLER.txt`** — там путь и время exe на момент сборки установщика.

4. Если ставил из **Inno GUI** без скрипта: теперь по умолчанию в `.iss` стоит относительный путь **`..\Build\Packaged\WindowsNoEditor\*`** (от папки `Tools\`). Не меняй его на выдуманный `D:\Path\To\...`.

5. Удали старую игру: **Параметры Windows → Приложения → HonorKitchen Demo → Удалить**, потом ставь новый `Setup` из **`Dist\`** (смотри дату файла `.exe`).
