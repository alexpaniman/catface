file(GLOB_RECURSE "source" CONFIGURE_DEPENDS
    src/*.c
)
set_source_files_properties(${source} PROPERTIES LANGUAGE ${CF_LANGUAGE})
add_library(util STATIC ${source})

# setup include directories
target_include_directories(util PUBLIC include)

# link dependencies
target_link_libraries(util m)
