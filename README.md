# Northstar Master - VST3 для Windows (x64)

## Вариант A. Собрать у себя (10-20 минут при первом запуске)

Нужно один раз установить:

1. **Visual Studio 2022** (Community, бесплатно) или *Build Tools for Visual Studio* -
   при установке отметить **Desktop development with C++**.
2. **CMake** 3.22+: `winget install Kitware.CMake` (галочка «Add to PATH»).
3. **Git**: `winget install Git.Git` (CMake скачает JUCE автоматически, нужен интернет).

Затем:

- **Правой кнопкой по `build-windows.bat` -> «Запуск от имени администратора».**
  Скрипт соберёт плагин и сам скопирует его в `C:\Program Files\Common Files\VST3`.
- Откройте DAW и выполните **Rescan / Scan for plug-ins**. Плагин: **Northstar Master**
  (категория Fx / Mastering). Ставьте его на мастер-шину.

Без прав администратора плагин ставится в `%LOCALAPPDATA%\Programs\Common\VST3` -
этот путь сканируют не все DAW; тогда скопируйте папку
`build-windows\NorthstarMastering_artefacts\Release\VST3\Northstar Master.vst3`
в `C:\Program Files\Common Files\VST3` вручную (папку целиком, не только .dll).

Полезные ключи (в PowerShell: `.\build-windows.ps1 -Ключ`):
`-Clean` (пересобрать с нуля), `-BuildStandalone` (ещё и .exe для проверки без DAW),
`-JuceDir <папка>` (использовать скачанный JUCE вместо git).

## Вариант B. Собрать в облаке, ничего не устанавливая

Залейте эту папку в репозиторий на GitHub -> вкладка **Actions** -> **Build Windows VST3**
-> **Run workflow**. Через несколько минут внизу страницы запуска появится артефакт
`Northstar-Master-VST3-Windows-x64` - распакуйте и положите папку
`Northstar Master.vst3` в `C:\Program Files\Common Files\VST3`.

## Если плагин не появился в DAW

- Нужна **64-битная** DAW (32-битные не поддерживаются).
- Убедитесь, что скопирована **вся папка** `Northstar Master.vst3` (внутри `Contents\x86_64-win\`).
- В настройках DAW включите путь `C:\Program Files\Common Files\VST3` и запустите полное
  пересканирование (в некоторых DAW - с очисткой чёрного списка).

## Как пользоваться

1. Вставьте плагин на мастер-шину и запустите воспроизведение репрезентативного фрагмента.
2. Нажмите **Analyze & Master** и оставьте воспроизведение ещё на 4 секунды.
3. Подстройте Target и Amount, выберите Balanced / Warm / Punch. Кнопки Tone, Dynamics,
   Limiter и Bypass позволяют сравнить стадии.

Индикатор громкости - приблизительная оценка, лимитер работает по сэмпл-пику
(не true-peak). Итоговый рендер проверяйте на измерителе громкости.

Лицензия JUCE: у неё есть открытая и коммерческая версии - перед распространением
собранного продукта проверьте, какая вам подходит.
