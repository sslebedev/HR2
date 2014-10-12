cmake_minimum_required(VERSION 3.0.0)
MESSAGE(STATUS "VERSION: 0.00.14")
# Project settings
set(PROJECT_NAME HR2)
set(APP_NAME HR2)
project(${PROJECT_NAME})

#-------------------------------------------------------------------------------------------
 
# OS settings
if(WIN32 OR APPLE)
  set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} CACHE STRING "" FORCE)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
elseif (UNIX AND NOT APPLE)
  message(SEND_ERROR "You can't do that")
endif()

#-------------------------------------------------------------------------------------------
 
# Compiler settings
if (MSVC)
  add_definitions(
  /DWIN32 /D_DEBUG /D_CONSOLE /D_MBCS
  /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE
  )
  set(CMAKE_CXX_FLAGS "/EHsc /W4 /nologo /Od /Ob0" CACHE STRING "" FORCE)
elseif (CMAKE_COMPILER_IS_GNUCXX)
  message(SEND_ERROR "You can't do that")
endif ()

#-------------------------------------------------------------------------------------------
 
#
## Adding the library CGNS
#set(CGNS_PATH "C:/SDK/cgns")
#include_directories(${CGNS_PATH}/include/) # include headers
#link_directories(${CGNS_PATH}/lib/) # include lib
#link_libraries(gdiplus.lib)
# 
## Adding the library PISTON
#include_directories("C:/SDK/PISTON") # include headers
# 
## Adding the packages CUDA
#find_package(CUDA QUIET REQUIRED)
#if(CUDA_FOUND)
#  set(CUDA_NVCC_FLAGS -gencode arch=compute_30,code=sm_30 CACHE STRING "" FORCE)
#  set(CUDA_NVCC_FLAGS_DEBUG -G CACHE STRING "" FORCE)
#endif()

#-------------------------------------------------------------------------------------------
 
# Adding the source code
set(SOURCE_PATH ${PROJECT_SOURCE_DIR}/src)
 
set(SOURCE_MAIN ${SOURCE_PATH}/win/main.cpp)

set(HEADER_UNIVERSAL ${SOURCE_PATH}/universal/xml.h
  ${SOURCE_PATH}/universal/utl.h
  ${SOURCE_PATH}/universal/raster.h
  ${SOURCE_PATH}/universal/pline.h)
set(SOURCE_UNIVERSAL ${SOURCE_PATH}/universal/xml.c
  ${SOURCE_PATH}/universal/raster.c
  ${SOURCE_PATH}/universal/pline.c)

#-------------------------------------------------------------------------------------------

#MSVC Filters
IF (MSVC)
  source_group("Source Files\\Win" FILES ${SOURCE_MAIN})

  source_group("Source Files\\Universal" FILES ${HEADER_UNIVERSAL})
  source_group("Source Files\\Universal\\c" FILES ${SOURCE_UNIVERSAL})
ENDIF (MSVC)
 
#-------------------------------------------------------------------------------------------

set(ALL_FILES ${SOURCE_MAIN})
set(ALL_FILES ${ALL_FILES} ${HEADER_UNIVERSAL} ${SOURCE_UNIVERSAL})
add_executable(${APP_NAME} ${ALL_FILES})
