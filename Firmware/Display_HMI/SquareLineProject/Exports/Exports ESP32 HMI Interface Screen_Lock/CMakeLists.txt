SET(SOURCES ui_ScreenLock.c
    ui_ScreenMain.c
    ui_ScreenAlarms.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_candado_png.c
    ui_img_windvector_png.c
    ui_img_bebe_icon_png.c
    ui_img_302897630.c
    ui_img_flecha_png.c
    ui_img_1007688293.c
    ui_img_check_png.c)

add_library(ui ${SOURCES})
