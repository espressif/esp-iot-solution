# Golden-frame / protocol tests for File Transfer profile.
# Protocol header is public as esp_gmp_ft_proto.h under the FT profile.
# Data-pipe helpers live in profile private_include (unit-tested here).
idf_component_register(
    SRCS "test_ft_protocol.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../profiles/file_transfer/private_include"
    REQUIRES unity esp_gmp
)
