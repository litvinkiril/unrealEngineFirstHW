# Домашнее задание №1 — основы механики Unreal Engine

Готовый C++/Blueprint-проект на основе **Third Person Template, Unreal Engine 5.8**: спринт, взаимодействие через Dynamic Multicast Delegate, отдельное состояние анимации Sprint и выносливость. Карта, персонаж, ввод и Animation Blueprint сохранены в `Content/`.

## Запуск

Нужны Unreal Engine **5.8** и Visual Studio с компонентами C++ для Unreal Engine и Windows SDK.

1. Склонировать репозиторий.
2. Выполнить `powershell -ExecutionPolicy Bypass -File Scripts/Build.ps1`. Если движок установлен в другом месте, передать `-EngineRoot "D:\Epic Games\UE_5.8"`.
3. Открыть `MechanicsHomework.uproject` и нажать **Play**. По умолчанию открывается `Lvl_ThirdPerson` с настроенным GameMode.

Все Blueprint-ассеты уже настроены. Вручную переподчинять персонажа, создавать Input Actions или собирать графы не нужно. После клонирования выполняется полная сборка C++; папки с готовыми бинарными файлами в Git не входят.

## Управление

| Клавиша | Действие |
|---|---|
| W / A / S / D | Движение |
| Мышь | Камера |
| Пробел | Прыжок |
| Левый Shift, удерживать | Спринт: 900 см/с |
| E | Взаимодействие: `Interaction triggered!` |

Выносливость отображается на экране: максимум 100, расход 25/с, восстановление 20/с. Полного запаса хватает на 4 секунды. Расход идёт во время спринта, включая стояние и прыжок. При нуле скорость возвращается к обычной. Для повторного спринта после восстановления нужно отпустить Shift и нажать снова; удержание клавиши не запускает бег автоматически.

## Выполненные требования

| Требование | Реализация |
|---|---|
| C++-класс от `ACharacter` | `AHomeworkCharacter`; сохранённый `BP_ThirdPersonCharacter` наследуется от него |
| Enhanced Input | `IA_Sprint` и `IA_Interact` в штатном `IMC_Default`; `BindAction` в C++ |
| Спринт | `MaxWalkSpeed`, восстановление исходной скорости, `BlueprintReadOnly bIsSprinting` |
| Dynamic Multicast Delegate | C++ `OnInteraction.Broadcast()`; в BP сохранены Bind Event, `HandleInteraction` и Print String |
| Animation State Machine | Штатный `ABP_Unarmed` наследуется от `UHomeworkAnimInstance`; в Locomotion добавлен Sprint |
| Переходы | Walk / Run → Sprint при `bIsSprinting && ShouldMove`; обратно при отрицании условия |
| Прыжки и камера | Сохранены графы шаблона и внешняя машина Main States для прыжка/падения/приземления |
| Выносливость | C++ Tick, истощение, восстановление, блокировка повтора и экранное отображение |
| Git | `.gitignore` исключает Binaries, Intermediate, DerivedDataCache, Saved и другие генерируемые файлы |

В Sprint используется штатная `MF_Unarmed_Jog_Fwd` с циклическим воспроизведением и скоростью 1.5. Смешивание занимает 0.15 с. `ShouldMove` предотвращает анимацию бега на месте. Во время прыжка позу выбирает внешняя машина Main States.

## Проверка

```powershell
# Быстрые тесты модели выносливости (Python 3 + C++ compiler)
python Tests/run_tests.py

# Сборка и сквозная проверка в Play In Editor
powershell -ExecutionPolicy Bypass -File Scripts/Verify.ps1

# Та же проверка с графикой и снимками экрана в Saved/Screenshots
powershell -ExecutionPolicy Bypass -File Scripts/Verify.ps1 -Render
```

Тест `Homework.Gameplay.EndToEnd` подаёт W, Shift, пробел и E через PlayerController и настоящий Enhanced Input. Проверяются движение, наличие камеры, скорость, реальное состояние AnimBP, прыжок и посадка, подписка Blueprint на делегат, однократное срабатывание при удержании E, истощение, восстановление и повторное нажатие Shift.

Результаты проверки и окружение: [Docs/Verification.md](Docs/Verification.md). Где расположены сохранённые графы: [Docs/BlueprintSetup.md](Docs/BlueprintSetup.md).

GitHub Actions запускает отдельные C++-тесты выносливости. Полная проверка Unreal выполняется локально, поскольку ей требуется установленный движок. Мультиплеер не предусмотрен. Экранные диагностические сообщения рассчитаны на Editor/Development.

## Структура

- `MechanicsHomework.uproject`, `Config/`, `Content/` — готовый проект и ресурсы стандартного шаблона Epic.
- `Plugins/MechanicsHomework/` — игровые C++-механики.
- `Source/HomeworkGame/` — основной игровой модуль.
- `Source/HomeworkEditor/` — воспроизводимая настройка ассетов и сквозной тест; в игровой билд не включается.
- `Scripts/` — сборка и проверка на Windows.
- `Tests/` — независимые тесты модели выносливости.

`HomeworkSetup` — служебный commandlet для воспроизведения настройки из исходного шаблона UE 5.8. Для обычного запуска он не нужен. Без `-Configure` он только сохраняет описание графов в `Saved/AssetInspection.txt`; с `-Configure` настраивает и сохраняет ассеты.
