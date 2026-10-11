#pragma once

#include "classes/editor_export_platform.hpp"
#include "classes/editor_export_plugin.hpp"
#include "variant/packed_string_array.hpp"
using namespace godot;
namespace FmodGodot
{
class FmodExportPlugin : public EditorExportPlugin
{
    GDCLASS(FmodExportPlugin, EditorExportPlugin)

  protected:
    static void _bind_methods()
    {
    }

  private:
    void _add_message(EditorExportPlatform::ExportMessageType p_type, const String &p_message) const;

  public:
    void _export_begin(const PackedStringArray &p_features, bool p_is_debug, const String &p_path,
                       uint32_t p_flags) override;
    void _export_file(const String &p_path, const String &p_type, const PackedStringArray &p_features) override;
    String _get_name() const override;
    bool _supports_platform(const Ref<EditorExportPlatform> &p_platform) const override;
};
} // namespace FmodGodot
