#include "fmod_export_plugin.h"
#include "classes/dir_access.hpp"
#include "classes/editor_export_platform.hpp"
#include "classes/file_access.hpp"
#include "classes/project_settings.hpp"
#include "fmod_audio_server.h"
#include "fmod_string_names.h"
#include "variant/dictionary.hpp"
#include "variant/utility_functions.hpp"
#include <algorithm>
#include <vector>
using namespace godot;
using namespace FmodGodot;

namespace
{
// Godot architecture for an Android ABI folder name ("arm64-v8a" or "arm64" gives "arm64"), empty for other names.
String android_arch_of_folder(const String &p_folder)
{
    static const char *abis[][2] = {
        {"armeabi-v7a", "arm32"}, {"arm64-v8a", "arm64"}, {"x86", "x86_32"}, {"x86_64", "x86_64"}};
    for (const auto &abi : abis)
    {
        if (p_folder == abi[0] || p_folder == abi[1])
        {
            return abi[1];
        }
    }
    return String();
}

// Multiple Platform Build: banks are in <bank_directory>/<platform folder>/. The base platform folder holds the
// banks the game uses; every other folder is named after '.'-separated feature tags.
struct BankLayout
{
    bool valid = false;         // Multiple Platform Build and the base platform folder exists.
    String bank_directory;      // e.g. "res://banks".
    String base_folder;         // e.g. "pc".
    PackedStringArray matching; // Other platform folders whose tags are all export features, most tags first.
};

int64_t count_tags(const String &p_folder)
{
    return p_folder.split(".", false).size();
}

BankLayout get_bank_layout(const PackedStringArray &p_features)
{
    ProjectSettings *settings = ProjectSettings::get_singleton();
    BankLayout layout;
    if (int(settings->get_setting_with_override_and_custom_features(SOURCE_TYPE, p_features)) != 1)
    {
        return layout;
    }
    layout.bank_directory =
        String(settings->get_setting_with_override_and_custom_features(BANK_DIRECTORY, p_features)).simplify_path();
    layout.base_folder = settings->get_setting_with_override_and_custom_features(BASE_PLATFORM_FOLDER, p_features);
    if (layout.base_folder.is_empty() || !DirAccess::dir_exists_absolute(layout.bank_directory))
    {
        return layout;
    }
    PackedStringArray folders = DirAccess::get_directories_at(layout.bank_directory);
    layout.valid = folders.has(layout.base_folder);
    std::vector<String> matching;
    for (const String &folder : folders)
    {
        PackedStringArray tags = folder.split(".", false);
        bool matches = folder != layout.base_folder && !tags.is_empty();
        for (const String &tag : tags)
        {
            matches = matches && p_features.has(tag);
        }
        if (matches)
        {
            matching.push_back(folder);
        }
    }
    // Most tags first ("web.nothreads" before "web"), then by name so the choice never depends on the file system.
    std::sort(matching.begin(), matching.end(), [](const String &a, const String &b) {
        int64_t a_tags = count_tags(a);
        int64_t b_tags = count_tags(b);
        return a_tags != b_tags ? a_tags > b_tags : a < b;
    });
    for (const String &folder : matching)
    {
        layout.matching.push_back(folder);
    }
    return layout;
}
} // namespace

void FmodExportPlugin::_add_message(EditorExportPlatform::ExportMessageType p_type, const String &p_message) const
{
    Ref<EditorExportPlatform> platform = get_export_platform();
    if (platform.is_valid())
    {
        platform->add_message(p_type, "FMOD", p_message);
    }
    else
    {
        UtilityFunctions::push_warning("FMOD: ", p_message);
    }
}

