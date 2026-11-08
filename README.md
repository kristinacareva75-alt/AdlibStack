# Adlib Stack Vol 2 — VST3 для обработки эдлибов (в стиле Travis Scott)

Цепочка: Low Cut -> Drive -> 2 питч-голоса (-12 / +12) -> ping-pong delay (SYNC по темпу DAW) -> reverb -> Width -> Dry/Wet

## Как получить плагин без установки компиляторов
1. Создай репозиторий на GitHub и залей туда содержимое этой папки (включая .github).
2. Открой вкладку Actions -> дождись зелёной галочки у "Build VST3".
3. Скачай артефакт для своей ОС (Windows / macOS) внизу страницы запуска.
4. Windows: положи папку "Adlib Stack.vst3" в C:\Program Files\Common Files\VST3
   macOS: положи в ~/Library/Audio/Plug-Ins/VST3 (если Gatekeeper ругается: xattr -cr "Adlib Stack.vst3")
5. Перескань плагины в DAW.

## Локальная сборка
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
