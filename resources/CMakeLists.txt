cmake_minimum_required(VERSION 2.8.9)

project(gmic CXX C)

find_package(PkgConfig)
include(GNUInstallDirs)

# options controlling the build process
option(BUILD_LIB "Build the GMIC shared library" ON)
option(BUILD_LIB_STATIC "Build the GMIC static library" ON)
option(BUILD_CLI "Build the CLI interface" ON)
option(BUILD_PLUGIN "Build the GIMP plug-in" ON)
option(BUILD_MAN "Build the manpage" ON)
option(ENABLE_X "Add support for X11" ON)
option(ENABLE_FFMPEG "Add support for FFMpeg" ON)
option(ENABLE_FFTW "Add support for FFTW" ON)
option(ENABLE_GRAPHICSMAGICK "Add support for GrahicsMagick" ON)
option(ENABLE_JPEG "Add support for handling images in Jpeg format" ON)
option(ENABLE_OPENCV "Add support for OpenCV" ON)
option(ENABLE_OPENEXR "Add support for handling images in EXR format" ON)
option(ENABLE_OPENMP "Add support for parallel processing" ON)
option(ENABLE_PNG "Add support for handling images in PNG format" ON)
option(ENABLE_TIFF "Add support for handling images in Tiff format" ON)
option(ENABLE_ZLIB "Add support for data compression via Zlib" ON)
option(ENABLE_DYNAMIC_LINKING "Dynamically link the binaries to the GMIC shared library" OFF)

# compile flags
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)
set(CMAKE_POSITION_INDEPENDENT_CODE True)

set(COMPILE_FLAGS "-Dgmic_build -Dcimg_use_vt100 -Dgmic_is_parallel -Dcimg_use_abort ")
IF(MINGW)
  SET(COMPILE_FLAGS "${COMPILE_FLAGS} -std=gnu++11")
ELSEIF(APPLE)
   SET(COMPILE_FLAGS "${COMPILE_FLAGS} -mmacosx-version-min=10.8 -std=c++11 -stdlib=libc++ -Wno-error=c++11-narrowing -Wc++11-extensions  -fpermissive")
ELSE(MINGW)
  SET(COMPILE_FLAGS "${COMPILE_FLAGS} -std=gnu++11 -Wno-error=narrowing -fno-ipa-sra -fpermissive")
ENDIF(MINGW)

IF(NOT "${PRERELEASE_TAG}" STREQUAL "")
  SET(COMPILE_FLAGS "${COMPILE_FLAGS} -Dgmic_prerelease=\"${PRERELEASE_TAG}\"")
ENDIF(NOT "${PRERELEASE_TAG}" STREQUAL "")

set(EXTRA_LIBRARIES)
set(CLI_COMPILE_FLAGS)


# OpenMP support
if(ENABLE_OPENMP)
  if(NOT APPLE)
    set(COMPILE_FLAGS "${COMPILE_FLAGS} -fopenmp -Dcimg_use_openmp ")
    set(EXTRA_LIBRARIES "${EXTRA_LIBRARIES} -lgomp")
  endif(NOT APPLE)
endif(ENABLE_OPENMP)


# Zlib support
if(ENABLE_ZLIB)
  find_package (ZLIB)
endif(ENABLE_ZLIB)
if(ZLIB_FOUND)
  set(COMPILE_FLAGS "${COMPILE_FLAGS} -Dcimg_use_zlib ")
  include_directories(${ZLIB_INCLUDE_DIRS})
  link_directories(${ZLIB_LIBRARY_DIRS})
endif(ZLIB_FOUND)


#X11 support
if(ENABLE_X)
  find_package (X11)
endif(ENABLE_X)
if(X11_FOUND)
  set(COMPILE_FLAGS "${COMPILE_FLAGS} -Dcimg_display=1 -Dcimg_appname=\\\"gmic\\\" ")
  include_directories(${X11_INCLUDE_DIR})
  include_directories(${X11_INCLUDE_DIRS})
  link_directories(${X11_LIBRARY_DIR})
  link_directories(${X11_LIBRARY_DIRS})
else(X11_FOUND)
  set(COMPILE_FLAGS "${COMPILE_FLAGS} -Dcimg_display=0 -Dcimg_appname=\\\"gmic\\\" ")
endif(X11_FOUND)
message( "X11_INCLUDE_DIR: " ${X11_INCLUDE_DIR} )
message( "X11_INCLUDE_DIRs: " ${X11_INCLUDE_DIRs} )
message( "X11_LIBRARY_DIR: " ${X11_LIBRARY_DIR} )
message( "X11_LIBRARY_DIRS: " ${X11_LIBRARY_DIRS} )
if(X11_XShm_FOUND)
  set(COMPILE_FLAGS "${COMPILE_FLAGS} -Dcimg_use_xshm ")
