find_package(spdlog REQUIRED)
add_library(search SHARED search_tree.cpp)
target_link_libraries(search PUBLIC ta mtl utilities spdlog::spdlog fmt::fmt)
target_include_directories(search PUBLIC include)
