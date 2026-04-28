idf_component_register(
    SRCS
        "protocol_parser.c"
        "protocol_builder.c"
        "uart_transport.c"
        "motherboard_comm.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        config
    REQUIRES
        freertos
        log
        driver
)
## Phase 5: full comm implementation
