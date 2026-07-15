#include "pl/runtime/ModMenuBridge.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <mutex>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <lunasvg.h>

#include "pl/Logger.hpp"
#include "pl/ModMenu.hpp"
#include "pl/Input.hpp"

namespace pl::runtime {
    namespace {

        constexpr size_t kMaxModuleIdLength = 160;
        constexpr size_t kMaxDisplayNameLength = 160;
        constexpr size_t kMaxDescriptionLength = 1024;
        constexpr size_t kMaxModIdLength = 160;
        constexpr size_t kMaxButtonLabelLength = 32;
        constexpr size_t kMaxConfigStringLength = 2048;
        constexpr size_t kMaxDrawTextLength = 4096;
        constexpr int kMaxConfigCount = 128;
        constexpr int kMaxDrawCommandCount = 4096;
        constexpr int kMaxFontBytes = 8 * 1024 * 1024;
        constexpr int kMaxButtonIconBytes = 4 * 1024 * 1024;
        constexpr int kDefaultRenderedIconSize = 128;
        constexpr int kMaxRenderedIconSize = 512;
        constexpr float kMinButtonWidthScale = 0.6f;
        constexpr float kMaxButtonWidthScale = 4.0f;
        constexpr float kMinButtonHeightScale = 0.6f;
        constexpr float kMaxButtonHeightScale = 2.0f;

        std::vector<RegisteredModule> g_registeredModules;
        std::vector<RegisteredButton> g_registeredButtons;
        std::mutex g_modMenuMutex;
        thread_local std::vector<std::string> g_currentOwnerModIds;
        
        static bool g_keyCallbackRegistered = false;

        void RegisterKeyCallbackIfNeeded() {
            if (g_keyCallbackRegistered) return;
            g_keyCallbackRegistered = true;

            pl::input::registerKeyCallback([](const pl::input::KeyEvent& event) {
                bool consumed = false;
                std::vector<std::function<void()>> callbacksToInvoke;

                {
                    std::lock_guard<std::mutex> lock(g_modMenuMutex);
                    for (auto& mod : g_registeredModules) {
                        if (!mod.enabled) continue;
                        if (!mod.on_keybind) continue;

                        for (auto& cfg : mod.configs) {
                            if (cfg.type == pl::modmenu::ConfigType::Keybind) {
                                try {
                                    if (!cfg.current_value.empty()) {
                                        int key = std::stoi(cfg.current_value);
                                        if (key != 0 && key == event.keyCode) {
                                            auto cb = mod.on_keybind;
                                            std::string mid = mod.module_id;
                                            std::string ckey = cfg.key;
                                            bool isDown = event.isKeyDown;
                                            callbacksToInvoke.push_back([cb, mid, ckey, isDown]() {
                                                cb(mid, ckey, isDown);
                                            });
                                            consumed = true;
                                        }
                                    }
                                } catch (...) {
                                }
                            }
                        }
                    }
                }

                for (auto& cb : callbacksToInvoke) {
                    cb();
                }

                return consumed;
            });
        }

        struct RegisteredFont {
            std::string font_id;
            std::vector<unsigned char> data;
        };
        std::vector<RegisteredFont> g_registeredFonts;

        struct RegisteredImage {
            std::string image_id;
            std::vector<unsigned char> data;
            int width;
            int height;
        };
        std::vector<RegisteredImage> g_registeredImages;

        std::string CurrentOwnerModId() {
            if (g_currentOwnerModIds.empty())
                return {};
            return g_currentOwnerModIds.back();
        }

        bool ReadString(const char *value, size_t maxLength, const char *fieldName,
                        bool required, std::string &out) {
            out.clear();
            if (!value) {
                if (required)
                    preloaderLogger.error("Mod Menu registration missing {}", fieldName);
                return !required;
            }

            const size_t length = strnlen(value, maxLength + 1);
            if (length == 0) {
                if (required)
                    preloaderLogger.error("Mod Menu registration has empty {}", fieldName);
                return !required;
            }
            if (length > maxLength) {
                preloaderLogger.error("Mod Menu registration {} is too long", fieldName);
                return false;
            }

            out.assign(value, length);
            return true;
        }

