function(add_capnp_cpp target name)
	set(INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/capnp_${name}_includes)
	set(CAPNP_TARGET capnp_${name}_compilation)
	
	add_custom_command(
		COMMAND $<TARGET_FILE:CapnProto::capnp_tool> compile -o$<TARGET_FILE:CapnProto::capnpc_cpp> --src-prefix ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${name}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${INCLUDE_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${name}.h ${INCLUDE_DIR}
		
		OUTPUT ${name}.c++
		DEPENDS CapnProto::capnpc_cpp CapnProto::capnp_tool ${name}
		
		COMMENT Running capnp C++ compiler for ${name}
	)
	
	add_library(${target} ${name}.c++)
	target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${INCLUDE_DIR}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)
	
	install(FILES ${INCLUDE_DIR}/${name}.h DESTINATION include)
	
	target_link_libraries(${target} CapnProto::capnp)
endfunction()

add_capnp_cpp(rtree_geo rtree_geo.capnp)

pybind11_add_module(tinyrgeo python.cpp)
target_link_libraries(tinyrgeo PRIVATE headers Eigen3::Eigen rtree_geo)

install(TARGETS tinyrgeo EXPORT tinyrgeo_exports LIBRARY DESTINATION lib)
install(TARGETS rtree_geo EXPORT tinyrgeo_exports)