void FmodExportPlugin::_export_begin(const PackedStringArray &p_features, bool p_is_debug, const String &p_path,
                                     uint32_t p_flags)
{
    ProjectSettings *settings = ProjectSettings::get_singleton();
    if (int(settings->get_setting_with_override_and_custom_features(SOURCE_TYPE, p_features)) == 1 &&
        !get_bank_layout(p_features).valid)
    {
        _add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING,
                     vformat("Multiple Platform Build: the base platform folder \"%s\" was not found in \"%s\", so "
                             "banks were exported unchanged. Set \"%s\" to the folder that holds the banks the "
                             "editor uses.",
                             settings->get_setting_with_override_and_custom_features(BASE_PLATFORM_FOLDER, p_features),
                             settings->get_setting_with_override_and_custom_features(BANK_DIRECTORY, p_features),
                             BASE_PLATFORM_FOLDER));
    }

    String plugin_path = settings->get_setting_with_override_and_custom_features(PLUGIN_PATH, p_features);
    Dictionary plugins = settings->get_setting_with_override_and_custom_features(PLUGINS, p_features);
    if (p_features.has("web"))
    {
        if (!plugins.is_empty())
        {
            _add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING,
                         vformat("FMOD plugins (%s) are not supported on Web and were not exported. "
                                 "Set \"%s.web\" to an empty Dictionary to silence this warning.",
                                 PLUGINS, PLUGINS));
        }
        if (p_features.has("web_noextensions"))
        {
            _add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR,
                         "FMOD requires GDExtension support on Web. "
                         "Enable Variant > Extensions Support in the export preset.");
        }
        if (p_features.has("threads"))
        {
            _add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR,
                         "FMOD for Web only supports exports without threads. "
                         "Disable Variant > Thread Support in the export preset.");
        }
        // The web exporter loads every shared object as a wasm side module, so native plugins would break startup.
        return;
    }
    Dictionary exported_files; // File name (per ABI on Android) -> key, the copies would overwrite each other.
    for (const Variant &plugin : plugins.keys())
    {
        String path = FmodAudioServer::_get_plugin_project_path(plugin, plugin_path);
        if (!FileAccess::file_exists(path))
        {
            _add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING,
                         vformat("FMOD plugin \"%s\" was not found at \"%s\" and was not exported.", plugin, path));
            continue;
        }
        PackedStringArray tags;
        if (p_features.has("android"))
        {
            // Android stores native libraries per ABI, so the library must be in a folder named after its ABI,
            // like in FMOD's Android SDK (lib/arm64-v8a/libresonanceaudio.so).
            String arch = android_arch_of_folder(path.get_base_dir().get_file());
            if (arch.is_empty() || !path.get_file().begins_with("lib"))
            {
                _add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING,
                             vformat("FMOD plugin \"%s\" was not exported. On Android, the file name must start with "
                                     "\"lib\" and the folder must be named after its ABI (arm64-v8a, armeabi-v7a, "
                                     "x86_64 or x86).",
                                     plugin));
                continue;
            }
            if (!p_features.has(arch))
            {
                continue; // This ABI is not part of the export.
            }
            tags.push_back(arch);
        }
        String exported_file = tags.is_empty() ? path.get_file() : tags[0] + "/" + path.get_file();
        if (exported_files.has(exported_file))
        {
            _add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING,
                         vformat("FMOD plugins \"%s\" and \"%s\" have the same file name, only \"%s\" was "
                                 "exported. Use feature overrides to list one library per platform.",
                                 exported_files[exported_file], plugin, exported_files[exported_file]));
            continue;
        }
        exported_files[exported_file] = plugin;
        // No target: the library goes where the platform keeps native libraries, next to the executable on
        // Windows and Linux, in Contents/Frameworks on macOS and in lib/<abi> on Android.
        // FmodAudioServer::_get_plugin_load_path() loads it from there.
        add_shared_object(path, tags, String());
    }
}

// Multiple Platform Build: each bank of the base platform folder is replaced by the same bank (same path relative
// to the platform folder) from the most specific platform folder whose tags all match the export. Banks are stored
// once, at their base path, so the game only ever loads the base platform folder.
void FmodExportPlugin::_export_file(const String &p_path, const String &p_type, const PackedStringArray &p_features)
{
    if (p_type != "FmodBank")
    {
        return;
    }
    BankLayout layout = get_bank_layout(p_features);
    if (!layout.valid)
    {
        return; // Single Platform Build, or no base platform folder (reported in _export_begin).
    }
    String prefix = layout.bank_directory.ends_with("/") ? layout.bank_directory : layout.bank_directory + "/";
    if (!p_path.begins_with(prefix))
    {
        return; // Outside the bank directory: exported unchanged.
    }
    String relative = p_path.substr(prefix.length());
    int64_t slash = relative.find("/");
    if (slash < 0)
    {
        return; // Directly in the bank directory: exported unchanged.
    }
    String folder = relative.substr(0, slash);
    String bank = relative.substr(slash + 1);
    if (folder == layout.base_folder)
    {
        for (const String &candidate : layout.matching)
        {
            String source = layout.bank_directory.path_join(candidate).path_join(bank);
            if (!FileAccess::file_exists(source))
            {
                continue;
            }
            PackedByteArray data = FileAccess::get_file_as_bytes(source);
            if (data.is_empty())
            {
                _add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING,
                             vformat("Multiple Platform Build: could not read \"%s\", \"%s\" was exported unchanged.",
                                     source, p_path));
                return;
            }
            add_file(p_path, data, false);
            skip();
            return;
        }
        return; // No platform-specific version: the base bank is exported unchanged.
    }
    if (!layout.matching.has(folder) ||
        FileAccess::file_exists(layout.bank_directory.path_join(layout.base_folder).path_join(bank)))
    {
        skip(); // Another platform's bank, or one already stored at its base path.
    }
    // Otherwise this platform's bank has no base version and is exported unchanged at its own path.
}

String FmodExportPlugin::_get_name() const
{
    return "Fmod Export Plugin";
}

bool FmodExportPlugin::_supports_platform(const Ref<EditorExportPlatform> &p_platform) const
{
    return true;
}
