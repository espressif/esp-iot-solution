set(srcs "test_app_main.c"
         "test_ramfs_boundary.c"
         "test_ramfs_concurrency.c"
         "test_ramfs_error.c"
         "test_ramfs_functional.c"
         "test_ramfs_performance.c"
         "test_ramfs_stability.c"
         "test_ramfs_sync.c")

# In order for the cases defined by `TEST_CASE` to be linked into the final elf,
# the component can be registered as WHOLE_ARCHIVE.
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ramfs esp_timer fatfs unity wear_levelling
                       WHOLE_ARCHIVE)
