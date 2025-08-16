#include <string.h>
#include "types.h"
#include "utils.h"
#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ft.h"

#ifdef __cplusplus
extern "C" {
#endif

static void RasterizeCharacter(
	uint8* pixels,
	int x,
	int y,
	int width,
	int height,
	int stride,
	const uint8 color[4],
	FT_Bitmap* bitmap
)
{
	int i, j, p, q;
	int x_max = bitmap->width;
	int y_max = bitmap->rows;
	
	for(i=0, q=0; i<y_max; i++, q++)
	{
		for(j=0, p=0; j<x_max; j++, p++)
		{
			if(i + y >= height || j + x >= width)
			{
				continue;
			}
			
			pixels[(i+y)*stride + (j+x)*4 + 0]
				= (color[0] * bitmap->buffer[q*bitmap->width + p]) / 255;
			pixels[(i+y)*stride + (j+x)*4 + 1]
				= (color[1] * bitmap->buffer[q*bitmap->width + p]) / 255;
			pixels[(i+y)*stride + (j+x)*4 + 2]
				= (color[2] * bitmap->buffer[q*bitmap->width + p]) / 255;
			pixels[(i+y)*stride + (j+x)*4 + 3]
				= (color[3] * bitmap->buffer[q*bitmap->width + p]) / 255;
		}
	}
}

void DrawText(
	const char* utf8_text,
	const char* true_type_file_path,
	uint8* pixels,
	int x,
	int y,
	int target_width,
	int target_height,
	int target_stride,
	const uint8 color[4],
	int font_size,
	int dot_per_inch,
	int bold,
	int italic,
	int* drawn_width,
	int* drawn_height
)
{
	hb_buffer_t *harfbuzz_buffer;
	hb_blob_t *harfbuzz_blob;
	hb_face_t *harfbuzz_face;
	hb_font_t *harfbuzz_font;
	hb_glyph_info_t *harfbuzz_glyph_info;
	
	FT_Library freetype_library;
	FT_Face freetype_face;
	
	int local_width = 0, local_height = 0;
	int current_width = 0;
	
	unsigned int glyph_count;
	int cursor_x = x, cursor_y = y;
	int max_height_in_row = 0;
	unsigned int i;
	
	FT_Init_FreeType(&freetype_library);
	FT_New_Face(freetype_library, true_type_file_path, 0, &freetype_face);
	FT_Set_Char_Size(freetype_face, font_size, font_size,
						dot_per_inch, dot_per_inch);
	if(bold)
	{
		freetype_face->style_flags |= FT_STYLE_FLAG_BOLD;
	}
	if(italic)
	{
		freetype_face->style_flags |= FT_STYLE_FLAG_ITALIC;
	}
	
	harfbuzz_buffer = hb_buffer_create();
	hb_buffer_add_utf8(harfbuzz_buffer, "utf8_text", -1, 0, -1);
	
	hb_buffer_set_direction(harfbuzz_buffer, HB_DIRECTION_LTR);
	hb_buffer_set_script(harfbuzz_buffer, HB_SCRIPT_LATIN);
	
	harfbuzz_blob = hb_blob_create_from_file(true_type_file_path);
	harfbuzz_face = hb_face_create(harfbuzz_blob, 0);
	harfbuzz_font = hb_font_create(harfbuzz_face);
	
	hb_shape(harfbuzz_font, harfbuzz_buffer, NULL, 0);
	
	harfbuzz_glyph_info = hb_buffer_get_glyph_infos(
							harfbuzz_buffer, &glyph_count);

	{
		const char *p = utf8_text;
		for(i=0; i<glyph_count; i++, p = GetNextUtf8Character(p))
		{
			if(*p == '\n')
			{
				int move_y;
				move_y = (max_height_in_row == 0) ?
								font_size : max_height_in_row;
				cursor_x = x;
				cursor_y += move_y;
				local_height += move_y;
				
				if(local_width < current_width)
				{
					local_width = current_width;
				}
				current_width = 0;
				max_height_in_row = 0;
			}
			else
			{
				FT_Load_Glyph(freetype_face,
							harfbuzz_glyph_info[i].codepoint, FT_LOAD_RENDER);
				RasterizeCharacter(pixels, cursor_x, cursor_y,
						target_width, target_height, target_stride, color, &freetype_face->glyph->bitmap);
				if(max_height_in_row < freetype_face->glyph->bitmap.rows)
				{
					max_height_in_row = freetype_face->glyph->bitmap.rows;
				}
				cursor_x += freetype_face->glyph->bitmap.width;
				current_width += freetype_face->glyph->bitmap.width;
			}
		}
	}
	
	if(local_width < current_width)
	{
		local_width = current_width;
	}
	
	if(drawn_width != NULL)
	{
		*drawn_width = local_width;
	}
	if(drawn_height != NULL)
	{
		*drawn_height = local_height;
	}
	
	hb_buffer_destroy(harfbuzz_buffer);
	hb_font_destroy(harfbuzz_font);
	hb_face_destroy(harfbuzz_face);
	hb_blob_destroy(harfbuzz_blob);
	
	FT_Done_Face(freetype_face);
	FT_Done_FreeType(freetype_library);
}

#ifdef __cplusplus
}
#endif
