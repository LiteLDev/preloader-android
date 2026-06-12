# Preloader Android

Preloader Android 是 LeviLaunchroid 的 native mod 预加载接口。它给 mod 提供稳定的 C ABI、C++ 便捷封装、输入回调、函数 hook、signature 解析和内存 patch 能力。

这份文档只讲外部接口怎么用：每个页面都会写明作用、头文件、签名、参数、返回值、示例和注意事项。项目内部架构不作为主线内容。

## 最小 native mod

```c
#include <pl/c/Mod.h>

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  (void)mod_info;
}
```

配套的 `manifest.json`：

```json
{
  "type": "preload-native",
  "name": "Example Mod",
  "entry": "libexample.so"
}
```

## 推荐阅读顺序

1. 先看 [快速开始](getting-started.md)，跑通一个最小 mod。
2. 再看 [Mod 入口](api/mod.md)，理解 `PLModInfo` 和 `LeviMod_Load`。
3. 需要输入时看 [Input API](api/input.md)。
4. 需要修改游戏函数时看 [Hook API](api/hook.md) 和 [Signature API](api/signature.md)。
5. 发布前看 [构建](build.md) 和 [兼容性](compatibility.md)。

## C 与 C++ 头文件

- C mod 优先 include `pl/c/*.h`。
- C++ mod 优先 include `pl/cpp/*.hpp`。
- 旧路径 `pl/*.h` 仍保留为兼容包装头。

