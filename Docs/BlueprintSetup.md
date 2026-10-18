# Настройка Blueprint-ассетов

Статус: эти изменения описаны, но ещё не выполнены и не сохранены в `.uasset`. Они требуют установленного Unreal Editor и исходного Third Person Blueprint проекта. Имена и пути стандартных ассетов могут отличаться между версиями UE.

## 1. Enhanced Input

В каталоге рядом со стандартными Input Action создать:

| Ассет | Value Type | Клавиша в стандартном IMC |
|---|---|---|
| `IA_Sprint` | Digital (bool) | Left Shift |
| `IA_Interact` | Digital (bool) | E |

Для этих действий оставить Triggers и Modifiers пустыми. Отдельный Hold-триггер не нужен: C++ начинает действие по `Started` и завершает спринт по `Completed` или `Canceled`.

Открыть **тот Mapping Context, который уже активирует шаблон**, обычно `IMC_Default`. Добавить новые действия и клавиши, сохранив существующие WASD, мышь и прыжок. Не создавать второй независимый контекст вместо контекста шаблона.

В Class Defaults `BP_ThirdPersonCharacter` после смены родителя назначить:

- Homework → Input → Sprint Action: `IA_Sprint`.
- Homework → Input → Interaction Action: `IA_Interact`.

Проверить, что штатный граф шаблона добавляет этот Mapping Context локальному игроку. Дополнительные обработчики Shift/E в Event Graph не нужны: их связывает `SetupPlayerInputComponent` в C++.

## 2. Подписка на делегат в персонаже

В Event Graph `BP_ThirdPersonCharacter`:

1. В существующей цепочке `Event BeginPlay` после настройки стандартного ввода добавить `Bind Event to On Interaction`, Target = Self. Если удобнее, поставить Sequence и сохранить старую цепочку в Then 0, новую подписку в Then 1.
2. Из красного пина Event создать Custom Event `HandleInteraction` без параметров.
3. Из белого выхода `HandleInteraction` вызвать `Print String` с точным текстом **`Interaction triggered!`** и включённым Print to Screen.
4. Compile и Save.

```text
Event BeginPlay → штатная инициализация → Bind Event to On Interaction (Self)
                                            Event ← HandleInteraction
HandleInteraction → Print String("Interaction triggered!")
```

При удержании E событие должно срабатывать один раз. Не заменять подписку прямым `Print String` в обработчике ввода: по условию сообщение приходит через Dynamic Multicast Delegate.

## 3. Animation Blueprint

У стандартного манекена Quinn может быть дочерний `ABP_Quinn`, наследующий графы от `ABP_Manny`. Изменять нужно **родительский AnimBP, который действительно содержит Locomotion State Machine**. Не менять родителя дочернего `ABP_Quinn`: это оторвёт его от графа родителя.

1. Открыть корневой AnimBP (обычно `ABP_Manny`) и проверить, что его текущий родитель — `AnimInstance`. Поменять его на `HomeworkAnimInstance`. Если текущий родитель другой и содержит важную логику, вместо переподчинения читать `bIsSprinting` через `Try Get Pawn Owner → Cast to HomeworkCharacter` в существующем Event Blueprint Update Animation.
2. Сохранить существующий Event Graph обновления скорости, направления и падения. Не создавать второй Event Blueprint Update Animation.
3. У `HomeworkAnimInstance` переменная `bIsSprinting` уже обновляется из C++-персонажа каждый кадр. Включить Show Inherited Variables, чтобы увидеть её; в интерфейсе она может отображаться как `Is Sprinting`.
4. В существующей машине Locomotion добавить состояние `Sprint`.
5. Внутри Sprint разместить Sequence Player со стандартной анимацией бега **для того же скелета**, что и текущий манекен. Например, выбрать имеющийся в проекте `MM_Run_Fwd` для Manny; точное имя проверить в Content Browser. Loop Animation включить. Соединить выход позы с Output Animation Pose. Не использовать анимацию другого скелета без ретаргетинга.
6. Соединить обычное наземное состояние движения (`Idle/Run`, `Locomotion` либо аналог в шаблоне) со Sprint. Правило входа: `bIsSprinting AND NOT IsFalling`. Правило возврата: `NOT bIsSprinting`.
7. Сохранить прыжки: продублировать существующие переходы из обычного наземного состояния в состояния прыжка/падения также для Sprint, с теми же условиями. При посадке можно возвращаться через штатное наземное состояние: если Shift всё ещё активен и выносливость есть, следующий переход снова направит в Sprint.
8. Для переходов обычное движение ↔ Sprint установить Crossfade Duration примерно 0,15 с. Compile и Save для корневого AnimBP и его дочерних Blueprint.

Это добавляет требуемое отдельное состояние Sprint и сохраняет стандартные прыжки. Условие `NOT IsFalling` защищает наземную анимацию от включения во время прыжка.

## 4. Сохранение и сдача

Сохранить персонажа, два Input Action, изменённый Mapping Context и AnimBP. Проверить в PIE, затем закрыть редактор и добавить `.uproject`, `Config`, `Content`, `Source`, `Plugins` в Git. Не добавлять `Binaries`, `DerivedDataCache`, `Intermediate` и `Saved`.