        bool ReadRequiredString(const char *value, size_t maxLength,
                                const char *fieldName, std::string &out) {
            return ReadString(value, maxLength, fieldName, true, out);
        }

        bool ReadOptionalString(const char *value, size_t maxLength,
                                const char *fieldName, std::string &out) {
            return ReadString(value, maxLength, fieldName, false, out);
        }

        bool ReadString(std::string_view value, size_t maxLength,
                        const char *fieldName, bool required, std::string &out) {
            out.clear();
            if (value.empty()) {
                if (required)
                    preloaderLogger.error("Mod Menu registration has empty {}", fieldName);
                return !required;
            }
            if (value.size() > maxLength) {
                preloaderLogger.error("Mod Menu registration {} is too long", fieldName);
                return false;
            }

            out.assign(value);
            return true;
        }

        bool ReadRequiredString(std::string_view value, size_t maxLength,
                                const char *fieldName, std::string &out) {
            return ReadString(value, maxLength, fieldName, true, out);
        }

        bool ReadOptionalString(std::string_view value, size_t maxLength,
                                const char *fieldName, std::string &out) {
            return ReadString(value, maxLength, fieldName, false, out);
        }

        bool IsValidConfigType(pl::modmenu::ConfigType type) {
            switch (type) {
                case pl::modmenu::ConfigType::Toggle:
                case pl::modmenu::ConfigType::SliderInt:
                case pl::modmenu::ConfigType::SliderFloat:
                case pl::modmenu::ConfigType::Radio:
                case pl::modmenu::ConfigType::Color:
                case pl::modmenu::ConfigType::Keybind:
                case pl::modmenu::ConfigType::Text:
                case pl::modmenu::ConfigType::Button:
                    return true;
            }
            return false;
        }

        bool IsValidButtonBehavior(pl::modmenu::ButtonBehavior behavior) {
            switch (behavior) {
                case pl::modmenu::ButtonBehavior::Click:
                case pl::modmenu::ButtonBehavior::Hold:
                case pl::modmenu::ButtonBehavior::Toggle:
                    return true;
            }
            return false;
        }

        bool IsValidButtonStylePreset(pl::modmenu::ButtonStylePreset preset) {
            switch (preset) {
                case pl::modmenu::ButtonStylePreset::Keycap:
                case pl::modmenu::ButtonStylePreset::Accent:
                    return true;
            }
            return false;
        }

        bool IsValidButtonIconFormat(pl::modmenu::ButtonIconFormat format) {
            switch (format) {
                case pl::modmenu::ButtonIconFormat::Auto:
                case pl::modmenu::ButtonIconFormat::Png:
                case pl::modmenu::ButtonIconFormat::Webp:
                case pl::modmenu::ButtonIconFormat::Svg:
                case pl::modmenu::ButtonIconFormat::Resource:
                    return true;
            }
            return false;
        }

        bool IsValidButtonEvent(pl::modmenu::ButtonEvent event) {
            switch (event) {
                case pl::modmenu::ButtonEvent::Click:
                case pl::modmenu::ButtonEvent::Down:
                case pl::modmenu::ButtonEvent::Up:
                case pl::modmenu::ButtonEvent::StateChanged:
                case pl::modmenu::ButtonEvent::Scroll:
                    return true;
            }
            return false;
        }

        bool IsValidDrawCommandType(pl::modmenu::DrawCommandType type) {
            switch (type) {
                case pl::modmenu::DrawCommandType::Text:
                case pl::modmenu::DrawCommandType::Rect:
                case pl::modmenu::DrawCommandType::Line:
                case pl::modmenu::DrawCommandType::RectFilled:
                case pl::modmenu::DrawCommandType::CircleFilled:
                case pl::modmenu::DrawCommandType::TriangleFilled:
                case pl::modmenu::DrawCommandType::Image:
                    return true;
            }
            return false;
        }

        float NormalizeButtonScale(float value, float minValue, float maxValue) {
            if (!std::isfinite(value) || value <= 0.0f)
                return 0.0f;
            return std::clamp(value, minValue, maxValue);
        }

