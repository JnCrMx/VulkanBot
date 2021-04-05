cmake_minimum_required(VERSION 3.10)

project(vulkan_bot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(CURL REQUIRED)

file(GLOB_RECURSE sources src/*.cpp src/*.h)
file(GLOB_RECURSE shaders shaders/*.vert shaders/*.frag shaders/*.comp)

add_executable(vulkan_bot ${sources})

target_include_directories(vulkan_bot PUBLIC include/)
target_include_directories(vulkan_bot PUBLIC include/external)
target_include_directories(vulkan_bot PUBLIC ${CURL_INCLUDE_DIR})

target_link_libraries(vulkan_bot PUBLIC ${Vulkan_LIBRARIES})
target_link_libraries(vulkan_bot PUBLIC glslang)
target_link_libraries(vulkan_bot PUBLIC SPIRV)
target_link_libraries(vulkan_bot PUBLIC ${CURL_LIBRARIES})

target_include_directories(vulkan_bot PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(vulkan_bot PUBLIC ${GLM_INCLUDE_DIRS})

find_program(GLSLC glslc)
find_program(GLSLang_Validator glslangValidator)
if(NOT GLSLC AND NOT GLSLang_Validator)
	message(FATAL_ERROR "No GLSL compiler found!")
endif()

if(GLSLC)
	set(GLSL_COMPILER ${GLSLC})
	set(GLSL_COMPILER_FLAGS "")
else()
	set(GLSL_COMPILER ${GLSLang_Validator})
	set(GLSL_COMPILER_FLAGS "-V")
endif()
message(STATUS "Found GLSL compiler: ${GLSL_COMPILER}")

function(add_shader TARGET SHADER)
	file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER})
	set(output ${CMAKE_BINARY_DIR}/${rel}.spv)

	get_filename_component(output-dir ${output} DIRECTORY)
	file(MAKE_DIRECTORY ${output-dir})

	add_custom_command(
		OUTPUT ${output}
		COMMAND ${GLSL_COMPILER} ${GLSL_COMPILER_FLAGS} -o ${output} ${SHADER}
		DEPENDS ${SHADER}
		VERBATIM)

	set_source_files_properties(${output} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${output})

endfunction(add_shader)

foreach(shader ${shaders})
	add_shader(vulkan_bot ${shader})
endforeach()

add_subdirectory(external/sleepy-discord)
target_link_libraries(vulkan_bot PUBLIC sleepy-discord)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/json)
target_link_libraries(vulkan_bot PUBLIC nlohmann_json::nlohmann_json)
