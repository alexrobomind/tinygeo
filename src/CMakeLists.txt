function(add_capnp_cpp target name)
	set(INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/capnp_${target}_includes)
	set(CAPNP_TARGET capnp_${target}_compilation)
	
	get_target_property(CAPNP_INCLUDE_DIRECTORY CapnProto::capnp_tool CAPNP_INCLUDE_DIRECTORY)
	
	add_custom_command(
		COMMAND $<TARGET_FILE:CapnProto::capnp_tool> compile -o$<TARGET_FILE:CapnProto::capnpc_cpp> --src-prefix ${CMAKE_CURRENT_SOURCE_DIR} -I${CAPNP_INCLUDE_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}/${name}
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

add_capnp_cpp(tinygeo_capnp tinygeo.capnp)

pybind11_add_module(tinygeo python.cpp)
target_link_libraries(tinygeo PRIVATE headers Eigen3::Eigen tinygeo_capnp)
#target_link_libraries(tinygeo PRIVATE headers Eigen3::Eigen)

install(TARGETS tinygeo EXPORT tinygeo_exports LIBRARY DESTINATION lib)
install(TARGETS tinygeo_capnp EXPORT tinygeo_exports)