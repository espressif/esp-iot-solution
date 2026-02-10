if(NOT EXISTS "${OUT}")
    message(FATAL_ERROR "Error: ${OUT} was not created")
endif()

file(SIZE "${OUT}" SIZE_VAR)
if(SIZE_VAR EQUAL 0)
    message(FATAL_ERROR "Error: ${OUT} is empty")
endif()