        pl::modmenu::ButtonIconFormat
        InferButtonIconFormat(const std::vector<unsigned char> &data,
                              pl::modmenu::ButtonIconFormat declaredFormat) {
            if (declaredFormat != pl::modmenu::ButtonIconFormat::Auto)
                return declaredFormat;
            static constexpr unsigned char kPngSignature[] = {0x89, 'P', 'N', 'G',
                                                              0x0D, 0x0A, 0x1A, 0x0A};
            if (data.size() >= sizeof(kPngSignature) &&
                std::memcmp(data.data(), kPngSignature, sizeof(kPngSignature)) == 0) {
                return pl::modmenu::ButtonIconFormat::Png;
            }
            if (data.size() >= 12 &&
                std::memcmp(data.data(), "RIFF", 4) == 0 &&
                std::memcmp(data.data() + 8, "WEBP", 4) == 0) {
                return pl::modmenu::ButtonIconFormat::Webp;
            }

            size_t offset = 0;
            while (offset < data.size() &&
                   std::isspace(static_cast<unsigned char>(data[offset]))) {
                ++offset;
            }
            if (offset < data.size() && data[offset] == '<') {
                return pl::modmenu::ButtonIconFormat::Svg;
            }
            return pl::modmenu::ButtonIconFormat::Auto;
        }

        int NormalizeRenderedIconSize(int value) {
            if (value <= 0)
                return kDefaultRenderedIconSize;
            return std::clamp(value, 1, kMaxRenderedIconSize);
        }

        void AppendPngBytes(void *closure, void *data, int size) {
            if (!closure || !data || size <= 0)
                return;
            auto *out = static_cast<std::vector<unsigned char> *>(closure);
            const auto *bytes = static_cast<const unsigned char *>(data);
            out->insert(out->end(), bytes, bytes + size);
        }

        bool RenderSvgIconToPng(const std::vector<unsigned char> &svgData, int width,
                                int height, std::vector<unsigned char> &out) {
            auto document = lunasvg::Document::loadFromData(
                    reinterpret_cast<const char *>(svgData.data()), svgData.size());
            if (!document) {
                preloaderLogger.error("Failed to parse registered SVG button icon");
                return false;
            }

            lunasvg::Bitmap bitmap = document->renderToBitmap(
                    NormalizeRenderedIconSize(width), NormalizeRenderedIconSize(height),
                    0x00000000);
            if (bitmap.isNull()) {
                preloaderLogger.error("Failed to render registered SVG button icon");
                return false;
            }

            out.clear();
            if (!bitmap.writeToPng(AppendPngBytes, &out)) {
                preloaderLogger.error("Failed to encode registered SVG button icon");
                return false;
            }
            return !out.empty();
        }

        bool HasFiniteDrawGeometry(const pl::modmenu::DrawCommand &command) {
            return std::isfinite(command.x) && std::isfinite(command.y) &&
                   std::isfinite(command.w) && std::isfinite(command.h) &&
                   std::isfinite(command.x3) && std::isfinite(command.y3) &&
                   std::isfinite(command.size);
        }

        bool ValidateDrawCommands(std::string_view moduleId,
                                  std::span<const pl::modmenu::DrawCommand> commands) {
            if (commands.size() > kMaxDrawCommandCount) {
                preloaderLogger.error("Rejected draw commands for {}: invalid count {}",
                                      moduleId, commands.size());
                return false;
            }

            for (size_t i = 0; i < commands.size(); ++i) {
                if (!HasFiniteDrawGeometry(commands[i])) {
                    preloaderLogger.error("Rejected draw command {} for {}: non-finite "
                                          "geometry or size",
                                          i, moduleId);
                    return false;
                }
                if (!IsValidDrawCommandType(commands[i].type)) {
                    preloaderLogger.error("Rejected draw command {} for {}: invalid type {}",
                                          i, moduleId, static_cast<int>(commands[i].type));
                    return false;
                }

                std::string unused;
                if (!ReadOptionalString(commands[i].text, kMaxDrawTextLength,
                                        "draw command text", unused) ||
                    !ReadOptionalString(commands[i].fontId, kMaxModuleIdLength,
                                        "draw command font_id", unused) ||
                    !ReadOptionalString(commands[i].imageId, kMaxModuleIdLength,
                                        "draw command image_id", unused)) {
                    preloaderLogger.error("Rejected draw command {} for {}", i, moduleId);
                    return false;
                }
            }
            return true;
        }

