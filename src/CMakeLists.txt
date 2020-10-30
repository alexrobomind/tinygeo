function(add_capnp_cpp target name)
	set(INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/capnp_${name}_includes)
	set(CAPNP_TARGET capnp_${name}_compilation)
	
	add_custom_command(
		COMMAND $<TARGET_FILE:CapnProto::capnp_tool> compile -o$<TARGET_FILE:CapnProto::capnpc_cpp> --src-prefix ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${name}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${INCLUDE_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${name}.h ${INCLUDE_DIR}
		
		OUTPUT ${name}.c++
		DEPENDS capnpc_cpp capnp_tool
		SOURCES name
		
		COMMENT Running capnp C++ compiler for ${name}
	)
	
	add_library(${target} ${name}.c++)
	target_include_directories(${target} PUBLIC ${INCLUDE_DIR})
	
	target_link_libraries(${target} CapnProto::capnp)
endfunction()

add_capnp_cpp(rtree_geo rtree_geo.capnp)

pybind11_add_module(tinyrgeo_python python.cpp)
target_link_libraries(tinyrgeo_python PRIVATE tinyrgeo)