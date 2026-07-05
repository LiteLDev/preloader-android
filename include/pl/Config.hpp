#pragma once

/**
 * @file Config.hpp
 * @brief Typed JSON configuration helpers for native mods.
 */

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/pfr.hpp>
#include <boost/pfr/core_name.hpp>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#ifndef PL_CONFIG_NO_RUNTIME
#include "pl/Mod.hpp"
#endif

namespace pl::reflection {

namespace detail {

template <typename T> struct dependentFalse : std::false_type {};

template <typename T> struct isVector : std::false_type {};

template <typename T, typename Allocator>
struct isVector<std::vector<T, Allocator>> : std::true_type {};

template <typename T> struct isOptional : std::false_type {};

template <typename T> struct isOptional<std::optional<T>> : std::true_type {};

template <typename T>
concept StringLike = std::is_same_v<std::remove_cvref_t<T>, std::string> ||
                     std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
                     std::is_same_v<std::remove_cvref_t<T>, const char *>;

template <typename T>
concept Reflectable =
    std::is_aggregate_v<std::remove_cvref_t<T>> && !StringLike<T> &&
    !isVector<std::remove_cvref_t<T>>::value &&
    !isOptional<std::remove_cvref_t<T>>::value &&
    !std::is_enum_v<std::remove_cvref_t<T>>;

template <typename T> struct hasVersion {
  template <typename U>
  static auto test(int)
      -> decltype(std::declval<U>().version, std::true_type{});
  template <typename> static auto test(...) -> std::false_type;
  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
constexpr bool hasIntegralVersion =
    hasVersion<T>::value &&
    std::is_integral_v<
        std::remove_cvref_t<decltype(std::declval<T>().version)>>;

template <typename T> nlohmann::ordered_json serializeValue(const T &value);

template <typename T>
void deserializeValue(T &target, const nlohmann::json &value);

template <typename T> nlohmann::ordered_json serializeObject(const T &value) {
  nlohmann::ordered_json result = nlohmann::ordered_json::object();
  constexpr auto names = boost::pfr::names_as_array<T>();
  boost::pfr::for_each_field(value, [&](const auto &field, std::size_t index) {
    auto name = std::string{names[index]};
    if (!name.empty() && name.front() == '$')
      return;
    auto serialized = serializeValue(field);
    if (!serialized.is_null())
      result[name] = std::move(serialized);
  });
  return result;
}

template <typename T> nlohmann::ordered_json serializeValue(const T &value) {
  using Value = std::remove_cvref_t<T>;

  if constexpr (Reflectable<Value>) {
    return serializeObject(value);
  } else if constexpr (isOptional<Value>::value) {
    if (!value)
      return nullptr;
    return serializeValue(*value);
  } else if constexpr (isVector<Value>::value) {
    nlohmann::ordered_json result = nlohmann::ordered_json::array();
    for (const auto &item : value)
      result.push_back(serializeValue(item));
    return result;
  } else if constexpr (std::is_enum_v<Value>) {
    if (auto name = magic_enum::enum_name(value); !name.empty())
      return std::string{name};
    return static_cast<std::underlying_type_t<Value>>(value);
  } else if constexpr (StringLike<Value>) {
    return std::string{value};
  } else if constexpr (std::is_arithmetic_v<Value>) {
    return value;
  } else {
    static_assert(dependentFalse<Value>::value,
                  "Unsupported reflected config field type");
  }
}

template <typename T>
void deserializeObject(T &target, const nlohmann::json &value) {
  if (!value.is_object())
    return;

  constexpr auto names = boost::pfr::names_as_array<T>();
  boost::pfr::for_each_field(target, [&](auto &field, std::size_t index) {
    auto name = std::string{names[index]};
    if (name.empty() || name.front() == '$' || !value.contains(name))
      return;
    deserializeValue(field, value.at(name));
  });
}

template <typename T>
void deserializeValue(T &target, const nlohmann::json &value) {
  using Value = std::remove_cvref_t<T>;

  try {
    if constexpr (Reflectable<Value>) {
      deserializeObject(target, value);
    } else if constexpr (isOptional<Value>::value) {
      if (value.is_null()) {
        target = std::nullopt;
      } else {
        if (!target)
          target.emplace();
        deserializeValue(*target, value);
      }
    } else if constexpr (isVector<Value>::value) {
      if (!value.is_array())
        return;
      target.clear();
      for (const auto &item : value) {
        typename Value::value_type parsed{};
        deserializeValue(parsed, item);
        target.push_back(std::move(parsed));
      }
    } else if constexpr (std::is_enum_v<Value>) {
      if (value.is_string()) {
        if (auto parsed =
                magic_enum::enum_cast<Value>(value.get<std::string>()))
          target = *parsed;
      } else if (value.is_number_integer()) {
        target = static_cast<Value>(value.get<std::underlying_type_t<Value>>());
      }
    } else if constexpr (std::is_same_v<Value, std::string>) {
      if (value.is_string())
        target = value.get<std::string>();
    } else if constexpr (std::is_same_v<Value, bool>) {
      if (value.is_boolean())
        target = value.get<bool>();
    } else if constexpr (std::is_integral_v<Value>) {
      if (value.is_number_integer())
        target = value.get<Value>();
    } else if constexpr (std::is_floating_point_v<Value>) {
      if (value.is_number())
        target = value.get<Value>();
    } else {
      static_assert(dependentFalse<Value>::value,
                    "Unsupported reflected config field type");
    }
  } catch (...) {
  }
}

} // namespace detail

/**
 * @brief Serializes an aggregate config object to ordered JSON.
 */
template <typename T> nlohmann::ordered_json serialize(const T &value) {
  return detail::serializeValue(value);
}

/**
 * @brief Deserializes JSON into an aggregate config object.
 */
template <typename T>
T deserialize(const nlohmann::json &value, const T &defaults = T{}) {
  auto result = defaults;
  detail::deserializeValue(result, value);
  return result;
}

/**
 * @brief Merges JSON values into an existing aggregate config object.
 */
template <typename T>
void deserializeTo(T &target, const nlohmann::json &value) {
  detail::deserializeValue(target, value);
}

} // namespace pl::reflection

namespace pl::config {

/**
 * @brief Optional JSON schema metadata for a reflected config field.
 */
struct FieldSchema {
  std::string_view title;
  std::string_view description;
  std::optional<double> minimum;
  std::optional<double> maximum;
  bool readOnly = false;
};

/**
 * @brief Schema metadata customization point for typed config structs.
 */
template <typename T> struct Schema {
  static constexpr std::string_view title = {};
  static constexpr std::string_view description = {};

  static constexpr FieldSchema field(std::string_view) { return {}; }
};

namespace detail {

template <typename T> nlohmann::ordered_json schemaForValue(const T &value);

inline void applyFieldSchema(nlohmann::ordered_json &schema,
                             const FieldSchema &metadata) {
  if (!metadata.title.empty())
    schema["title"] = metadata.title;
  if (!metadata.description.empty())
    schema["description"] = metadata.description;
  if (metadata.minimum)
    schema["minimum"] = *metadata.minimum;
  if (metadata.maximum)
    schema["maximum"] = *metadata.maximum;
  if (metadata.readOnly)
    schema["readOnly"] = true;
}

template <typename T> nlohmann::ordered_json objectSchema(const T &defaults) {
  nlohmann::ordered_json result{
      {"type", "object"}, {"properties", nlohmann::ordered_json::object()}};
  constexpr auto names = boost::pfr::names_as_array<T>();
  boost::pfr::for_each_field(
      defaults, [&](const auto &field, std::size_t index) {
        auto name = std::string{names[index]};
        if (name.empty() || name.front() == '$')
          return;
        auto property = schemaForValue(field);
        property["default"] = pl::reflection::serialize(field);
        applyFieldSchema(property, Schema<T>::field(name));
        result["properties"][name] = std::move(property);
      });
  return result;
}

template <typename T> nlohmann::ordered_json schemaForValue(const T &value) {
  using Value = std::remove_cvref_t<T>;

  if constexpr (pl::reflection::detail::Reflectable<Value>) {
    return objectSchema(value);
  } else if constexpr (pl::reflection::detail::isOptional<Value>::value) {
    if (value)
      return schemaForValue(*value);
    return schemaForValue(typename Value::value_type{});
  } else if constexpr (pl::reflection::detail::isVector<Value>::value) {
    auto itemDefaults =
        value.empty() ? typename Value::value_type{} : value.front();
    return {{"type", "array"}, {"items", schemaForValue(itemDefaults)}};
  } else if constexpr (std::is_enum_v<Value>) {
    nlohmann::ordered_json enumValues = nlohmann::ordered_json::array();
    for (auto entry : magic_enum::enum_values<Value>())
      enumValues.push_back(std::string{magic_enum::enum_name(entry)});
    return {{"type", "string"}, {"enum", std::move(enumValues)}};
  } else if constexpr (std::is_same_v<Value, bool>) {
    return {{"type", "boolean"}};
  } else if constexpr (std::is_integral_v<Value>) {
    return {{"type", "integer"}};
  } else if constexpr (std::is_floating_point_v<Value>) {
    return {{"type", "number"}};
  } else if constexpr (std::is_same_v<Value, std::string> ||
                       std::is_same_v<Value, std::string_view>) {
    return {{"type", "string"}};
  } else {
    static_assert(pl::reflection::detail::dependentFalse<Value>::value,
                  "Unsupported reflected config field type");
  }
}

template <typename T>
void logWarning(const std::filesystem::path &path, const T &message) {
#ifndef PL_CONFIG_NO_RUNTIME
  if (const auto mod = pl::mod::NativeMod::current()) {
    mod->getLogger().warn("Config {}: {}", path.string(), message);
    return;
  }
#endif
  (void)path;
  (void)message;
}

} // namespace detail

/**
 * @brief Returns the default config path when no explicit path is
 * provided.
 */
inline std::filesystem::path defaultConfigPath() {
#ifdef PL_CONFIG_NO_RUNTIME
  return {};
#else
  const auto mod = pl::mod::NativeMod::current();
  if (!mod)
    return {};
  return mod->getConfigDir() / "config.json";
#endif
}

/**
 * @brief Returns the default schema path next to the default config file.
 */
inline std::filesystem::path defaultSchemaPath() {
  auto path = defaultConfigPath();
  if (path.empty())
    return {};
  return path.parent_path() / "config.schema.json";
}

inline bool writeConfig(const std::filesystem::path &path,
                        const nlohmann::json &value) {
  if (path.empty())
    return false;

  std::error_code errorCode;
  std::filesystem::create_directories(path.parent_path(), errorCode);
  if (errorCode)
    return false;

  const auto tempPath = path.string() + ".tmp";
  {
    std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
    if (!stream)
      return false;

    stream << value.dump(4);
    stream << '\n';
    if (!stream.good())
      return false;
  }

  std::filesystem::rename(tempPath, path, errorCode);
  if (!errorCode)
    return true;

  std::filesystem::remove(path, errorCode);
  errorCode.clear();
  std::filesystem::rename(tempPath, path, errorCode);
  if (errorCode) {
    std::filesystem::remove(tempPath, errorCode);
    return false;
  }
  return true;
}

inline nlohmann::json loadConfig(const std::filesystem::path &path,
                                 const nlohmann::json &defaults) {
  if (path.empty())
    return defaults;

  if (!std::filesystem::exists(path)) {
    writeConfig(path, defaults);
    return defaults;
  }

  try {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
      return defaults;

    nlohmann::json parsed;
    stream >> parsed;
    return parsed;
  } catch (const std::exception &ex) {
    detail::logWarning(path, ex.what());
    return defaults;
  }
}

inline nlohmann::json loadConfig(const nlohmann::json &defaults) {
  return loadConfig(defaultConfigPath(), defaults);
}

inline bool saveConfig(const std::filesystem::path &path,
                       const nlohmann::json &value) {
  return writeConfig(path, value);
}

inline bool saveConfig(const nlohmann::json &value) {
  return saveConfig(defaultConfigPath(), value);
}

/**
 * @brief Concept for aggregate config objects with an integral version field.
 */
template <typename T>
concept TypedConfig = pl::reflection::detail::Reflectable<T> &&
                      pl::reflection::detail::hasIntegralVersion<T>;

template <TypedConfig T> nlohmann::ordered_json schema(const T &defaults = T{});

/**
 * @brief Owns a typed config value and persists JSON plus schema files.
 */
template <TypedConfig T> class ConfigFile {
public:
  explicit ConfigFile(T defaults = T{},
                      std::filesystem::path configPath = defaultConfigPath(),
                      std::filesystem::path schemaPath = defaultSchemaPath())
      : mCurrentValue(std::move(defaults)), mDefaultValue(mCurrentValue),
        mPath(std::move(configPath)), mSchemaFilePath(std::move(schemaPath)) {}

  [[nodiscard]] T &value() { return mCurrentValue; }
  [[nodiscard]] const T &value() const { return mCurrentValue; }
  [[nodiscard]] const std::filesystem::path &configPath() const {
    return mPath;
  }
  [[nodiscard]] const std::filesystem::path &schemaPath() const {
    return mSchemaFilePath;
  }

  bool load() {
    auto defaults = pl::reflection::serialize(mDefaultValue);
    bool rewrite = false;
    nlohmann::ordered_json loaded = defaults;

    if (!mPath.empty() && std::filesystem::exists(mPath)) {
      try {
        std::ifstream stream(mPath, std::ios::binary);
        nlohmann::json parsed;
        stream >> parsed;
        loaded = parsed;
      } catch (const std::exception &ex) {
        detail::logWarning(mPath, ex.what());
        loaded = defaults;
        rewrite = true;
      }
    } else {
      rewrite = true;
    }

    if (!loaded.is_object()) {
      loaded = defaults;
      rewrite = true;
    }

    if (!loaded.contains("version") || !loaded["version"].is_number_integer() ||
        loaded["version"].get<decltype(mDefaultValue.version)>() !=
            mDefaultValue.version) {
      rewrite = true;
    }

    nlohmann::ordered_json merged = defaults;
    merged.merge_patch(loaded);
    merged["version"] = defaults["version"];
    mCurrentValue = pl::reflection::deserialize<T>(merged, mDefaultValue);
    auto normalized = pl::reflection::serialize(mCurrentValue);
    if (normalized != loaded)
      rewrite = true;

    if (rewrite && !save())
      return false;

    return writeSchema();
  }

  bool save() const {
    return writeConfig(mPath, pl::reflection::serialize(mCurrentValue));
  }

  bool writeSchema() const {
    if (mSchemaFilePath.empty())
      return true;
    return writeConfig(mSchemaFilePath, schema(mDefaultValue));
  }

private:
  T mCurrentValue;
  T mDefaultValue;
  std::filesystem::path mPath;
  std::filesystem::path mSchemaFilePath;
};

/**
 * @brief Builds default JSON for a typed config object.
 */
template <TypedConfig T>
nlohmann::ordered_json defaultJson(const T &defaults = T{}) {
  return pl::reflection::serialize(defaults);
}

/**
 * @brief Builds JSON schema for a typed config object.
 */
template <TypedConfig T> nlohmann::ordered_json schema(const T &defaults) {
  auto result = detail::objectSchema(defaults);
  if (!Schema<T>::title.empty())
    result["title"] = Schema<T>::title;
  if (!Schema<T>::description.empty())
    result["description"] = Schema<T>::description;
  return result;
}

/**
 * @brief Loads a typed config from an explicit path.
 */
template <TypedConfig T>
T load(const std::filesystem::path &path, const T &defaults = T{}) {
  ConfigFile<T> file(defaults, path, path.parent_path() / "config.schema.json");
  file.load();
  return file.value();
}

/**
 * @brief Loads a typed config from the default path.
 */
template <TypedConfig T> T load(const T &defaults = T{}) {
  ConfigFile<T> file(defaults);
  file.load();
  return file.value();
}

/**
 * @brief Saves a typed config to an explicit path.
 */
template <TypedConfig T>
bool save(const std::filesystem::path &path, const T &value) {
  return saveConfig(path, pl::reflection::serialize(value));
}

/**
 * @brief Saves a typed config to the default path.
 */
template <TypedConfig T> bool save(const T &value) {
  return save(defaultConfigPath(), value);
}

} // namespace pl::config