        bool RegisterCppModule(const pl::modmenu::ModuleInfo &info) {
            std::string moduleId;
            std::string displayName;
            std::string description;
            std::string modId;
            if (!ReadRequiredString(info.moduleId, kMaxModuleIdLength, "module_id",
                                    moduleId) ||
                !ReadRequiredString(info.displayName, kMaxDisplayNameLength,
                                    "display_name", displayName) ||
                !ReadOptionalString(info.description, kMaxDescriptionLength,
                                    "description", description) ||
                !ReadOptionalString(info.modId, kMaxModIdLength, "mod_id", modId)) {
                return false;
            }

            const std::string ownerModId = CurrentOwnerModId();
            if (!ownerModId.empty()) {
                if (modId.empty()) {
                    modId = ownerModId;
                } else if (modId != ownerModId) {
                    preloaderLogger.error(
                            "Rejected Mod Menu module {}: declared mod_id {} does not match "
                            "owning lifecycle mod {}",
                            moduleId, modId, ownerModId);
                    return false;
                }
            }

            if (info.configs.size() > kMaxConfigCount) {
                preloaderLogger.error("Rejected Mod Menu module {}: invalid config_count "
                                      "{}",
                                      moduleId, info.configs.size());
                return false;
            }

            RegisteredModule entry;
            entry.module_id = std::move(moduleId);
            entry.display_name = std::move(displayName);
            entry.description = std::move(description);
            entry.mod_id = std::move(modId);
            entry.enabled = info.defaultEnabled;
            entry.hide_in_hud_editor = info.hideInHudEditor;
            entry.on_toggle = info.onToggle;
            entry.on_config_changed = info.onConfigChanged;
            entry.on_keybind = info.onKeybind;
            entry.configs.reserve(info.configs.size());

            for (size_t i = 0; i < info.configs.size(); ++i) {
                const auto &src = info.configs[i];
                if (!IsValidConfigType(src.type)) {
                    preloaderLogger.error("Rejected Mod Menu module {}: config {} has "
                                          "invalid type {}",
                                          entry.module_id, i, static_cast<int>(src.type));
                    return false;
                }

                RegisteredModule::ConfigEntry cfg;
                if (!ReadRequiredString(src.key, kMaxConfigStringLength, "config key",
                                        cfg.key) ||
                    !ReadRequiredString(src.displayName, kMaxConfigStringLength,
                                        "config display_name", cfg.display_name) ||
                    !ReadOptionalString(src.defaultValue, kMaxConfigStringLength,
                                        "config default_value", cfg.default_value) ||
                    !ReadOptionalString(src.minValue, kMaxConfigStringLength,
                                        "config min_value", cfg.min_value) ||
                    !ReadOptionalString(src.maxValue, kMaxConfigStringLength,
                                        "config max_value", cfg.max_value) ||
                    !ReadOptionalString(src.dependsOn, kMaxConfigStringLength,
                                        "config depends_on", cfg.depends_on)) {
                    preloaderLogger.error("Rejected Mod Menu module {}: invalid config {}",
                                          entry.module_id, i);
                    return false;
                }
                cfg.type = src.type;
                cfg.current_value = cfg.default_value;
                entry.configs.push_back(std::move(cfg));
            }

            const std::string registeredModuleId = entry.module_id;
            const std::string registeredModId = entry.mod_id;
            const bool registeredEnabled = entry.enabled;

            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (const auto &mod : g_registeredModules) {
                if (mod.module_id == registeredModuleId) {
                    preloaderLogger.error("Rejected duplicate Mod Menu module {}",
                                          registeredModuleId);
                    return false;
                }
            }

            RegisterKeyCallbackIfNeeded();
            g_registeredModules.push_back(std::move(entry));
            for (auto &button : g_registeredButtons) {
                if (button.module_id == registeredModuleId) {
                    button.module_enabled = registeredEnabled;
                    if (button.mod_id.empty())
                        button.mod_id = registeredModId;
                }
            }
            return true;
        }