endif(X11_XShm_FOUND)


if(ENABLE_FFTW)
  pkg_check_modules(FFTW3 fftw3>=3.0)
endif(ENABLE_FFTW)
if(FFTW3_FOUND)
  set(COMPILE_FLAGS "${COMPILE_FLAGS} -Dcimg_use_fftw3 ")
  include_directories(${FFTW3_INCLUDE_DIRS})
  link_directories(${FFTW3_LIBRARY_DIRS})
  if(APPLE)
    set(COMPILE_FLAGS "${COMPILE_FLAGS} -Dcimg_use_fftw3_singlethread")
  else(APPLE)
    set(EXTRA_LIBRARIES "${EXTRA_LIBRARIES} -lfftw3_threads")
  endif(APPLE)
endif(FFTW3_FOUND)


if(ENABLE_OPENCV)
  pkg_check_modules(OPENCV opencv)
endif(ENABLE_OPENCV)
if(OPENCV_FOUND)
  set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dcimg_use_opencv ")
  include_directories(${OPENCV_INCLUDE_DIRS})
  link_directories(${OPENCV_LIBRARY_DIRS})
endif(OPENCV_FOUND)


if(ENABLE_GRAPHICSMAGICK)
  pkg_check_modules(GRAPHICSMAGICK GraphicsMagick++)
endif(ENABLE_GRAPHICSMAGICK)
if(GRAPHICSMAGICK_FOUND)
  set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dcimg_use_magick ")
  include_directories(${GRAPHICSMAGICK_INCLUDE_DIRS})
  link_directories(${GRAPHICSMAGICK_LIBRARY_DIRS})
endif(GRAPHICSMAGICK_FOUND)


if(ENABLE_TIFF)
  find_package (TIFF)
endif(ENABLE_TIFF)
if(TIFF_FOUND)
  set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dcimg_use_tiff ")
  include_directories(${TIFF_INCLUDE_DIRS})
  link_directories(${TIFF_LIBRARY_DIRS})
endif(TIFF_FOUND)


if(ENABLE_PNG)
  find_package (PNG)
endif(ENABLE_PNG)
if(PNG_FOUND)
  set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dcimg_use_png ")
  include_directories(${PNG_INCLUDE_DIRS})
  link_directories(${PNG_LIBRARY_DIRS})
endif(PNG_FOUND)


if(ENABLE_JPEG)
  find_package (JPEG)
endif(ENABLE_JPEG)
if(JPEG_FOUND)
  set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dcimg_use_jpeg ")
  include_directories(${JPEG_INCLUDE_DIRS})
  link_directories(${JPEG_LIBRARY_DIRS})
endif(JPEG_FOUND)


if(ENABLE_OPENEXR)
  pkg_check_modules(OPENEXR OpenEXR)
endif(ENABLE_OPENEXR)
if(OPENEXR_FOUND)
  set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dcimg_use_openexr ")
  include_directories(${OPENEXR_INCLUDE_DIRS})
  link_directories(${OPENEXR_LIBRARY_DIRS})
endif(OPENEXR_FOUND)


if(ENABLE_DYNAMIC_LINKING)
  if(NOT BUILD_LIB)
    message(FATAL_ERROR "ENABLE_DYNAMIC_LINKING needs BUILD_LIB")
  endif(NOT BUILD_LIB)
  set(CMAKE_SKIP_RPATH TRUE)
endif(ENABLE_DYNAMIC_LINKING)

# CImg.h header
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CImg_download.cmake "file(DOWNLOAD https://github.com/dtschump/CImg/blob/master/CImg.h?raw=true ${CMAKE_SOURCE_DIR}/src/CImg.h)")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/src/CImg.h
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CImg_download.cmake
  #COMMAND ${WGET_EXECUTABLE} https://github.com/dtschump/CImg/blob/master/CImg.h?raw=true -O ${CMAKE_SOURCE_DIR}/src/CImg.h
  COMMAND touch ${CMAKE_SOURCE_DIR}/src/CImg.h
)
add_custom_target(CImg_download ALL DEPENDS ${CMAKE_SOURCE_DIR}/src/CImg.h)


