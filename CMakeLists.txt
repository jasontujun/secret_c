cmake_minimum_required(VERSION 3.2)
project(secret)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/png)
set(ZLIB_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/zlib)
find_package(ZLIB REQUIRED)

set(libpng_header
        include/png/png.h
        include/png/pngconf.h
        include/png/pnglibconf.h)

set(secret_header
        include/secret_image.h
        include/secret_hider.h
        include/secret_util.h)

set(secret_src
        src/secret_hider.c
        src/secret_image.c
        src/secret_util.c)

add_executable(test_r
        ${libpng_header}
        ${secret_header}
        ${secret_src}
        test/test_r.c)
target_link_libraries(test_r ${PROJECT_SOURCE_DIR}/lib/libpng16d.dll)

add_executable(test_rw
        ${libpng_header}
        ${secret_header}
        ${secret_src}
        test/test_rw.c)
target_link_libraries(test_rw ${PROJECT_SOURCE_DIR}/lib/libpng16d.dll)