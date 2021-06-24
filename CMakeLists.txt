project(rgb-to-uv)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/rgb-to-uv.h.in"
	"${CMAKE_BINARY_DIR}/plugins/obs-filters/config/rgb-to-uv.h")

set(rgb-to-uv_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/obs-filters/config/rgb-to-uv.h")
include_directories(${LIBSPEEXDSP_INCLUDE_DIRS} ${LIBRNNOISE_INCLUDE_DIRS}
	"${CMAKE_BINARY_DIR}/plugins/obs-filters/config")

if(MSVC)
	set(rgb-to-uv_PLATFORM_DEPS
		w32-pthreads)
endif()

set(rgb-to-uv_SOURCES
	rgb-to-uv.c
	uv-filter.c)

if(WIN32)
	set(MODULE_DESCRIPTION "RGB To UV")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in rgb-to-uv.rc)
	list(APPEND rgb-to-uv_SOURCES
		rgb-to-uv.rc)
endif()

add_library(rgb-to-uv MODULE
	${rnnoise_SOURCES}
	${rgb-to-uv_SOURCES}
	${rgb-to-uv_config_HEADERS})
target_link_libraries(rgb-to-uv
	libobs
	${rgb-to-uv_PLATFORM_DEPS})
set_target_properties(rgb-to-uv PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(rgb-to-uv data)