# gmic_stdlib.h header
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gmic_stdlib_download.cmake "file(DOWNLOAD http://gmic.eu/gmic_stdlib.h ${CMAKE_SOURCE_DIR}/src/gmic_stdlib.h)")
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/src/gmic_stdlib.h
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/gmic_stdlib_download.cmake
  #COMMAND ${WGET_EXECUTABLE} http://gmic.eu/gmic_stdlib.h -O ${CMAKE_SOURCE_DIR}/src/gmic_stdlib.h
  COMMAND touch ${CMAKE_SOURCE_DIR}/src/gmic_stdlib.h
)
add_custom_target(gmic_stdlib_download ALL DEPENDS ${CMAKE_SOURCE_DIR}/src/gmic_stdlib.h)


add_custom_target(gmic_extra_headers DEPENDS CImg_download gmic_stdlib_download)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo."
    FORCE)
endif(NOT CMAKE_BUILD_TYPE)
SET(CMAKE_CXX_FLAGS_DEBUG " -O0 -g -ansi -pedantic -Dcimg_verbosity=3 ${COMPILE_FLAGS}")
SET(CMAKE_CXX_FLAGS_RELEASE " -O3 -mtune=generic ${COMPILE_FLAGS}")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O3 -g -mtune=generic ${COMPILE_FLAGS}")


# source files
SET(CLI_Includes ./src/gmic.h) #${CMAKE_BINARY_DIR}/gmic_stdlib.h ${CMAKE_BINARY_DIR}/CImg.h)
SET(CLI_Sources ./src/gmic.cpp)

IF(MINGW)
  IF("${EXTRA_LIBRARIES}" STREQUAL "")
    SET(EXTRA_LIBRARIES "-Wl,--stack,16777216")
  ELSE("${EXTRA_LIBRARIES}" STREQUAL "")
    SET(EXTRA_LIBRARIES "-Wl,--stack,16777216 ${EXTRA_LIBRARIES}")
  ENDIF("${EXTRA_LIBRARIES}" STREQUAL "")
ELSEIF(APPLE)
  IF("${EXTRA_LIBRARIES}" STREQUAL "")
    SET(EXTRA_LIBRARIES "-lpthread")
  ELSE("${EXTRA_LIBRARIES}" STREQUAL "")
    SET(EXTRA_LIBRARIES "-lpthread ${EXTRA_LIBRARIES}")
  ENDIF("${EXTRA_LIBRARIES}" STREQUAL "")
ELSE(MINGW)
  IF("${EXTRA_LIBRARIES}" STREQUAL "")
    SET(EXTRA_LIBRARIES "-lpthread")
  ELSE("${EXTRA_LIBRARIES}" STREQUAL "")
    SET(EXTRA_LIBRARIES "-lpthread ${EXTRA_LIBRARIES}")
  ENDIF("${EXTRA_LIBRARIES}" STREQUAL "")
ENDIF(MINGW)


