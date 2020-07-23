idf_component_register(SRCS "mcp23017.c" "mcp23017_obj.cpp"
                        INCLUDE_DIRS include
                        REQUIRES i2c_bus)