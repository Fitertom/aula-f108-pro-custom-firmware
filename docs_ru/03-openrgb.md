# 3. Поддержка OpenRGB

Контроллер лежит в `openrgb/Controllers/AULAF108ProController`. Он работает только с кастомной прошивкой этого проекта (в заводской
нет режима прямого RGB), по кабелю и через донгл 2.4G.

## Зоны в OpenRGB

| Зона | LED | Тип |
|---|---|---|
| Keyboard | 104 клавиши | матрица |
| Arrow bar | 5 сегментов над стрелками, слева направо | линейная |
| Left strip | 7 светодиодов, от дальнего края к ближнему | линейная |
| Right strip | 7 светодиодов, от ближнего края к дальнему | линейная |

Светодиодов 140-146 и около 20 ячеек матрицы физически нет, они не показываются. У зоны над стрелками красный и зелёный перепутаны
аппаратно (GRB), контроллер это учитывает, см. `AULAF108ProLayout.h` (`AULA_ARROW_BAR_SWAP_RG`, `AULA_KEYS_SWAP_RG`,
`AULA_SIDES_SWAP_RG`).

Режимы: Direct (всё управляется из OpenRGB) и "Keyboard's own lighting" (заводская подсветка). Если закрыть OpenRGB или выдернуть
кабель, клавиатура сама вернётся к своей подсветке примерно через 1,3 секунды.

## Два канала связи

| | Кабель | Донгл 2.4G |
|---|---|---|
| USB ID | 0C45:800A, интерфейс 3, usage page 0xFF13 | 05AC:024F, интерфейс 3, usage page 0xFF60 |
| Имя в OpenRGB | "AULA F108 Pro" | "AULA F108 Pro (2.4G)" |
| Скорость | полные кадры в обычном темпе | ограничение радиоканала, примерно 100-125 пакетов в секунду, задержка около 14 мс |

По 2.4G полный кадр из 147 светодиодов это много пакетов, поэтому быстрые эффекты рывками и с задержкой. Контроллер отправляет только
изменившиеся блоки. Статичные цвета и медленные эффекты работают нормально.

## Вариант A. Собрать OpenRGB самому (рекомендуется)

Контроллер написан под OpenRGB master, коммит `3021bc4f930c74c867a8f47d425031f55e47ef43` (октябрь 2026). В новых версиях могут
понадобиться небольшие правки под изменения API.

1. `git clone https://gitlab.com/CalcProgrammer1/OpenRGB`, при желании переключись на коммит выше.
2. Скопируй `openrgb/Controllers/AULAF108ProController` в `OpenRGB/Controllers/`. Файл проекта править не нужно: qmake собирает всё из
   `Controllers` по маске.
3. Собери по `Documentation/Compiling.md` из репозитория OpenRGB.

Windows, что использовал автор: Qt 6.8.3 (msvc2022_64), Visual Studio 2022 Build Tools (C++ x64), jom. В консоли с окружением MSVC и
Qt `bin` в PATH:

```
qmake OpenRGB.pro CONFIG-=debug_and_release CONFIG+=release
jom -j 16
windeployqt --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-opengl-sw --no-network .\release\
```

Результат: `release\OpenRGB.exe`. Если сборке нужны `Qt6Core5Compat.dll` и `Qt6OpenGL.dll`, скопируй их из `bin` Qt.

Linux: `mkdir build && cd build && qmake ../OpenRGB.pro && make -j`. Ещё нужно правило udev, чтобы пользователь мог открывать
hidraw-устройства (в стандартных правилах OpenRGB этих ID нет), например `/etc/udev/rules.d/61-aula-f108.rules`:

```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0c45", ATTRS{idProduct}=="800a", TAG+="uaccess"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="05ac", ATTRS{idProduct}=="024f", TAG+="uaccess"
```

затем `sudo udevadm control --reload && sudo udevadm trigger`. Сборку под Linux автор не проверял.

## Вариант B. Заменить установленный OpenRGB

Если OpenRGB уже установлен (возможно, как служба Windows), а ты хочешь подменить его своей сборкой, запусти
`openrgb/install_openrgb.ps1` из PowerShell от администратора. Скрипт останавливает службу и процессы, делает резервную копию
папки установки, копирует твою сборку поверх (сохраняя `plugins`, `service_config` и настройки) и запускает службу. Предварительно
поправь параметры в начале скрипта. Откат: `openrgb/restore_openrgb.ps1`.

## Отключи детектор Keychron

В OpenRGB есть детектор Keychron с тем же ID донгла (05AC:024F). Он будет общаться с донглом AULA по другому протоколу. В OpenRGB:
Settings, Supported Devices, отключи "Keychron Gaming Keyboard 1" (или поставь false в `OpenRGB.json`, раздел `Detectors`).

## Плагины и прочее

Это обычная сборка OpenRGB, плагины работают как обычно. Закрой драйвер AULA на время работы OpenRGB: две программы, пишущие в одно
устройство, мешают друг другу.

## Перегенерация раскладки

`AULAF108ProLayout.h` создаётся `openrgb/gen_layout.py` из `openrgb/keymap.json` (соответствие клавиш и индексов светодиодов).
Нужно только если хочешь переименовать клавиши или поменять порядок каналов.
