# Сохранённая настройка Blueprint

Все изменения выполнены и сохранены в `.uasset` для Unreal Engine 5.8. Эта страница служит навигацией для проверки, а не списком оставшихся ручных действий.

## Персонаж

`/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`

Parent Class: `HomeworkCharacter`. Штатные Move, Aim, прыжок, камера, меш и обработка основных Input Actions сохранены.

В Class Defaults назначены `IA_Sprint` и `IA_Interact`. Новые графы взаимодействия расположены ниже штатных графов в EventGraph:

```text
Event BeginPlay → Bind Event to OnInteraction (Self)
                            Event ← HandleInteraction
HandleInteraction → Print String("Interaction triggered!")
```

`Print to Screen` и `Print to Log` включены. Действие E привязано в C++ к `Started`, поэтому удержание клавиши не повторяет событие.

## Ввод

- `/Game/Input/Actions/IA_Sprint`: Boolean, Left Shift.
- `/Game/Input/Actions/IA_Interact`: Boolean, E.
- Оба действия добавлены в `/Game/Input/IMC_Default` без Hold-триггеров.
- Штатный `BP_ThirdPersonPlayerController` добавляет контексты игроку. Существующие WASD, мышь, пробел и поддержка контроллера сохранены.
- В C++ спринт привязан к Started, Completed и Canceled.

## Анимация

`/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`

Это штатный AnimBP шаблона **UE 5.8**. Parent Class: `HomeworkAnimInstance`. Его C++ `NativeUpdateAnimation` читает `bIsSprinting` у персонажа. Существующий EventGraph по-прежнему обновляет скорость, направление, ShouldMove и IsFalling.

Внутренняя машина `Locomotion`:

```text
Idle ↔ Walk / Run ↔ Sprint
```

- Walk / Run → Sprint: `bIsSprinting AND ShouldMove`.
- Sprint → Walk / Run: `NOT (bIsSprinting AND ShouldMove)`.
- Crossfade Duration: 0.15 с.
- Sprint: Sequence Player `MF_Unarmed_Jog_Fwd`, Loop включён, Play Rate = 1.5.

Внешняя машина `Main States` не изменена. Она выбирает Locomotion / Jump / Fall Loop / Land. Поэтому Sprint не перекрывает прыжок; после завершения Land снова обновляется наземное состояние.

## Воспроизводимость

Исходник редакторской настройки: `Source/HomeworkEditor/HomeworkSetupCommandlet.cpp`. Повторный запуск `-run=HomeworkSetup -Configure` не дублирует Input mappings, подписку на делегат или Sprint. Он предназначен для версии штатных ассетов UE 5.8, поставляемой вместе с этим проектом.
