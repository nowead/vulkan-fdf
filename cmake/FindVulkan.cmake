# cmake/FindVulkan.cmake

find_path(VULKAN_INCLUDE_DIR
    NAMES vulkan/vulkan.h
    HINTS ENV VULKAN_SDK
    PATH_SUFFIXES Include
)

find_library(VULKAN_LIBRARY
    NAMES vulkan-1
    HINTS ENV VULKAN_SDK
    PATH_SUFFIXES Lib Lib32
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vulkan
    REQUIRED_VARS VULKAN_LIBRARY VULKAN_INCLUDE_DIR
)

mark_as_advanced(VULKAN_INCLUDE_DIR VULKAN_LIBRARY)

if(VULKAN_FOUND AND NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan::Vulkan UNKNOWN IMPORTED)
    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_LOCATION "${VULKAN_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_INCLUDE_DIR}"
    )
endif()