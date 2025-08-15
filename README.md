# Logging Library

Библиотека для логирования с поддержкой:
- разноуровневых логов (info, warn, error, debug)
- вывода в консоль и в файл
- потокобезопасности
- изменения уровня логирования на лету

## Структура
- `src/` — код библиотеки
- `include/` — заголовочные файлы
- `apps/` — примеры приложений (`logger_demo`, `stats_collector`)

## Сборка
```bash
mkdir build && cd build
cmake ..
cmake --build .

