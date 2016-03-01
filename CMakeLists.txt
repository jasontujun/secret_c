cmake_minimum_required(VERSION 3.2)
project(secret)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/png)
include_directories(${PROJECT_SOURCE_DIR}/include/jpeg)
set(ZLIB_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/zlib)
find_package(ZLIB REQUIRED)

option(TEST_META "test meta module" ON)
option(TEST_PNG "test png module" ON)
option(TEST_JPEG "test jpeg module" ON)

set(zlib_header
        include/zlib/zlib.h
        include/zlib/zconf.h)

set(libpng_header
        include/png/png.h
        include/png/pngconf.h
        include/png/pnglibconf.h)

set(libjpeg_header
        include/jpeg/jpeglib.h
        include/jpeg/jconfig.h
        include/jpeg/jmorecfg.h
        include/jpeg/jerror.h)

set(secret_header
        include/secret_struct.h
        include/secret_file.h
        include/secret_codec.h
        include/secret_util.h)

set(secret_base_src
        src/secret_struct.c
        src/secret_codec.c
        src/secret_file.c
        src/secret_util.c)

if(TEST_META)
    add_executable(test_meta
            ${zlib_header}
            ${secret_header}
            src/secret_struct.c
            src/secret_codec.c
            src/secret_util.c
            test/test_meta.c)
    target_link_libraries(test_meta ${ZLIB_LIBRARY})
endif()


if(TEST_PNG)
    add_definitions("-DSUPPORT_FORMAT_PNG")
    remove_definitions("-DSUPPORT_FORMAT_JPEG")
    add_executable(test_png
            ${libpng_header}
            ${zlib_header}
            ${secret_header}
            ${secret_base_src}
            src/secret_file_png.c
            test/test_png.c)
    target_link_libraries(test_png ${PROJECT_SOURCE_DIR}/lib/libpng16d.dll ${ZLIB_LIBRARY})
endif()


if(TEST_JPEG)
    add_definitions("-DSUPPORT_FORMAT_JPEG")
    remove_definitions("-DSUPPORT_FORMAT_PNG")
    add_executable(test_jpeg
            ${libjpeg_header}
            ${zlib_header}
            ${secret_header}
            ${secret_base_src}
            src/secret_file_jpeg.c
            test/test_jpeg.c)
    target_link_libraries(test_jpeg ${PROJECT_SOURCE_DIR}/lib/libjpeg.dll ${ZLIB_LIBRARY})
endif()