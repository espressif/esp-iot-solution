set(incs "include"
         "xz-embedded/linux/include/linux")

set(priv_incs "port/include"
              "xz-embedded/linux/lib/xz")

set(srcs "src/xz_decompress.c"
         "xz-embedded/linux/lib/xz/xz_dec_bcj.c"
         "xz-embedded/linux/lib/xz/xz_dec_lzma2.c"
         "xz-embedded/linux/lib/xz/xz_dec_stream.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${incs}
                       PRIV_INCLUDE_DIRS ${priv_incs})
