#include <obs-module.h>
#include "rgb-to-uv.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("rgb-to-uv", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "RGB To UV";
}

extern struct obs_source_info uv_filter;

bool obs_module_load(void)
{
	obs_register_source(&uv_filter);
	return true;
}
