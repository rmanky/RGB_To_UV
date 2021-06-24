#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>

/* clang-format off */

#define SETTING_PATH		       "effect_path"
#define SETTING_IMAGE_PATH_A           "path_A"
#define SETTING_IMAGE_PATH_B           "path_B"
#define SETTING_LIGHTING	       "lighting"
#define SETTING_RESOLUTION             "resolution"

#define TEXT_IMAGE_PATH_A              obs_module_text("ImageA")
#define TEXT_IMAGE_PATH_B              obs_module_text("ImageB")
#define TEXT_LIGHTING		       obs_module_text("Lighting")
#define TEXT_RESOLUTION                obs_module_text("Resolution")
#define TEXT_PATH_IMAGES               obs_module_text("BrowsePath.Images")
#define TEXT_PATH_ALL_FILES            obs_module_text("BrowsePath.AllFiles")

/* clang-format on */

struct uv_filter_data {
	obs_source_t *context;
	gs_effect_t *effect;

	char *image_file_a;
	char *image_file_b;

	gs_texture_t *target_a;
	gs_texture_t *target_b;

	gs_image_file_t image_a;
	gs_image_file_t image_b;

	float lighting;
	float resolution;
};

static const char *uv_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("RGBToUV");
}

static void uv_filter_image_unload(struct uv_filter_data *filter)
{
	obs_enter_graphics();
	gs_image_file_free(&filter->image_a);
	gs_image_file_free(&filter->image_b);
	obs_leave_graphics();
}

static void uv_filter_image_load(struct uv_filter_data *filter)
{
	uv_filter_image_unload(filter);

	char *path_a = filter->image_file_a;
	char *path_b = filter->image_file_b;

	if (path_a && *path_a && path_b && *path_b) {
		gs_image_file_init(&filter->image_a, path_a);
		gs_image_file_init(&filter->image_b, path_b);

		obs_enter_graphics();
		gs_image_file_init_texture(&filter->image_a);
		gs_image_file_init_texture(&filter->image_b);
		obs_leave_graphics();
	}

	filter->target_a = filter->image_a.texture;
	filter->target_b = filter->image_b.texture;
}

static void uv_filter_update_internal(void *data, obs_data_t *settings,
				      float lighting, float resolution)
{
	struct uv_filter_data *filter = data;

	const char *path_a =
		obs_data_get_string(settings, SETTING_IMAGE_PATH_A);
	const char *path_b =
		obs_data_get_string(settings, SETTING_IMAGE_PATH_B);

	const char *effect_file = obs_data_get_string(settings, SETTING_PATH);
	char *effect_path;

	if (filter->image_file_a)
		bfree(filter->image_file_a);

	if (filter->image_file_b)
		bfree(filter->image_file_b);

	filter->image_file_a = bstrdup(path_a);
	filter->image_file_b = bstrdup(path_b);

	filter->lighting = lighting;
	filter->resolution = resolution;

	uv_filter_image_load(filter);

	obs_enter_graphics();
	effect_path = obs_module_file(effect_file);
	gs_effect_destroy(filter->effect);
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	bfree(effect_path);
	obs_leave_graphics();
}

static void uv_filter_update(void *data, obs_data_t *settings)
{
	const float lighting =
		(float)obs_data_get_double(settings, SETTING_LIGHTING);
	const float resolution =
		(float)obs_data_get_double(settings, SETTING_RESOLUTION);

	uv_filter_update_internal(data, settings, lighting, resolution);
}

static void uv_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SETTING_PATH, "uv_filter.effect");
	obs_data_set_default_double(settings, SETTING_LIGHTING, 1.0);
	obs_data_set_default_double(settings, SETTING_RESOLUTION, 5.0);
}

#define IMAGE_FILTER_EXTENSIONS " (*.bmp *.jpg *.jpeg *.tga *.gif *.png)"

static obs_properties_t *uv_filter_properties_internal(bool use_float_opacity)
{
	obs_properties_t *props = obs_properties_create();
	struct dstr filter_str = {0};

	dstr_copy(&filter_str, TEXT_PATH_IMAGES);
	dstr_cat(&filter_str, IMAGE_FILTER_EXTENSIONS ";;");
	dstr_cat(&filter_str, TEXT_PATH_ALL_FILES);
	dstr_cat(&filter_str, " (*.*)");

	obs_properties_add_path(props, SETTING_IMAGE_PATH_A, TEXT_IMAGE_PATH_A,
				OBS_PATH_FILE, filter_str.array, NULL);
	obs_properties_add_path(props, SETTING_IMAGE_PATH_B, TEXT_IMAGE_PATH_B,
				OBS_PATH_FILE, filter_str.array, NULL);

	obs_properties_add_float_slider(props, SETTING_LIGHTING, TEXT_LIGHTING,
					0.0, 1.0, 0.1);
	obs_properties_add_float_slider(props, SETTING_RESOLUTION,
					TEXT_RESOLUTION, 1.0, 8.0, 1.0);

	dstr_free(&filter_str);

	return props;
}

static obs_properties_t *uv_filter_properties(void *data)
{
	UNUSED_PARAMETER(data);

	return uv_filter_properties_internal(true);
}

static void *uv_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct uv_filter_data *filter = bzalloc(sizeof(struct uv_filter_data));
	filter->context = context;

	obs_source_update(context, settings);
	return filter;
}

static void uv_filter_destroy(void *data)
{
	struct uv_filter_data *filter = data;

	if (filter->image_file_a)
		bfree(filter->image_file_a);

	if (filter->image_file_b)
		bfree(filter->image_file_b);

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	gs_image_file_free(&filter->image_a);
	gs_image_file_free(&filter->image_b);
	obs_leave_graphics();

	bfree(filter);
}

static void uv_filter_render(void *data, gs_effect_t *effect)
{
	struct uv_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);
	gs_eparam_t *param;

	if (!target || !filter->target_a || !filter->target_b ||
	    !filter->effect) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
					     OBS_ALLOW_DIRECT_RENDERING))
		return;

	param = gs_effect_get_param_by_name(filter->effect, "texture_a");
	gs_effect_set_texture(param, filter->target_a);

	param = gs_effect_get_param_by_name(filter->effect, "texture_b");
	gs_effect_set_texture(param, filter->target_b);

	param = gs_effect_get_param_by_name(filter->effect, "lighting");
	gs_effect_set_float(param, filter->lighting);

	param = gs_effect_get_param_by_name(filter->effect, "resolution");
	gs_effect_set_float(param, filter->resolution);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);
	gs_blend_state_pop();

	UNUSED_PARAMETER(effect);
}

struct obs_source_info uv_filter = {
	.id = "uv_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = uv_filter_get_name,
	.create = uv_filter_create,
	.destroy = uv_filter_destroy,
	.update = uv_filter_update,
	.get_defaults = uv_filter_defaults,
	.get_properties = uv_filter_properties,
	.video_render = uv_filter_render,
};
