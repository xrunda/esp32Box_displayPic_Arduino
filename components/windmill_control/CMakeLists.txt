idf_component_register(
    SRCS
        "windmill_control.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        esp_driver_gpio
        json
    REQUIRES
        mcp_client
)

