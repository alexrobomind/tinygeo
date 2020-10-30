cmake_minimum_required(VERSION 3.18)

project(tinyrgeo)

# =================================== DEPENDENCY SETUP ======================================

# Helper function to manage the optional bundled dependency setup
function(setup_dependency CNAME LONGNAME)
	set(TINYRGEO_ALLOW_BUNDLED_${CNAME} ON CACHE BOOL "Whether the bundled ${LONGNAME} may be used")
	set(TINYRGEO_FORCE_BUNDLED_${CNAME} OFF CACHE BOOL "Whether the bundled ${LONGNAME} library must be used")
	
	if(NOT TINYRGEO_FORCE_BUNDLED_${CNAME})
		find_package(${LONGNAME} QUIET)
	else()
		set(${CNAME}_FOUND ${CNAME}_NOTFOUND)
	endif()
	
	if(${CNAME}_FOUND)
		set(${CNAME}_MSG "External: ${${CNAME}_DIR}" PARENT_SCOPE)
	else()
		if(NOT TINYRGEO_ALLOW_BUNDLED_${CNAME})
			message(FATAL_ERROR "Bundled ${LONGNAME} is disabled, but no '${CNAME}' module could be found")
		endif()
	
		set(${CNAME}_MSG "Bundled" PARENT_SCOPE)
		
		cmake_language(CALL setup_bundled_${CNAME})
	endif()
endfunction()

# CapnProto
function(setup_bundled_CapnProto)
	add_subdirectory(external/capnproto EXCLUDE_FROM_ALL)
	
	add_executable(CapnProto::capnp_tool ALIAS capnp_tool)
	add_executable(CapnProto::capnpc_cpp ALIAS capnpc_cpp)
endfunction()

setup_dependency(CapnProto "Cap'n'Proto" )

# pybind11
function(setup_bundled_pybind11)
	add_subdirectory(external/pybind11 EXCLUDE_FROM_ALL)
endfunction()

setup_dependency(pybind11 pybind11)

# ================================ MAIN CODE ===============================================

add_library(tinyrgeo INTERFACE)
add_library(tinyrgeo::tinyrgeo ALIAS tinyrgeo)
target_include_directories(tinyrgeo INTERFACE include)

add_subdirectory(src)

# =============================== SETUP INFO ===============================================

message(STATUS "")
message(STATUS " --- Installation information ---")
message(STATUS "")
message(STATUS "Libraries:")
message(STATUS "  CapnProto: ${CapnProto_MSG}")
message(STATUS "  pybind11:  ${pybind11_MSG}")
message(STATUS "")