        void UnregisterModule(const char *module_id) {
            if (!module_id)
                return;
            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            g_registeredModules.erase(
                    std::remove_if(g_registeredModules.begin(), g_registeredModules.end(),
                                   [module_id](const RegisteredModule &m) {
                                       return m.module_id == module_id;
                                   }),
                    g_registeredModules.end());
            g_registeredButtons.erase(
                    std::remove_if(g_registeredButtons.begin(), g_registeredButtons.end(),
                                   [module_id](const RegisteredButton &button) {
                                       return button.module_id == module_id;
                                   }),
                    g_registeredButtons.end());
        }

        void SubmitCppDrawCommands(std::string_view moduleIdView,
                                   std::span<const pl::modmenu::DrawCommand> commands) {
            std::string moduleId;
            if (!ReadRequiredString(moduleIdView, kMaxModuleIdLength, "module_id",
                                    moduleId) ||
                !ValidateDrawCommands(moduleId, commands)) {
                return;
            }

            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (auto &mod : g_registeredModules) {
                if (mod.module_id == moduleId) {
                    mod.draw_commands.clear();
                    if (!commands.empty()) {
                        mod.draw_commands.reserve(commands.size());
                        for (const auto &command : commands) {
                            InternalDrawCommand icmd;
                            icmd.module_id = moduleId;
                            icmd.type = command.type;
                            icmd.x = command.x;
                            icmd.y = command.y;
                            icmd.w = command.w;
                            icmd.h = command.h;
                            icmd.x3 = command.x3;
                            icmd.y3 = command.y3;
                            icmd.color = command.color;
                            icmd.size = command.size;
                            icmd.text = command.text;
                            icmd.font_id = command.fontId;
                            icmd.image_id = command.imageId;
                            mod.draw_commands.push_back(std::move(icmd));
                        }
                    }
                    return;
                }
            }
        }

        bool RegisterCppButton(const pl::modmenu::ButtonInfo &info) {
            pl::modmenu::ButtonStylePreset stylePreset = info.stylePreset;
            if (!IsValidButtonStylePreset(stylePreset)) {
                preloaderLogger.error("Rejected Mod Menu button: invalid style preset {}",
                                      static_cast<int>(stylePreset));
                return false;
            }

            if (!IsValidButtonIconFormat(info.iconFormat)) {
                preloaderLogger.error("Rejected Mod Menu button: invalid icon format {}",
                                      static_cast<int>(info.iconFormat));
                return false;
            }

            if (info.iconData.size() > kMaxButtonIconBytes) {
                preloaderLogger.error("Rejected Mod Menu button: invalid icon size {}",
                                      info.iconData.size());
                return false;
            }
            std::vector<unsigned char> iconData = info.iconData;

            const float resolvedWidthScale = NormalizeButtonScale(
                    info.widthScale, kMinButtonWidthScale, kMaxButtonWidthScale);
            const float resolvedHeightScale =
                    NormalizeButtonScale(info.heightScale, kMinButtonHeightScale,
                                         kMaxButtonHeightScale);

            std::string buttonId;
            std::string moduleId;
            std::string displayName;
            std::string modId;
            std::string label;
            if (!ReadRequiredString(info.buttonId, kMaxModuleIdLength, "button_id",
                                    buttonId) ||
                !ReadRequiredString(info.moduleId, kMaxModuleIdLength, "module_id",
                                    moduleId) ||
                !ReadRequiredString(info.displayName, kMaxDisplayNameLength,
                                    "button display_name", displayName) ||
                !ReadOptionalString(info.modId, kMaxModIdLength, "button mod_id",
                                    modId) ||
                !ReadOptionalString(info.label, kMaxButtonLabelLength, "button label",
                                    label)) {
                return false;
            }

            if (!IsValidButtonBehavior(info.behavior)) {
                preloaderLogger.error("Rejected Mod Menu button {}: invalid behavior {}",
                                      buttonId, static_cast<int>(info.behavior));
                return false;
            }

            const std::string ownerModId = CurrentOwnerModId();
            if (!ownerModId.empty()) {
                if (modId.empty()) {
                    modId = ownerModId;
                } else if (modId != ownerModId) {
                    preloaderLogger.error(
                            "Rejected Mod Menu button {}: declared mod_id {} does not match "
                            "owning lifecycle mod {}",
                            buttonId, modId, ownerModId);
                    return false;
                }
            }

            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (const auto &button : g_registeredButtons) {
                if (button.button_id == buttonId) {
                    preloaderLogger.error("Rejected duplicate Mod Menu button {}",
                                          buttonId);
                    return false;
                }
            }

            bool moduleEnabled = false;
            for (const auto &mod : g_registeredModules) {
                if (mod.module_id == moduleId) {
                    moduleEnabled = mod.enabled;
                    if (modId.empty()) {
                        modId = mod.mod_id;
                    } else if (!mod.mod_id.empty() && modId != mod.mod_id) {
                        preloaderLogger.error(
                                "Rejected Mod Menu button {}: mod_id {} does not match module {} "
                                "owner {}",
                                buttonId, modId, moduleId, mod.mod_id);
                        return false;
                    }
                    break;
                }
            }

            RegisteredButton entry;
            entry.button_id = std::move(buttonId);
            entry.module_id = std::move(moduleId);
            entry.display_name = std::move(displayName);
            entry.mod_id = std::move(modId);
            entry.label = std::move(label);
            entry.android_key_code = info.androidKeyCode;
            entry.behavior = info.behavior;
            entry.default_visible = info.defaultVisible;
            entry.module_enabled = moduleEnabled;
            entry.style_preset = stylePreset;
            entry.normal_bg_color = info.normalBgColor;
            entry.active_bg_color = info.activeBgColor;
            entry.border_color = info.borderColor;
            entry.text_color = info.textColor;
            entry.active_text_color = info.activeTextColor;
            entry.width_scale = resolvedWidthScale;
            entry.height_scale = resolvedHeightScale;
            entry.icon_format = InferButtonIconFormat(iconData, info.iconFormat);
            entry.hide_label_when_icon_present = info.hideLabelWhenIconPresent;
            entry.icon_data = std::move(iconData);
            entry.active_icon_data = info.activeIconData;
            entry.on_event = info.onEvent;
            g_registeredButtons.push_back(std::move(entry));
            return true;
        }

