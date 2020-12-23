cmake_minimum_required(VERSION 3.18)

include(ExternalProject)

project(tinygeo)

set(CMAKE_POSITION_INDEPENDENT_CODE On)

# =================================== DEPENDENCY SETUP ======================================

set(TINYGEO_ALLOW_BUNDLED ON CACHE BOOL "Whether bundled libraries are allowed (overrides individual settings)")
set(TINYGEO_FORCE_BUNDLED OFF CACHE BOOL "Whether bundled libraries must be used")

# Helper function to manage the optional bundled dependency setup
function(setup_dependency CNAME LONGNAME)
	set(TINYGEO_ALLOW_BUNDLED_${CNAME} ON CACHE BOOL "Whether the bundled ${LONGNAME} may be used")
	set(TINYGEO_FORCE_BUNDLED_${CNAME} OFF CACHE BOOL "Whether the bundled ${LONGNAME} library must be used")
	
	if(NOT TINYGEO_FORCE_BUNDLED_${CNAME} AND NOT TINYGEO_FURCE_BUNDLED)
		find_package(${CNAME} QUIET)
	else()
		set(${CNAME}_FOUND ${CNAME}_NOTFOUND)
	endif()
	
	if(${CNAME}_FOUND)
		set(${CNAME}_MSG "External: ${${CNAME}_DIR}" PARENT_SCOPE)
	else()
		if(NOT TINYGEO_ALLOW_BUNDLED_${CNAME} OR NOT TINYGEO_ALLOW_BUNDLED)
			message(FATAL_ERROR "Bundled ${LONGNAME} is disabled, but no '${CNAME}' module could be found")
		endif()
	
		set(${CNAME}_MSG "Bundled" PARENT_SCOPE)
		
		cmake_language(CALL setup_bundled_${CNAME})
	endif()
endfunction()

# CapnProto
function(setup_bundled_CapnProto)
	add_subdirectory(external/capnproto)
	
	add_executable(CapnProto::capnp_tool ALIAS capnp_tool)
	add_executable(CapnProto::capnpc_cpp ALIAS capnpc_cpp)
endfunction()

setup_dependency(CapnProto "Cap'n'Proto")

# pybind11
function(setup_bundled_pybind11)
	set(PYBIND11_INSTALL ON CACHE BOOL "Install pybind11" FORCE)
	add_subdirectory(external/pybind11)
endfunction()

setup_dependency(pybind11 pybind11)

# Eigen
function(setup_bundled_Eigen3)
	set(BUILD_TESTING OFF CACHE BOOL "Eigen testing should be disabled" FORCE)
	add_subdirectory(external/eigen)
	#ExternalProject_Add(
	#	eigen_project
	#	SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/eigen
	#)
endfunction()

setup_dependency(Eigen3 Eigen)

find_package(Python REQUIRED)

# ================================ MAIN CODE ===============================================

add_library(headers INTERFACE)
target_include_directories(headers INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)

install(TARGETS headers EXPORT tinygeoConfig)
install(DIRECTORY include/tinygeo DESTINATION include)

add_subdirectory(src)

install(EXPORT tinygeoConfig DESTINATION lib/tinygeo/cmake NAMESPACE "tinygeo::")

# =============================== SETUP INFO ===============================================

message(STATUS "")
message(STATUS " --- Installation information ---")
message(STATUS "")
message(STATUS "Libraries:")
message(STATUS "  INC: ${CAPNP_INCLUDE_DIRS}")
message(STATUS "  CapnProto: ${CapnProto_MSG}")
message(STATUS "  pybind11:  ${pybind11_MSG}")
message(STATUS "  Eigen:     ${Eigen3_MSG}")
message(STATUS "  Python:    ${Python_VERSION}")
message(STATUS "")
