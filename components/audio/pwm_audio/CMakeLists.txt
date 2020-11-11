
set(pwm_audio_srcs "pwm_audio.c")

idf_component_register(SRCS "${pwm_audio_srcs}"
                       INCLUDE_DIRS "include")
