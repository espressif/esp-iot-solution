set(src_dirs        ./)
set(include_dirs    ./)
set(embed_files   "test.jpg")

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires} EMBED_FILES ${embed_files})
