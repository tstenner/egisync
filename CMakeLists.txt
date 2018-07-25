cmake_minimum_required(VERSION 3.5)

project(EGISync
	VERSION 0.1
	LANGUAGES CXX)


# set up LSL if not done already
if(NOT TARGET LSL::lsl)
	# when building out of tree LSL_ROOT needs to be specified on the cmd line
	file(TO_CMAKE_PATH "${LSL_INSTALL_ROOT}" LSL_INSTALL_ROOT)
	list(APPEND LSL_INSTALL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../LSL/liblsl/build/install")
	find_package(LSL HINTS ${LSL_INSTALL_ROOT}/share/LSL/ ${LSL_INSTALL_ROOT}/LSL/share/LSL QUIET)
	if(NOT LSL_FOUND)
		message(FATAL_ERROR "Precompiled LSL was not found. See https://github.com/labstreaminglayer/labstreaminglayer/blob/master/doc/BUILD.md#lsl_install_root for more information.")
	endif()
	list(APPEND CMAKE_MODULE_PATH ${LSL_DIR})
	message(STATUS "Looking for LSLCMake in ${LSL_DIR}")
	include(LSLCMake)
endif()

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

target_compile_features(${PROJECT_NAME} PRIVATE cxx_auto_type)
installLSLApp(${PROJECT_NAME})
#installLSLAuxFiles(${PROJECT_NAME}
#	${PROJECT_NAME}.cfg
#)
LSLGenerateCPackConfig()