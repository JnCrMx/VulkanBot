cmake_minimum_required(VERSION 3.10)

project(vulkan_bot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED COMPONENTS glslangValidator SPIRV-Tools)
find_package(glslang REQUIRED)
find_package(glm REQUIRED)

file(GLOB_RECURSE sources src/*.cpp src/*.h external/lodepng/lodepng.cpp)
file(GLOB_RECURSE shaders shaders/*.vert shaders/*.frag shaders/*.comp)
file(GLOB_RECURSE shader_includes shader_include/*.glsl)

add_executable(vulkan_bot ${sources})

target_include_directories(vulkan_bot PUBLIC include/)
target_include_directories(vulkan_bot PUBLIC external/lodepng)

include(FetchContent)
FetchContent_Declare(DPP
  GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
  GIT_TAG        v10.0.30
)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_Declare(avcpp
  GIT_REPOSITORY https://github.com/h4tr3d/avcpp.git
  GIT_TAG        v2.4.0
)
FetchContent_MakeAvailable(json avcpp)

option(USE_INSTALLED_DPP "Use preinstalled DPP" OFF)
if(USE_INSTALLED_DPP)
  find_package(dpp REQUIRED)
else()
  FetchContent_MakeAvailable(DPP)
  add_library(dpp::dpp ALIAS dpp)
endif(USE_INSTALLED_DPP)

message(STATUS "Found GLSL compiler: ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")

function(add_shader TARGET SHADER)
	file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER})
	set(output ${CMAKE_BINARY_DIR}/${rel}.spv)

	get_filename_component(output-dir ${output} DIRECTORY)
	file(MAKE_DIRECTORY ${output-dir})

	add_custom_command(
		OUTPUT ${output}
		COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V -o ${output} ${SHADER}
		DEPENDS ${SHADER}
		VERBATIM)

	set_source_files_properties(${output} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${output})

endfunction(add_shader)

foreach(shader ${shaders})
	add_shader(vulkan_bot ${shader})
endforeach()

foreach(shader_include ${shader_includes})
	file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${shader_include})
	configure_file(${rel} ${rel} COPYONLY)
endforeach()

target_link_libraries(vulkan_bot PUBLIC Vulkan::Vulkan)
target_link_libraries(vulkan_bot PUBLIC glslang::SPIRV glslang::glslang-default-resource-limits)
target_link_libraries(vulkan_bot PUBLIC dpp::dpp nlohmann_json::nlohmann_json)
target_link_libraries(vulkan_bot PUBLIC avcpp::avcpp-static)
target_link_libraries(vulkan_bot PUBLIC glm::glm)
target_compile_features(vulkan_bot PUBLIC cxx_std_23)

if(USE_INSTALLED_DPP) # DPP doesn't properly export include directories nor libraries
  target_include_directories(vulkan_bot PUBLIC ${dpp_DIR}/../../../include)
  target_link_libraries(vulkan_bot PUBLIC nlohmann_json::nlohmann_json)
  target_link_libraries(vulkan_bot PUBLIC crypto ssl)
endif(USE_INSTALLED_DPP)

include(GNUInstallDirs)

install(TARGETS vulkan_bot DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders DESTINATION ${CMAKE_INSTALL_DATADIR}/vulkan_bot)
