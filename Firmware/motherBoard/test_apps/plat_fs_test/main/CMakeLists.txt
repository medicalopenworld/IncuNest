idf_component_register(
    SRCS "test_main.c" "test_plat_fs.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity incunest_platform
    WHOLE_ARCHIVE  # los TEST_CASE de ficheros sin app_main solo entran al binario con whole-archive
)