        void UnregisterButton(const char *button_id) {
            if (!button_id)
                return;

            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            g_registeredButtons.erase(
                    std::remove_if(g_registeredButtons.begin(), g_registeredButtons.end(),
                                   [button_id](const RegisteredButton &button) {
                                       return button.button_id == button_id;
                                   }),
                    g_registeredButtons.end());
        }

    } // namespace

    ScopedModMenuOwner::ScopedModMenuOwner(std::string modId) {
        g_currentOwnerModIds.push_back(std::move(modId));
    }

    ScopedModMenuOwner::~ScopedModMenuOwner() {
        if (!g_currentOwnerModIds.empty())
            g_currentOwnerModIds.pop_back();
    }

    int GetRegisteredModuleCount() {
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        return static_cast<int>(g_registeredModules.size());
    }

    bool GetRegisteredModuleInfo(int index, RegisteredModule &out) {
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        if (index < 0 || index >= static_cast<int>(g_registeredModules.size()))
            return false;
        out = g_registeredModules[index];
        return true;
    }

    void ToggleRegisteredModule(const char *module_id, bool enabled) {
        if (!module_id)
            return;
        std::function<void(std::string_view, bool)> callback;
        {
            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (auto &mod : g_registeredModules) {
                if (mod.module_id == module_id) {
                    mod.enabled = enabled;
                    callback = mod.on_toggle;
                    break;
                }
            }
            for (auto &button : g_registeredButtons) {
                if (button.module_id == module_id)
                    button.module_enabled = enabled;
            }
        }
        if (callback)
            callback(module_id, enabled);
    }

    void SetRegisteredModuleConfig(const char *module_id, const char *key,
                                   const char *value) {
        if (!module_id || !key)
            return;
        const char *safeValue = value ? value : "";
        std::function<void(std::string_view, std::string_view, std::string_view)>
                callback;
        {
            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (auto &mod : g_registeredModules) {
                if (mod.module_id == module_id) {
                    for (auto &cfg : mod.configs) {
                        if (cfg.key == key) {
                            cfg.current_value = safeValue;
                            break;
                        }
                    }
                    callback = mod.on_config_changed;
                    break;
                }
            }
        }
        if (callback)
            callback(module_id, key, safeValue);
    }

