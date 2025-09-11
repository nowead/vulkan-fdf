# cmake/FindStb.cmake

find_path(STB_INCLUDE_DIR
    NAMES stb_image.h stb_truetype.h
    HINTS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES include
    DOC "Path to stb header files"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Stb
    REQUIRED_VARS STB_INCLUDE_DIR
)

if(STB_FOUND AND NOT TARGET Stb::stb)
    add_library(Stb::stb INTERFACE IMPORTED)
    set_target_properties(Stb::stb PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${STB_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(STB_INCLUDE_DIR)