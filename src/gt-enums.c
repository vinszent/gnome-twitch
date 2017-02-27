#include "gt-enums.h"
#include "gt-twitch.h"
#include "gt-settings-dlg.h"

static const GEnumValue gt_settings_dlg_view_enum_values[] =
{
    {GT_SETTINGS_DLG_VIEW_GENERAL, "GT_SETTINGS_DLG_GENERAL", "general"},
    {GT_SETTINGS_DLG_VIEW_PLAYERS, "GT_SETTINGS_DLG_PLUGINS", "plugins"}
};

GType
gt_settings_dlg_view_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtSettingsDlgView", gt_settings_dlg_view_enum_values);

    return type;
}
