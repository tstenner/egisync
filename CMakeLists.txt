cmake_minimum_required(VERSION 3.10)

project(EGISync
	VERSION 0.1
	LANGUAGES CXX)

# also look for CMake modules in the cmake subfolder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find an installed liblsl in paths set by the user (LSL_INSTALL_ROOT)
# and some default paths
find_package(LSL REQUIRED
	HINTS ${LSL_INSTALL_ROOT}
	"${CMAKE_CURRENT_LIST_DIR}/../lsl/LSL/liblsl/build/"
	PATH_SUFFIXES share/LSL)
get_filename_component(LSL_PATH ${LSL_CONFIG} DIRECTORY)
message(STATUS "Found LSL lib in ${LSL_PATH}")
LSLAPP_Setup_Boilerplate()

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads)

add_executable(${PROJECT_NAME}
	egisync.cpp
	netstation.cpp
	netstation.hpp
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Boost::system
	LSL::lsl
	Threads::Threads
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
installLSLApp(${PROJECT_NAME})
#installLSLAuxFiles(${PROJECT_NAME}
#	${PROJECT_NAME}.cfg
#)
LSLGenerateCPackConfig()
