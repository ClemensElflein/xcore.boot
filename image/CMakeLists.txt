add_custom_command(OUTPUT image.h
        COMMAND xxd -i image.dat > ${CMAKE_CURRENT_BINARY_DIR}/image.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS image.dat
)
add_library(binary_image INTERFACE image.h)
target_include_directories(binary_image INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
