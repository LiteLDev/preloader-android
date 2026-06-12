# Input API

## Purpose

Input API lets mods register touch and keyboard callbacks, and request the soft keyboard to show or hide.

## Headers

```c
#include <pl/c/PreloaderInput.h>
```

C++:

```cpp
#include <pl/cpp/PreloaderInput.hpp>
```

## Signatures

```c
typedef bool (*PreloaderInput_OnTouch_Fn)(int action, int pointerId,
                                          float x, float y);

typedef bool (*PreloaderInput_OnKeyEvent_Fn)(int keyCode,
                                             unsigned int unicodeChar,
                                             bool isKeyDown);

typedef struct PreloaderInput_Interface {
  void (*RegisterTouchCallback)(PreloaderInput_OnTouch_Fn callback);
  void (*RegisterKeyEventCallback)(PreloaderInput_OnKeyEvent_Fn callback);
  void (*ShowKeyboard)(void);
  void (*HideKeyboard)(void);
} PreloaderInput_Interface;

PLAPI PreloaderInput_Interface *GetPreloaderInput(void);
```

## GetPreloaderInput

### Purpose

Returns the input interface table.

### Parameters

None.

### Return Value

Returns `PreloaderInput_Interface *`.

### Example

```c
static bool on_touch(int action, int pointerId, float x, float y) {
  (void)action;
  (void)pointerId;
  (void)x;
  (void)y;
  return false;
}

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  (void)mod_info;

  PreloaderInput_Interface *input = GetPreloaderInput();
  input->RegisterTouchCallback(on_touch);
}
```

## RegisterTouchCallback

### Purpose

Registers a touch event callback.

### Parameters

| Parameter | Description |
| --- | --- |
| `callback` | Touch callback |

Callback parameters:

| Parameter | Description |
| --- | --- |
| `action` | Android `MotionEvent` action |
| `pointerId` | Pointer ID |
| `x` | Pointer X coordinate |
| `y` | Pointer Y coordinate |

### Return Value

The registration function returns nothing. The callback returns `true` to consume the event.

## RegisterKeyEventCallback

### Purpose

Registers a keyboard event callback.

### Parameters

| Parameter | Description |
| --- | --- |
| `callback` | Key event callback |

Callback parameters:

| Parameter | Description |
| --- | --- |
| `keyCode` | Android key code |
| `unicodeChar` | Unicode character code |
| `isKeyDown` | `true` for key down, `false` for key up |

### Return Value

The registration function returns nothing. The callback returns `true` to consume the event.

## ShowKeyboard / HideKeyboard

### Purpose

Calls the current Activity's `showSoftKeyboard` or `hideSoftKeyboard` method.

### Parameters

None.

### Return Value

None.

## Notes

- There is currently no unregister API. Avoid registering the same callback repeatedly.
- Internal mutex is held while callbacks run. Do not block for long periods.
- Soft keyboard calls are ignored when no Activity is available.

