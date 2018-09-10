cmake_minimum_required(VERSION 3.2)
project(sywave)

find_package(yaml-cpp) 
find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
	pkg_check_modules(LIBSNDFILE_PKGCONF sndfile)
else()
  message(ERROR "Could not find PkgConfig!")
endif(PKG_CONFIG_FOUND)

# Find libsndfile
find_path(LIBSNDFILE_INCLUDE_DIRS
	NAMES sndfile.h
	PATHS ${LIBSNDFILE_PKGCONF_INCLUDE_DIRS}
)
find_library(LIBSNDFILE_LIBRARIES
	NAMES sndfile libsndfile-1
	PATHS ${LIBSNDFILE_PKGCONF_LIBRARY_DIRS}
)

add_executable(sywave src/sywave.cpp)
target_include_directories(sywave PRIVATE src ${yaml-cpp_INCLUDE_DIRS} ${LIBSNDFILE_INCLUDE_DIRS})
target_compile_features(sywave PRIVATE cxx_std_17)
target_link_libraries(sywave ${YAML_CPP_LIBRARIES} ${LIBSNDFILE_LIBRARIES})
