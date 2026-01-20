SET(SOURCES ui_ScreenMain.c
    ui_ScreenSettings.c
    ui_ScreenAlarms.c
    ui_ScreenCharts.c
    ui_ScreenPulseOxi.c
    ui_ScreenLock.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_296721678.c
    ui_img_1007688293.c
    ui_img_1084506651.c
    ui_img_1370137984.c
    ui_img_bebe_icon_png.c
    ui_img_triangulo_abajo_png.c
    ui_img_triangulo_arriba_png.c
    ui_img_gota_png.c
    ui_img_pulse_png.c
    ui_img_chart_png.c
    ui_img_candado_png.c
    ui_img_check_png.c)

add_library(ui ${SOURCES})
