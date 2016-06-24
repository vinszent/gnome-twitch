#include "gt-enums.h"
#include "gt-twitch.h"
#include "gt-settings-dlg.h"

static const GEnumValue gt_twitch_stream_quality_enum_values[] =
{
    {GT_TWITCH_STREAM_QUALITY_SOURCE, "GT_TWITCH_STREAM_QUALITY_SOURCE", "source"},
    {GT_TWITCH_STREAM_QUALITY_HIGH, "GT_TWITCH_STREAM_QUALITY_HIGH", "high"},
    {GT_TWITCH_STREAM_QUALITY_MEDIUM, "GT_TWITCH_STREAM_QUALITY_MEDIUM", "medium"},
    {GT_TWITCH_STREAM_QUALITY_LOW, "GT_TWITCH_STREAM_QUALITY_LOW", "low"},
    {GT_TWITCH_STREAM_QUALITY_MOBILE, "GT_TWITCH_STREAM_QUALITY_MOBILE", "mobile"},
};

static const GEnumValue gt_settings_dlg_view_enum_values[] =
{
    {GT_SETTINGS_DLG_VIEW_GENERAL, "GT_SETTINGS_DLG_GENERAL", "general"},
    {GT_SETTINGS_DLG_VIEW_PLUGINS, "GT_SETTINGS_DLG_PLUGINS", "plugins"}
};

GType
gt_twitch_stream_quality_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtTwitchStreamQuality", gt_twitch_stream_quality_enum_values);

    return type;
}

GType
gt_settings_dlg_view_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtSettingsDlgView", gt_settings_dlg_view_enum_values);

    return type;
}
