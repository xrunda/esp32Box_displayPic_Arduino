idf_component_register(
    SRCS
        "mcp_client.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        tcp_transport
        esp-tls
        json
        mbedtls
    REQUIRES
        freertos
)

