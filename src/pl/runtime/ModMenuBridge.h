#pragma once

#include "pl/ModMenu.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace pl::runtime {

    struct InternalDrawCommand {
        std::string module_id;
        pl::modmenu::DrawCommandType type;
        float x, y, w, h;
        float x3, y3;
        uint32_t color;
        float size;
        std::string text;
        std::string font_id;
        std::string image_id;
    };

    struct RegisteredModule {
        std::string module_id;
        std::string display_name;
        std::string description;
        std::string mod_id;
        bool enabled;
        bool hide_in_hud_editor;
        std::function<void(std::string_view moduleId, bool enabled)> on_toggle;
        std::function<void(std::string_view moduleId, std::string_view key,
                std::string_view value)>
        on_config_changed;
        std::function<void(std::string_view moduleId, std::string_view key,
                bool isDown)>
        on_keybind;

        struct ConfigEntry {
            std::string key;
            std::string display_name;
            pl::modmenu::ConfigType type;
            std::string default_value;
            std::string min_value;
            std::string max_value;
            std::string current_value;
            std::string depends_on;
        };
        std::vector<ConfigEntry> configs;
        std::vector<InternalDrawCommand> draw_commands;
    };

    struct RegisteredButton {
        std::string button_id;
        std::string module_id;
        std::string display_name;
        std::string mod_id;
        std::string label;
        int android_key_code;
        pl::modmenu::ButtonBehavior behavior;
        bool default_visible;
        bool module_enabled;
        pl::modmenu::ButtonStylePreset style_preset;
        uint32_t normal_bg_color;
        uint32_t active_bg_color;
        uint32_t border_color;
        uint32_t text_color;
        uint32_t active_text_color;
        float width_scale;
        float height_scale;
        pl::modmenu::ButtonIconFormat icon_format;
        bool hide_label_when_icon_present;
        std::vector<unsigned char> icon_data;
        std::vector<unsigned char> active_icon_data;
        std::function<void(std::string_view buttonId, pl::modmenu::ButtonEvent event,
        float value)>
        on_event;
    };

    class ScopedModMenuOwner {
    public:
        explicit ScopedModMenuOwner(std::string modId);
        ~ScopedModMenuOwner();

        ScopedModMenuOwner(const ScopedModMenuOwner &) = delete;
        ScopedModMenuOwner &operator=(const ScopedModMenuOwner &) = delete;
    };

    int GetRegisteredModuleCount();
    bool GetRegisteredModuleInfo(int index, RegisteredModule &out);
    void ToggleRegisteredModule(const char *module_id, bool enabled);
    void SetRegisteredModuleConfig(const char *module_id, const char *key,
                                   const char *value);
    void UnregisterModulesForModId(const std::string &modId);

    int GetRegisteredButtonCount();
    bool GetRegisteredButtonInfo(int index, RegisteredButton &out);
    bool GetRegisteredButtonIconBytes(const char *button_id, int width, int height, bool active,
                                      std::vector<unsigned char> &out);
    void DispatchRegisteredButtonEvent(const char *button_id,
                                       pl::modmenu::ButtonEvent event, float value);

    void GetDrawCommands(std::vector<InternalDrawCommand> &out);

    bool RegisterFontInternal(const char *font_id, const unsigned char *ttf_data,
                              int ttf_size);
    bool GetRegisteredFontBytes(const char *font_id,
                                std::vector<unsigned char> &out);

    bool RegisterImageInternal(const char *image_id, const unsigned char *image_data,
                               int width, int height);
    bool GetRegisteredImageBytes(const char *image_id,
                                 std::vector<unsigned char> &out, int &width, int &height);

} // namespace pl::runtime