    void UnregisterModulesForModId(const std::string &modId) {
        if (modId.empty())
            return;

        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        g_registeredModules.erase(
                std::remove_if(g_registeredModules.begin(), g_registeredModules.end(),
                               [&modId](const RegisteredModule &m) {
                                   return m.mod_id == modId;
                               }),
                g_registeredModules.end());
        g_registeredButtons.erase(
                std::remove_if(g_registeredButtons.begin(), g_registeredButtons.end(),
                               [&modId](const RegisteredButton &button) {
                                   return button.mod_id == modId;
                               }),
                g_registeredButtons.end());
    }

    int GetRegisteredButtonCount() {
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        return static_cast<int>(g_registeredButtons.size());
    }

    bool GetRegisteredButtonInfo(int index, RegisteredButton &out) {
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        if (index < 0 || index >= static_cast<int>(g_registeredButtons.size()))
            return false;

        out = g_registeredButtons[index];
        for (const auto &mod : g_registeredModules) {
            if (mod.module_id == out.module_id) {
                out.module_enabled = mod.enabled;
                break;
            }
        }
        return true;
    }

    bool GetRegisteredButtonIconBytes(const char *button_id, int width, int height, bool active,
                                      std::vector<unsigned char> &out) {
        if (!button_id)
            return false;

        std::vector<unsigned char> iconData;
        pl::modmenu::ButtonIconFormat iconFormat =
                pl::modmenu::ButtonIconFormat::Auto;
        {
            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (const auto &button : g_registeredButtons) {
                if (button.button_id == button_id) {
                    iconData = (active && !button.active_icon_data.empty()) ? button.active_icon_data : button.icon_data;
                    iconFormat = button.icon_format;
                    break;
                }
            }
        }

        if (iconData.empty())
            return false;

        iconFormat = InferButtonIconFormat(iconData, iconFormat);
        if (iconFormat == pl::modmenu::ButtonIconFormat::Svg) {
            return RenderSvgIconToPng(iconData, width, height, out);
        }

        out = std::move(iconData);
        return !out.empty();
    }

    void DispatchRegisteredButtonEvent(const char *button_id,
                                       pl::modmenu::ButtonEvent event, float value) {
        if (!button_id || !IsValidButtonEvent(event))
            return;

        std::function<void(std::string_view, pl::modmenu::ButtonEvent, float)>
                callback;
        {
            std::lock_guard<std::mutex> lock(g_modMenuMutex);
            for (const auto &button : g_registeredButtons) {
                if (button.button_id == button_id) {
                    callback = button.on_event;
                    break;
                }
            }
        }
        if (callback)
            callback(button_id, event, value);
    }

    void GetDrawCommands(std::vector<InternalDrawCommand> &out) {
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        size_t commandCount = 0;
        for (const auto &mod : g_registeredModules) {
            if (mod.enabled)
                commandCount += mod.draw_commands.size();
        }
        out.reserve(out.size() + commandCount);
        for (const auto &mod : g_registeredModules) {
            if (mod.enabled && !mod.draw_commands.empty()) {
                out.insert(out.end(), mod.draw_commands.begin(), mod.draw_commands.end());
            }
        }
    }

    bool RegisterFontInternal(const char *font_id, const unsigned char *ttf_data,
                              int ttf_size) {
        std::string fontId;
        if (!ReadRequiredString(font_id, kMaxModuleIdLength, "font_id", fontId) ||
            !ttf_data || ttf_size <= 0 || ttf_size > kMaxFontBytes) {
            preloaderLogger.error("Rejected registered font: invalid input or size "
                                  "{}",
                                  ttf_size);
            return false;
        }
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        for (auto &f : g_registeredFonts) {
            if (f.font_id == fontId)
                return false; // Already registered
        }
        RegisteredFont f;
        f.font_id = std::move(fontId);
        f.data.assign(ttf_data, ttf_data + ttf_size);
        g_registeredFonts.push_back(std::move(f));
        return true;
    }

    bool GetRegisteredFontBytes(const char *font_id,
                                std::vector<unsigned char> &out) {
        if (!font_id)
            return false;
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        for (const auto &f : g_registeredFonts) {
            if (f.font_id == font_id) {
                out = f.data;
                return true;
            }
        }
        return false;
    }

