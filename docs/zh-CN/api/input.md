# Input API

## 作用

Input API 允许 mod 注册触摸和键盘事件回调，也可以请求显示或隐藏软键盘。

## 头文件

```c
#include <pl/c/PreloaderInput.h>
```

C++ 也可以使用：

```cpp
#include <pl/cpp/PreloaderInput.hpp>
```

## 类型签名

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

### 作用

返回输入接口表。mod 通过这个接口注册回调或控制软键盘。

### 参数

无。

### 返回值

返回 `PreloaderInput_Interface *`。当前实现返回全局接口表。

### 示例

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

### 作用

注册触摸事件回调。

### 参数

| 参数 | 说明 |
| --- | --- |
| `callback` | 触摸事件回调函数 |

回调参数：

| 参数 | 说明 |
| --- | --- |
| `action` | Android `MotionEvent` action |
| `pointerId` | 指针 ID |
| `x` | 当前指针 X 坐标 |
| `y` | 当前指针 Y 坐标 |

### 返回值

注册函数无返回值。回调返回 `true` 表示消费事件，返回 `false` 表示继续传递。

## RegisterKeyEventCallback

### 作用

注册键盘事件回调。

### 参数

| 参数 | 说明 |
| --- | --- |
| `callback` | 键盘事件回调函数 |

回调参数：

| 参数 | 说明 |
| --- | --- |
| `keyCode` | Android key code |
| `unicodeChar` | Unicode 字符码 |
| `isKeyDown` | `true` 表示按下，`false` 表示抬起 |

### 返回值

注册函数无返回值。回调返回 `true` 表示消费事件。

## ShowKeyboard / HideKeyboard

### 作用

调用当前 Activity 的 `showSoftKeyboard` 或 `hideSoftKeyboard` 方法。

### 参数

无。

### 返回值

无。

### 示例

```c
PreloaderInput_Interface *input = GetPreloaderInput();
input->ShowKeyboard();
input->HideKeyboard();
```

## 注意事项

- 回调列表当前没有注销接口，避免重复注册同一个回调。
- 回调执行时会持有内部 mutex，不要在回调里做长时间阻塞操作。
- 软键盘依赖 Java 层已设置当前 Activity；Activity 不存在时调用会被忽略。
