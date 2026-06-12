import { defineConfig, type DefaultTheme } from "vitepress";

const repo = "https://github.com/LiteLDev/preloader-android";
const releases = `${repo}/releases`;
const base = process.env.VITEPRESS_BASE ?? "/";

function buildEnglishSidebar(localePrefix: string): DefaultTheme.SidebarItem[] {
  return [
    {
      text: "Quick Start",
      items: [
        {
          text: "Getting Started",
          link: `${localePrefix}guide/getting-started`,
        },
      ],
    },
    {
      text: "API Reference",
      items: [
        { text: "Mod Entry", link: `${localePrefix}api/mod` },
        { text: "Input API", link: `${localePrefix}api/input` },
        { text: "Hook API", link: `${localePrefix}api/hook` },
        { text: "Signature API", link: `${localePrefix}api/signature` },
        { text: "Patch API", link: `${localePrefix}api/patch` },
        {
          text: "Memory Hook Macros",
          link: `${localePrefix}api/memory-hook-macros`,
        },
        {
          text: "Types and Macros",
          link: `${localePrefix}api/types-and-macros`,
        },
      ],
    },
    {
      text: "Build & Compatibility",
      items: [
        { text: "Build", link: `${localePrefix}guide/build` },
        { text: "Compatibility", link: `${localePrefix}guide/compatibility` },
      ],
    },
  ];
}

function buildChineseSidebar(localePrefix: string): DefaultTheme.SidebarItem[] {
  return [
    {
      text: "快速开始",
      items: [
        { text: "快速开始", link: `${localePrefix}guide/getting-started` },
      ],
    },
    {
      text: "API Reference",
      items: [
        { text: "Mod 入口", link: `${localePrefix}api/mod` },
        { text: "Input API", link: `${localePrefix}api/input` },
        { text: "Hook API", link: `${localePrefix}api/hook` },
        { text: "Signature API", link: `${localePrefix}api/signature` },
        { text: "Patch API", link: `${localePrefix}api/patch` },
        {
          text: "Memory Hook 宏",
          link: `${localePrefix}api/memory-hook-macros`,
        },
        { text: "Types 与宏", link: `${localePrefix}api/types-and-macros` },
      ],
    },
    {
      text: "构建与兼容",
      items: [
        { text: "构建", link: `${localePrefix}guide/build` },
        { text: "兼容性", link: `${localePrefix}guide/compatibility` },
      ],
    },
  ];
}

function buildEnglishNav(): DefaultTheme.NavItem[] {
  return [
    { text: "Guide", link: "/guide/getting-started" },
    { text: "API", link: "/api/mod" },
    { text: "Downloads", link: releases },
    { text: "GitHub", link: repo },
  ];
}

function buildChineseNav(): DefaultTheme.NavItem[] {
  return [
    { text: "文档", link: "/zh-CN/guide/getting-started" },
    { text: "API", link: "/zh-CN/api/mod" },
    { text: "下载", link: releases },
    { text: "GitHub", link: repo },
  ];
}

export default defineConfig({
  title: "Preloader Android",
  description: "API documentation for Preloader Android native mod SDK.",
  lang: "en-US",
  base,
  cleanUrls: true,
  lastUpdated: true,
  head: [
    ["link", { rel: "icon", href: `${base}appicon.png` }],
    ["meta", { name: "theme-color", content: "#16a34a" }],
  ],
  themeConfig: {
    logo: "/appicon.png",
    search: {
      provider: "local",
    },
    socialLinks: [{ icon: "github", link: repo }],
    footer: {
      copyright: "Copyright © 2024-2026 LeviMC",
    },
  },
  locales: {
    root: {
      label: "English",
      lang: "en-US",
      link: "/",
      themeConfig: {
        nav: buildEnglishNav(),
        sidebar: {
          "/guide/": buildEnglishSidebar("/"),
          "/api/": buildEnglishSidebar("/"),
        },
        outline: { level: [2, 3] },
        docFooter: { prev: "Previous page", next: "Next page" },
        editLink: {
          pattern: `${repo}/edit/main/docs/:path`,
          text: "Edit this page on GitHub",
        },
        returnToTopLabel: "Back to top",
        sidebarMenuLabel: "Menu",
        darkModeSwitchLabel: "Appearance",
        lightModeSwitchTitle: "Switch to light theme",
        darkModeSwitchTitle: "Switch to dark theme",
      },
    },
    "zh-CN": {
      label: "简体中文",
      lang: "zh-CN",
      link: "/zh-CN/",
      themeConfig: {
        nav: buildChineseNav(),
        sidebar: {
          "/zh-CN/guide/": buildChineseSidebar("/zh-CN/"),
          "/zh-CN/api/": buildChineseSidebar("/zh-CN/"),
        },
        outline: { level: [2, 3] },
        docFooter: { prev: "上一页", next: "下一页" },
        editLink: {
          pattern: `${repo}/edit/main/docs/:path`,
          text: "在 GitHub 上编辑此页",
        },
        returnToTopLabel: "返回顶部",
        sidebarMenuLabel: "菜单",
        darkModeSwitchLabel: "外观",
        lightModeSwitchTitle: "切换到浅色主题",
        darkModeSwitchTitle: "切换到深色主题",
      },
    },
  },
  markdown: {
    lineNumbers: true,
  },
  sitemap: {
    hostname: "https://liteldev.github.io/preloader-android/",
  },
});