    bool RegisterImageInternal(const char *image_id, const unsigned char *image_data,
                               int width, int height) {
        std::string imageId;
        if (!ReadRequiredString(image_id, kMaxModuleIdLength, "image_id", imageId) ||
            width <= 0 || height <= 0 || !image_data) {
            return false;
        }

        const auto imageWidth = static_cast<size_t>(width);
        const auto imageHeight = static_cast<size_t>(height);
        const auto maxInt = static_cast<size_t>(std::numeric_limits<int>::max());
        if (imageWidth > maxInt / imageHeight) {
            return false;
        }
        const size_t pixelCount = imageWidth * imageHeight;
        if (pixelCount > maxInt / 4) {
            return false;
        }

        const size_t byteCount = pixelCount * 4;
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        for (const auto &image : g_registeredImages) {
            if (image.image_id == imageId) {
                return false;
            }
        }

        RegisteredImage image;
        image.image_id = std::move(imageId);
        image.data.assign(image_data, image_data + byteCount);
        image.width = width;
        image.height = height;
        g_registeredImages.push_back(std::move(image));
        return true;
    }

    bool GetRegisteredImageBytes(const char *image_id,
                                 std::vector<unsigned char> &out, int &width, int &height) {
        if (!image_id) {
            return false;
        }
        std::lock_guard<std::mutex> lock(g_modMenuMutex);
        for (const auto &image : g_registeredImages) {
            if (image.image_id == image_id) {
                out = image.data;
                width = image.width;
                height = image.height;
                return true;
            }
        }
        return false;
    }

    bool RegisterCppFont(std::string_view fontId,
                         std::span<const unsigned char> ttfData) {
        if (ttfData.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        const std::string fontIdString(fontId);
        return RegisterFontInternal(fontIdString.c_str(), ttfData.data(),
                                    static_cast<int>(ttfData.size()));
    }

    bool RegisterCppImage(std::string_view imageId,
                          std::span<const unsigned char> imageData, int width,
                          int height) {
        if (width <= 0 || height <= 0) {
            return false;
        }

        const auto imageWidth = static_cast<size_t>(width);
        const auto imageHeight = static_cast<size_t>(height);
        const auto maxInt = static_cast<size_t>(std::numeric_limits<int>::max());
        if (imageWidth > maxInt / imageHeight) {
            return false;
        }
        const size_t pixelCount = imageWidth * imageHeight;
        if (pixelCount > maxInt / 4) {
            return false;
        }

        const size_t byteCount = pixelCount * 4;
        if (imageData.size() != byteCount) {
            return false;
        }

        const std::string imageIdString(imageId);
        return RegisterImageInternal(imageIdString.c_str(), imageData.data(), width,
                                     height);
    }

} // namespace pl::runtime

namespace pl::modmenu {

    bool registerModule(const ModuleInfo &info) {
        return pl::runtime::RegisterCppModule(info);
    }

    void unregisterModule(std::string_view moduleId) {
        const std::string moduleIdString(moduleId);
        pl::runtime::UnregisterModule(moduleIdString.c_str());
    }

    void setModuleEnabled(std::string_view moduleId, bool enabled) {
        const std::string moduleIdString(moduleId);
        pl::runtime::ToggleRegisteredModule(moduleIdString.c_str(), enabled);
    }

    void submitDrawCommands(std::string_view moduleId,
                            std::span<const DrawCommand> commands) {
        pl::runtime::SubmitCppDrawCommands(moduleId, commands);
    }

    bool registerFont(std::string_view fontId,
                      std::span<const unsigned char> ttfData) {
        return pl::runtime::RegisterCppFont(fontId, ttfData);
    }

    bool registerImage(std::string_view imageId,
                       std::span<const unsigned char> imageData, int width,
                       int height) {
        return pl::runtime::RegisterCppImage(imageId, imageData, width, height);
    }

    bool registerButton(const ButtonInfo &info) {
        return pl::runtime::RegisterCppButton(info);
    }

    void unregisterButton(std::string_view buttonId) {
        const std::string buttonIdString(buttonId);
        pl::runtime::UnregisterButton(buttonIdString.c_str());
    }

} // namespace pl::modmenu
