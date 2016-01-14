#include "gt-enums.h"
#include "gt-twitch.h"

static const GEnumValue gt_twitch_stream_quality_enum_values[] = 
{
    {GT_TWITCH_STREAM_QUALITY_SOURCE, "GT_TWITCH_STREAM_QUALITY_SOURCE", "source"},
    {GT_TWITCH_STREAM_QUALITY_HIGH, "GT_TWITCH_STREAM_QUALITY_HIGH", "high"},
    {GT_TWITCH_STREAM_QUALITY_MEDIUM, "GT_TWITCH_STREAM_QUALITY_MEDIUM", "medium"},
    {GT_TWITCH_STREAM_QUALITY_LOW, "GT_TWITCH_STREAM_QUALITY_LOW", "low"},
};

GType
gt_twitch_stream_quality_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtTwitchStreamQuality", gt_twitch_stream_quality_enum_values);

    return type;
}