if(BUILD_LIB)
  add_library(libgmic SHARED ${CLI_Includes} ${CLI_Sources})
  add_dependencies(libgmic gmic_extra_headers)
  set_target_properties(libgmic PROPERTIES COMPILE_FLAGS "${CLI_COMPILE_FLAGS} ")
  set_target_properties(libgmic PROPERTIES SOVERSION "1" OUTPUT_NAME "gmic")
  IF(NOT APPLE)
    set_target_properties(libgmic PROPERTIES LINK_FLAGS "-Wl,-soname,libgmic.so.1")
  ENDIF(NOT APPLE)
  target_link_libraries(libgmic
    ${X11_LIBRARIES}
    ${TIFF_LIBRARIES}
    ${PNG_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${GRAPHICSMAGICK_LIBRARIES}
    ${OPENEXR_LIBRARIES}
    ${OPENCV_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${EXTRA_LIBRARIES}
    )

  INSTALL(TARGETS libgmic LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  INSTALL(FILES src/gmic.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif(BUILD_LIB)


if(BUILD_LIB_STATIC)
  add_library(libgmicstatic STATIC ${CLI_Includes} ${CLI_Sources})
  add_dependencies(libgmicstatic gmic_extra_headers)
  set_target_properties(libgmicstatic PROPERTIES COMPILE_FLAGS "${CLI_COMPILE_FLAGS} ")
  set_target_properties(libgmicstatic PROPERTIES OUTPUT_NAME "gmic")
  target_link_libraries(libgmicstatic
    ${X11_LIBRARIES}
    ${TIFF_LIBRARIES}
    ${PNG_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${GRAPHICSMAGICK_LIBRARIES}
    ${OPENEXR_LIBRARIES}
    ${OPENCV_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${EXTRA_LIBRARIES}
    )

  INSTALL(TARGETS libgmicstatic ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  INSTALL(FILES src/gmic.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif(BUILD_LIB_STATIC)


if(BUILD_CLI)
  add_executable(gmic ${CLI_Includes} ${CLI_Sources})
  add_dependencies(gmic gmic_extra_headers)

  if(ENABLE_DYNAMIC_LINKING)
    add_dependencies(gmic libgmic)
    target_link_libraries(gmic
      "libgmic"
      )
    set(CLI_COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dgmic_dynamic_linking ")
  else(ENABLE_DYNAMIC_LINKING)
    target_link_libraries(gmic
      ${X11_LIBRARIES}
      ${TIFF_LIBRARIES}
      ${PNG_LIBRARIES}
      ${JPEG_LIBRARIES}
      ${GRAPHICSMAGICK_LIBRARIES}
      ${OPENEXR_LIBRARIES}
      ${OPENCV_LIBRARIES}
      ${ZLIB_LIBRARIES}
      ${FFTW3_LIBRARIES}
      ${EXTRA_LIBRARIES}
      )
  endif(ENABLE_DYNAMIC_LINKING)

  set_target_properties(gmic PROPERTIES COMPILE_FLAGS "${CLI_COMPILE_FLAGS} -Dgmic_main ")
  INSTALL(TARGETS gmic RUNTIME DESTINATION bin)
endif(BUILD_CLI)


# GIMP plug-in
if(BUILD_PLUGIN)
  execute_process(COMMAND gimptool-2.0 --gimpplugindir OUTPUT_VARIABLE GIMP2_PLUGIN_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND gimptool-2.0 --libs OUTPUT_VARIABLE GIMP2_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND gimptool-2.0 --cflags OUTPUT_VARIABLE GIMP2_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)

  add_executable(gmic_gimp_gtk ${CLI_Includes} ./src/gmic_gimp_gtk.cpp)
  add_dependencies(gmic_gimp_gtk gmic_extra_headers)
  set_target_properties(gmic_gimp_gtk PROPERTIES COMPILE_FLAGS "${GIMP2_INCLUDE_DIRS} -Dgmic_gui")

  if(ENABLE_DYNAMIC_LINKING)
    add_dependencies(gmic_gimp_gtk libgmic)
    target_link_libraries(gmic_gimp_gtk
      ${GIMP2_LIBRARIES}
      "libgmic"
      )
  else(ENABLE_DYNAMIC_LINKING)
    target_sources(gmic_gimp_gtk PUBLIC ${CLI_Sources})
    target_link_libraries(gmic_gimp_gtk
      ${X11_LIBRARIES}
      ${ZLIB_LIBRARIES}
      ${FFTW3_LIBRARIES}
      ${GIMP2_LIBRARIES}
      ${EXTRA_LIBRARIES}
      )
  endif(ENABLE_DYNAMIC_LINKING)

  if(PLUGIN_INSTALL_PREFIX)
    INSTALL(TARGETS gmic_gimp_gtk RUNTIME DESTINATION ${PLUGIN_INSTALL_PREFIX})
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/resources/gmic_film_cluts.gmz DESTINATION ${PLUGIN_INSTALL_PREFIX})
  else(PLUGIN_INSTALL_PREFIX)
    INSTALL(TARGETS gmic_gimp_gtk RUNTIME DESTINATION ${GIMP2_PLUGIN_DIR}/plug-ins)
    INSTALL(FILES ${CMAKE_SOURCE_DIR}/resources/gmic_film_cluts.gmz DESTINATION ${GIMP2_PLUGIN_DIR}/plug-ins)
  endif(PLUGIN_INSTALL_PREFIX)

endif(BUILD_PLUGIN)

if(BUILD_MAN)
  file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/man)
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/man/gmic.1.gz
    DEPENDS gmic
    COMMAND ${CMAKE_BINARY_DIR}/gmic -v - ${CMAKE_SOURCE_DIR}/src/gmic_stdlib.gmic raw:${CMAKE_SOURCE_DIR}/src/gmic_stdlib.gmic,uchar -__help man 2> ${CMAKE_SOURCE_DIR}/man/gmic.1
    COMMAND gzip -f ${CMAKE_SOURCE_DIR}/man/gmic.1
  )
  add_custom_target(man ALL DEPENDS ${CMAKE_SOURCE_DIR}/man/gmic.1.gz)
  INSTALL(FILES ${CMAKE_SOURCE_DIR}/man/gmic.1.gz DESTINATION ${CMAKE_INSTALL_MANDIR}/man1/)
endif(BUILD_MAN)
