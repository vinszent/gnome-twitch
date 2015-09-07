#include <gtk/gtk.h>
#include <gst/gst.h>
#include "gt-app.h"

GtApp* main_app;

int main(int argc, char** argv)
{
    gst_init(0, NULL);

    main_app = gt_app_new();

    return g_application_run(G_APPLICATION(main_app), argc, argv);
}
