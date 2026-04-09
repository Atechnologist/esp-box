#include "lvgl.h"

void ui_init(void)
{
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "ESP-BOX-S3 HUB");
    lv_obj_center(label);
}
