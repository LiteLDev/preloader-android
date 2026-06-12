---
layout: home

hero:
  name: Preloader Android
  text: LeviLaunchroid 的 native mod 预加载 API
  tagline: 使用稳定 C ABI、C++ 封装、输入回调、函数 hook、signature 查找和内存 patch 编写 preload-native mod。
  image:
    src: /appicon.png
    alt: Preloader Android
  actions:
    - theme: brand
      text: 快速开始
      link: /zh-CN/guide/getting-started
    - theme: alt
      text: API 参考
      link: /zh-CN/api/mod

features:
  - title: 稳定 C ABI
    details: 使用小而稳定的 C 头文件声明 mod 入口、输入回调、hook 与 signature 查找。
  - title: C++ 便捷封装
    details: C++ mod 可以使用类型封装和辅助宏，减少直接处理 C ABI 的重复代码。
  - title: 运行时输入桥
    details: 注册触摸与按键回调，并在需要时请求 Android 软键盘。
  - title: Hook 工作流
    details: 解析目标地址、安装带优先级的 detour，并保留 original 调用能力。
  - title: Signature 查找
    details: 在已加载模块中解析符号或扫描字节 pattern。
  - title: Patch 工具
    details: 对已知地址写入、读取和回滚命名内存 patch。
---

::: tip Need English documentation?
Use the language selector or open [English](/).
:::

## 本站覆盖内容

本文档面向使用 Preloader Android 的 native mod 开发者。

- 编写 `manifest.json` 并导出 `LeviMod_Load`。
- 安全读取 `PLModInfo` 元数据。
- 注册触摸与按键回调。
- Hook 函数并调用 original。
- 解析符号或字节 signature。
- 写入并回滚内存 patch。

## 最小 native mod

```c
#include <pl/c/Mod.h>

void LeviMod_Load(JavaVM *vm, const PLModInfo *mod_info) {
  (void)vm;
  (void)mod_info;
}
```

## 推荐阅读顺序

1. [快速开始](/zh-CN/guide/getting-started)
2. [Mod 入口 API](/zh-CN/api/mod)
3. [Input API](/zh-CN/api/input)
4. [Hook API](/zh-CN/api/hook)
5. [构建](/zh-CN/guide/build)
