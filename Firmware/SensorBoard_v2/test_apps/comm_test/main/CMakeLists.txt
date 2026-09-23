idf_component_register(
    SRCS "test_main.c" "test_usb_orient.c"
    INCLUDE_DIRS "."
    REQUIRES unity usb_comm
    WHOLE_ARCHIVE  # los TEST_CASE de ficheros sin app_main solo entran al binario con whole-archive
)
