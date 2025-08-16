#include <zlib.h>
#include "../types.h"
#include "../draw_window.h"
#include "../application.h"
#include "../vector_layer.h"
#include "../utils.h"
#include "../memory.h"
#include "png_file.h"

#ifdef __cplusplus
extern "C" {
#endif

void StoreVectorShapeData(
	VECTOR_DATA* data,
	MEMORY_STREAM_PTR stream
)
{
	switch(data->line.base_data.vector_type)
	{
	case VECTOR_TYPE_SQUARE:
		(void)MemWrite(&data->square.base_data.vector_type, sizeof(data->square.base_data.vector_type), 1, stream);
		(void)MemWrite(&data->square.flags, sizeof(data->square.flags), 1, stream);
		(void)MemWrite(&data->square.line_joint, sizeof(data->square.line_joint), 1, stream);
		(void)MemWrite(&data->square.left, sizeof(data->square.left), 1, stream);
		(void)MemWrite(&data->square.top, sizeof(data->square.top), 1, stream);
		(void)MemWrite(&data->square.width, sizeof(data->square.width), 1, stream);
		(void)MemWrite(&data->square.height, sizeof(data->square.height), 1, stream);
		(void)MemWrite(&data->square.rotate, sizeof(data->square.rotate), 1, stream);
		(void)MemWrite(&data->square.line_width, sizeof(data->square.line_width), 1, stream);
		(void)MemWrite(&data->square.line_color, sizeof(data->square.line_color), 1, stream);
		(void)MemWrite(&data->square.fill_color, sizeof(data->square.fill_color), 1, stream);
		break;
	case VECTOR_TYPE_RHOMBUS:
	case VECTOR_TYPE_ECLIPSE:
		(void)MemWrite(&data->eclipse.base_data.vector_type, sizeof(data->eclipse.base_data.vector_type), 1, stream);
		(void)MemWrite(&data->eclipse.flags, sizeof(data->eclipse.fill_color), 1, stream);
		(void)MemWrite(&data->eclipse.line_joint, sizeof(data->eclipse.line_joint), 1, stream);
		(void)MemWrite(&data->eclipse.x, sizeof(data->eclipse.x), 1, stream);
		(void)MemWrite(&data->eclipse.y, sizeof(data->eclipse.y), 1, stream);
		(void)MemWrite(&data->eclipse.radius, sizeof(data->eclipse.radius), 1, stream);
		(void)MemWrite(&data->eclipse.ratio, sizeof(data->eclipse.ratio), 1, stream);
		(void)MemWrite(&data->eclipse.rotate, sizeof(data->eclipse.rotate), 1, stream);
		(void)MemWrite(&data->eclipse.line_width, sizeof(data->eclipse.line_width), 1, stream);
		(void)MemWrite(&data->eclipse.line_color, sizeof(data->eclipse.line_color), 1, stream);
		(void)MemWrite(&data->eclipse.fill_color, sizeof(data->eclipse.fill_color), 1, stream);
		break;
	}
}

void StoreVectorScriptData(VECTOR_SCRIPT* script, MEMORY_STREAM_PTR stream)
{
	uint32 length;
	length = (uint32)strlen(script->script_data) + 1;
	(void)MemWrite(&script->base_data.vector_type, sizeof(script->base_data.vector_type), 1, stream);
	(void)MemWrite(&length, sizeof(length), 1, stream);
	(void)MemWrite(script->script_data, 1, length, stream);
}

void StoreAdjustmentLayerData(
	void* stream,
	stream_func_t write_function,
	ADJUSTMENT_LAYER* layer
)
{
	int32 write_data;
	(void)write_function(&layer->target, sizeof(layer->target), 1, stream);
	(void)write_function(&layer->type, sizeof(layer->type), 1, stream);

	switch(layer->type)
	{
	case ADJUSTMENT_LAYER_TYPE_BRIGHT_CONTRAST:
		write_data = layer->filter_data.bright_contrast.bright;
		(void)write_function(&write_data, sizeof(write_data), 1, stream);
		write_data = layer->filter_data.bright_contrast.contrast;
		(void)write_function(&write_data, sizeof(write_data), 1, stream);
		break;
	case ADJUSTMENT_LAYER_TYPE_HUE_SATURATION:
		write_data = layer->filter_data.hue_saturation.h;
		(void)write_function(&write_data, sizeof(write_data), 1, stream);
		write_data = layer->filter_data.hue_saturation.s;
		(void)write_function(&write_data, sizeof(write_data), 1, stream);
		write_data = layer->filter_data.hue_saturation.v;
		(void)write_function(&write_data, sizeof(write_data), 1, stream);
		break;
	}
}

void StoreVectorLayerdata(
	LAYER* layer,
	void* out,
	stream_func_t write_function,
	MEMORY_STREAM_PTR stream,
	int32 compress_level
)
{
	VECTOR_LINE_BASE_DATA line_base;
	MEMORY_STREAM_PTR raw_stream;
	z_stream compress_stream;
	uint32 vector_data_size;
	uint32 data_size;
	uint32 num_line = 0;
	VECTOR_LINE* line = (VECTOR_LINE*)((VECTOR_BASE_DATA*)layer->layer_data.vector_layer->base->base_data.next);
	uint32 data32;	// �f�[�^4�o�C�g�����o���p
	unsigned int i;

	raw_stream = CreateMemoryStream(layer->stride * layer->height);

	(void)MemSeek(raw_stream, sizeof(uint32), SEEK_SET);
	data32 = layer->layer_data.vector_layer->flags;
	(void)MemWrite(&data32, sizeof(data32), 1, raw_stream);

	while(line != NULL)
	{
		if(line->base_data.vector_type < NUM_VECTOR_LINE_TYPE)
		{
			line_base.vector_type = line->base_data.vector_type;
			line_base.flags = line->flags;
			line_base.num_points = line->num_points;
			line_base.blur = line->blur;
			line_base.outline_hardness = line->outline_hardness;

			(void)MemWrite(&line_base.vector_type, sizeof(line_base.vector_type), 1, raw_stream);
			(void)MemWrite(&line_base.flags, sizeof(line_base.flags), 1, raw_stream);
			(void)MemWrite(&line_base.num_points, sizeof(line_base.num_points), 1, raw_stream);
			(void)MemWrite(&line_base.blur, sizeof(line_base.blur), 1, raw_stream);
			(void)MemWrite(&line_base.outline_hardness, sizeof(line_base.outline_hardness), 1, raw_stream);
			for(i = 0; i < line->num_points; i++)
			{
				(void)MemWrite(&line->points[i].vector_type, sizeof(line->points->vector_type), 1, raw_stream);
				(void)MemWrite(&line->points[i].flags, sizeof(line->points->flags), 1, raw_stream);
				(void)MemWrite(&line->points[i].color, sizeof(line->points->color), 1, raw_stream);
				(void)MemWrite(&line->points[i].pressure, sizeof(line->points->pressure), 1, raw_stream);
				(void)MemWrite(&line->points[i].size, sizeof(line->points->size), 1, raw_stream);
				(void)MemWrite(&line->points[i].x, sizeof(line->points->x), 1, raw_stream);
				(void)MemWrite(&line->points[i].y, sizeof(line->points->y), 1, raw_stream);
			}
		}
		else if(line->base_data.vector_type > VECTOR_TYPE_SHAPE
			&& line->base_data.vector_type < VECTOR_SHAPE_END)
		{
			StoreVectorShapeData((VECTOR_DATA*)line, raw_stream);
		}
		else if(line->base_data.vector_type == VECTOR_TYPE_SCRIPT)
		{
			StoreVectorScriptData((VECTOR_SCRIPT*)line, raw_stream);
		}

		line = (VECTOR_LINE*)line->base_data.next;
		num_line++;
	}

	// �������ݍς݂̃f�[�^ = �����f�[�^�T�C�Y
	vector_data_size = (uint32)raw_stream->data_point;
	// �f�[�^�����o���ʒu��߂��Đ��̐������o��
	(void)MemSeek(raw_stream, 0, SEEK_SET);
	(void)MemWrite(&num_line, sizeof(num_line), 1, raw_stream);

	// �����o����̃������T�C�Y�m�F
	if(stream->data_size < raw_stream->data_size)
	{
		stream->data_size += raw_stream->data_size;
		stream->buff_ptr = (uint8*)MEM_REALLOC_FUNC(stream->buff_ptr, stream->data_size);
	}

	// �x�N�g���f�[�^��ZIP���k����
		// ���k�p�X�g���[���̃f�[�^���Z�b�g
	compress_stream.zalloc = Z_NULL;
	compress_stream.zfree = Z_NULL;
	compress_stream.opaque = Z_NULL;
	(void)deflateInit(&compress_stream, compress_level);
	compress_stream.avail_in = (uInt)vector_data_size;
	compress_stream.next_in = raw_stream->buff_ptr;
	compress_stream.avail_out = (uInt)stream->data_size;
	compress_stream.next_out = stream->buff_ptr;
	// ���k���s
	(void)deflate(&compress_stream, Z_FINISH);

	// ���k��̃f�[�^�o�C�g������������
	data_size = (uint32)(stream->data_size - compress_stream.avail_out
							+ sizeof(vector_data_size));
	(void)write_function(&data_size, sizeof(data_size), 1, out);
	(void)write_function(&vector_data_size, sizeof(vector_data_size), 1, out);
	(void)write_function(stream->buff_ptr, 1, data_size - sizeof(data_size), out);

	(void)deflateEnd(&compress_stream);

	(void)DeleteMemoryStream(raw_stream);
}

void StoreLayerData(
	LAYER* layer,
	MEMORY_STREAM_PTR stream,
	int32 compress_level,
	int save_layer_data
)
{
	size_t data_start_point;
	uint32 size_t_temp;
	int i;

	if(stream == NULL)
	{
		stream = CreateMemoryStream(layer->stride * layer->height);
	}

	// ���O���̊�{������������
	{
		LAYER_BASE_DATA base;
		uint16 name_length;
		int8 hierarchy = 0;

		name_length = (uint16)strlen(layer->name) + 1;
		(void)MemWrite(&name_length, sizeof(name_length), 1, stream);
		(void)MemWrite(layer->name, 1, name_length, stream);

		// ���C���[�Z�b�g�ɂ��K�w���`�F�b�N
		{
			LAYER_SET *layer_set = layer->layer_set;
			while(layer_set != NULL)
			{
				hierarchy++;
				layer_set = layer_set->layer_set;
			}
		}

		base.layer_type = layer->layer_type;
		base.layer_mode = layer->layer_mode;
		base.x = layer->x;
		base.y = layer->y;
		base.width = layer->width;
		base.height = layer->height;
		base.flags = layer->flags;
		base.alpha = layer->alpha;
		base.channel = layer->channel;
		base.layer_set = hierarchy;

		(void)MemWrite(&base.layer_type, sizeof(base.layer_type), 1, stream);
		(void)MemWrite(&base.layer_mode, sizeof(base.layer_mode), 1, stream);
		(void)MemWrite(&base.x, sizeof(base.x), 1, stream);
		(void)MemWrite(&base.y, sizeof(base.y), 1, stream);
		(void)MemWrite(&base.width, sizeof(base.width), 1, stream);
		(void)MemWrite(&base.height, sizeof(base.height), 1, stream);
		(void)MemWrite(&base.flags, sizeof(base.flags), 1, stream);
		(void)MemWrite(&base.alpha, sizeof(base.alpha), 1, stream);
		(void)MemWrite(&base.channel, sizeof(base.channel), 1, stream);
		(void)MemWrite(&base.layer_set, sizeof(base.layer_set), 1, stream);
	}

	data_start_point = stream->data_point;
	(void)MemSeek(stream, sizeof(uint32), SEEK_CUR);

	switch(layer->layer_type)
	{
	case TYPE_NORMAL_LAYER:
	default:
		WritePNGStream(stream, (stream_func_t)MemWrite, NULL, layer->pixels, layer->width, layer->height,
							layer->stride, 4, FALSE, compress_level);
		break;
	case TYPE_VECTOR_LAYER:
		{
			MEMORY_STREAM_PTR vector_stream;
			vector_stream = CreateMemoryStream(stream->data_size);
			StoreVectorLayerdata(layer, (void*)stream, (stream_func_t)MemWrite, vector_stream, compress_level);
			(void)DeleteMemoryStream(vector_stream);
		}
		break;
	case TYPE_TEXT_LAYER:
		{
			MEMORY_STREAM_PTR text_stream;
			TEXT_LAYER_BASE_DATA text_base;
			const char *font_name;
			uint16 font_name_length;

			text_stream = CreateMemoryStream(layer->window->pixel_buf_size*2);

			// �e�L�X�g���C���[�̊�{���(���W�A�c������)����������
			text_base.x = layer->layer_data.text_layer->x;
			text_base.y = layer->layer_data.text_layer->y;
			text_base.width = layer->layer_data.text_layer->width;
			text_base.height = layer->layer_data.text_layer->height;
			text_base.font_size = layer->layer_data.text_layer->font_size;
			text_base.balloon_type = layer->layer_data.text_layer->balloon_type;
			(void)memcpy(text_base.color, layer->layer_data.text_layer->color, 3);
			text_base.edge_position[0][0] = layer->layer_data.text_layer->edge_position[0][0];
			text_base.edge_position[0][1] = layer->layer_data.text_layer->edge_position[0][1];
			text_base.edge_position[1][0] = layer->layer_data.text_layer->edge_position[1][0];
			text_base.edge_position[1][1] = layer->layer_data.text_layer->edge_position[1][1];
			text_base.edge_position[2][0] = layer->layer_data.text_layer->edge_position[2][0];
			text_base.edge_position[2][1] = layer->layer_data.text_layer->edge_position[2][1];
			text_base.arc_start = layer->layer_data.text_layer->arc_start;
			text_base.arc_end = layer->layer_data.text_layer->arc_end;
			text_base.balloon_data = layer->layer_data.text_layer->balloon_data;
			(void)memcpy(text_base.back_color, layer->layer_data.text_layer->back_color, 4);
			(void)memcpy(text_base.line_color, layer->layer_data.text_layer->line_color, 4);
			text_base.line_width = layer->layer_data.text_layer->line_width;
			text_base.base_size = layer->layer_data.text_layer->base_size;
			text_base.flags = layer->layer_data.text_layer->flags;

			(void)MemWrite(&text_base.x, sizeof(text_base.x), 1, text_stream);
			(void)MemWrite(&text_base.y, sizeof(text_base.y), 1, text_stream);
			(void)MemWrite(&text_base.width, sizeof(text_base.width), 1, text_stream);
			(void)MemWrite(&text_base.height, sizeof(text_base.height), 1, text_stream);
			(void)MemWrite(&text_base.balloon_type, sizeof(text_base.balloon_type), 1, text_stream);
			(void)MemWrite(&text_base.font_size, sizeof(text_base.font_size), 1, text_stream);
			(void)MemWrite(text_base.color, sizeof(text_base.color), 1, text_stream);
			(void)MemWrite(text_base.edge_position, sizeof(text_base.edge_position), 1, text_stream);
			(void)MemWrite(&text_base.arc_start, sizeof(text_base.arc_start), 1, text_stream);
			(void)MemWrite(&text_base.arc_end, sizeof(text_base.arc_end), 1, text_stream);
			(void)MemWrite(text_base.back_color, sizeof(text_base.back_color), 1, text_stream);
			(void)MemWrite(text_base.line_color, sizeof(text_base.line_color), 1, text_stream);
			(void)MemWrite(&text_base.line_width, sizeof(text_base.line_width), 1, text_stream);
			(void)MemWrite(&text_base.base_size, sizeof(text_base.base_size), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.num_edge, sizeof(text_base.balloon_data.num_edge), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.num_children, sizeof(text_base.balloon_data.num_children), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.edge_size, sizeof(text_base.balloon_data.edge_size), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.random_seed, sizeof(text_base.balloon_data.random_seed), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.edge_random_size, sizeof(text_base.balloon_data.edge_random_size), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.edge_random_distance, sizeof(text_base.balloon_data.edge_random_distance), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.start_child_size, sizeof(text_base.balloon_data.start_child_size), 1, text_stream);
			(void)MemWrite(&text_base.balloon_data.end_child_size, sizeof(text_base.balloon_data.end_child_size), 1, text_stream);
			(void)MemWrite(&text_base.flags, sizeof(text_base.flags), 1, text_stream);
			// �t�H���g�̖��O����������
			font_name = layer->layer_data.text_layer->font_name;
			font_name_length = (uint16)strlen(font_name) + 1;
			(void)MemWrite(&font_name_length, sizeof(font_name_length), 1, text_stream);
			(void)MemWrite((void*)font_name, 1, font_name_length, text_stream);

			// �e�L�X�g�f�[�^����������
			if(layer->layer_data.text_layer->text != NULL)
			{
				uint16 text_length;
				text_length = (uint16)strlen(layer->layer_data.text_layer->text) + 1;
				(void)MemWrite(&text_length, sizeof(text_length), 1, text_stream);
				(void)MemWrite(layer->layer_data.text_layer->text,
					1, text_length, text_stream);
			}
			else
			{
				uint16 text_length;
				font_name_length = 1;
				(void)MemWrite(&font_name_length, sizeof(font_name_length), 1, text_stream);
				text_length = 0;
				(void)MemWrite(&text_length, 1, 1, text_stream);
			}

			// �o�C�g������������ł���
			size_t_temp = (uint32)text_stream->data_point;
			(void)MemWrite(&size_t_temp, sizeof(size_t_temp), 1, stream);
			// �f�[�^�������o��
			(void)MemWrite(text_stream->buff_ptr, 1, text_stream->data_point, stream);

			(void)DeleteMemoryStream(text_stream);
		}
		break;
	case TYPE_3D_LAYER:
		ASSERT(0);
		break;
	case TYPE_LAYER_SET:
		break;
	case TYPE_ADJUSTMENT_LAYER:
		StoreAdjustmentLayerData((void*)stream, (stream_func_t)MemWrite, layer->layer_data.adjustment_layer);
		break;
	}
	// �������݃f�[�^�o�C�g�����L�^
	size_t_temp = (uint32)stream->data_point - data_start_point - sizeof(data_start_point);
	(void)MemSeek(stream, data_start_point, SEEK_SET);
	(void)MemWrite(&size_t_temp, sizeof(size_t_temp), 1, stream);
	(void)MemSeek(stream, (long)size_t_temp, SEEK_CUR);

	// �ǉ���񂪂���Ώ����o��
	(void)MemWrite(&layer->num_extra_data, sizeof(layer->num_extra_data), 1, stream);
	for(i = 0; i < layer->num_extra_data; i++)
	{
		uint16 data_name_length;
		data_name_length = (uint16)strlen(layer->extra_data[i].name) + 1;
		(void)MemWrite(&data_name_length, sizeof(data_name_length), 1, stream);
		(void)MemWrite(layer->extra_data[i].name, 1, data_name_length, stream);
		size_t_temp = (uint32)layer->extra_data[i].data_size;
		(void)MemWrite(&size_t_temp, sizeof(size_t_temp), 1, stream);
		(void)MemWrite(layer->extra_data[i].data, 1, layer->extra_data->data_size, stream);
	}

	if(save_layer_data)
	{
		size_t end_point = stream->data_point;
		size_t layer_data_size = end_point - data_start_point;

		if(layer->last_write_data == NULL || layer->modeling_data_size < layer_data_size)
		{
			layer->last_write_data = MEM_REALLOC_FUNC(layer->last_write_data, layer_data_size);
		}
	}
}

void StoreCanvas(
	void* stream,
	stream_func_t write_function,
	DRAW_WINDOW* canvas,
	int add_thumbnail,
	int compress_level
)
{
	APPLICATION *app = canvas->app;
	LAYER *layer = canvas->layer;

	// �K���f�[�^����p�̕�����
	char format_string[] = "Paint Soft KABURAGI";

	// ��U�A���������ɏ������ݓ��e���쐬����
	MEMORY_STREAM_PTR image = CreateMemoryStream(canvas->pixel_buf_size*2);

	uint32 tag;
	uint32 size_t_temp;

	int i;
	
	// �t�@�C������p�̕�����������o��
	(void)write_function(format_string, 1, sizeof(format_string), stream);
	// �t�@�C���o�[�W�����������o��
	size_t_temp = FILE_VERSION;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);

	// �T���l�C���쐬�Ə�������
	if(add_thumbnail)
	{
		uint8 *pixels = (uint8*)MEM_ALLOC_FUNC( THUMBNAIL_SIZE * THUMBNAIL_SIZE * 4);
		uint8 has_thumbnail = TRUE;
		uint8 r;
		GRAPHICS_IMAGE_SURFACE surface = {0};
		GRAPHICS_DEFAULT_CONTEXT context = {0};
		GRAPHICS_SURFACE_PATTERN pattern = {0};
		FLOAT_T zoom_x, zoom_y, zoom;
		int thumbnail_width = canvas->width, thunbnail_height = canvas->height;

		(void)write_function(&has_thumbnail, sizeof(has_thumbnail), 1, stream);

		if(thumbnail_width > THUMBNAIL_SIZE || thunbnail_height > THUMBNAIL_SIZE)
		{
			zoom_x = THUMBNAIL_SIZE / (FLOAT_T)thumbnail_width;
			zoom_y = THUMBNAIL_SIZE / (FLOAT_T)thunbnail_height;

			zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y;
			thumbnail_width = (int)(canvas->width * zoom);
			thunbnail_height = (int)(canvas->height * zoom);
		}

		InitializeGraphicsImageSurfaceForData(&surface, pixels, GRAPHICS_FORMAT_ARGB32, thumbnail_width,
					thunbnail_height, thumbnail_width*4, &app->graphics);
		InitializeGraphicsDefaultContext(&context, &surface, &app->graphics);

		GraphicsScale(&context.base, zoom, zoom);
		GraphicsSetSourceSurface(&context.base, &surface.base, 0, 0, &pattern);
		GraphicsPaint(&context.base);

		for(i=0; i<thumbnail_width*thunbnail_height; i++)
		{
			r = pixels[i*4];
			pixels[i*4] = pixels[i*4+2];
			pixels[i*4+2] = r;
		}

		WritePNGStream(image, (stream_func_t)MemWrite, NULL, pixels,
						thumbnail_width, thunbnail_height, thumbnail_width*4, 4, 0, compress_level);

		size_t_temp = image->data_point;
		(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
		(void)write_function(image->buff_ptr, 1, image->data_point, stream);

		image->data_point = 0;

		MEM_FREE_FUNC(pixels);
	}
	else
	{
		uint8 has_thumbnail = FALSE;
		(void)write_function(&has_thumbnail, sizeof(has_thumbnail), 1, stream);
	}

	// �`�����l�����A�J���[���[�h�����o��
	(void)write_function(&canvas->channel, sizeof(canvas->channel), 1, stream);
	(void)write_function(&canvas->color_mode, sizeof(canvas->color_mode), 1, stream);

	// (4�̔{���ɂ���O��)�I���W�i���̕��ƍ����������o��
	size_t_temp = canvas->original_width;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
	size_t_temp = canvas->original_height;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);

	// �L�����o�X�̕��ƍ���(4�̔{��)�����o��
	size_t_temp = canvas->width;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
	size_t_temp = canvas->height;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);

	// ���C���[�̐��������o��
	(void)write_function(&canvas->num_layer, sizeof(canvas->num_layer), 1, stream);

	// �w�i�摜�̃s�N�Z���f�[�^�������o��
	WritePNGStream(image, (stream_func_t)MemWrite, NULL, canvas->back_ground,
					canvas->width, canvas->height, canvas->stride, canvas->channel, 0, compress_level);
	size_t_temp = (uint32)image->data_point;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
	(void)write_function(image->buff_ptr, 1, image->data_point, stream);

	image->data_point = 0;
	do
	{
		StoreLayerData(layer, image, compress_level, TRUE);
		layer = layer->next;
	} while(layer != NULL);
	(void)write_function(image->buff_ptr, 1, image->data_point, stream);

	// �𑜓x
	if(canvas->resolution > 0)
	{
		tag = 'resl';
		tag = UINT32_TO_BE(tag);

		(void)write_function(&tag, sizeof(tag), 1, stream);
		size_t_temp = sizeof(size_t_temp);
		(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
		size_t_temp = canvas->resolution;
		(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
	}

	// 2�߂̔w�i�F
	{
		uint8 second_bg[4];

#if defined(USE_BGR_COLOR_SPACE) && USE_BGR_COLOR_SPACE != 0
		second_bg[0] = canvas->second_back_ground[2];
		second_bg[1] = canvas->second_back_ground[1];
		second_bg[2] = canvas->second_back_ground[0];
#else
		second_bg[0] = canvas->second_back_ground[0];
		second_bg[1] = canvas->second_back_ground[1];
		second_bg[2] = canvas->second_back_ground[2];
#endif
		second_bg[3] = ((canvas->flags & DRAW_WINDOW_SECOND_BG) == 0) ? 0 : 1;

		tag = '2ndb';
		tag = UINT32_TO_BE(tag);

		(void)write_function(&tag, sizeof(tag), 1, stream);
		size_t_temp = 4;
		(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
		(void)write_function(second_bg, 1, 4, stream);
	}

	// ICC�v���t�@�C��
	if(canvas->icc_profile_data != NULL)
	{
		// ICC�v���t�@�C���̓��e�������o��
		tag = 'iccp';
		tag = UINT32_TO_BE(tag);

		(void)write_function(&tag, sizeof(tag), 1, stream);
		size_t_temp = canvas->icc_profile_size;
		(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
		(void)write_function(canvas->icc_profile_data, 1, canvas->icc_profile_size, stream);
	}
	if(canvas->icc_profile_path != NULL)
	{
		// ICC�v���t�@�C���̃t�@�C���p�X�������o��
		tag = 'iccf';
		tag = UINT32_TO_BE(tag);

		(void)write_function(&tag, sizeof(tag), 1, stream);
		size_t_temp = (uint32)strlen(canvas->icc_profile_path) + 1;
		(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
		(void)write_function(canvas->icc_profile_path, 1, size_t_temp, stream);
	}

	// �p�[�X��K
	tag = 'rulr';
	tag = UINT32_TO_BE(tag);
	(void)write_function(&tag, sizeof(tag), 1, stream);
	size_t_temp = sizeof(canvas->perspective_ruler.type) + sizeof(FLOAT_T) * NUM_PERSPECTIVE_RULER_POINTS * 2 * 2
		+ sizeof(canvas->perspective_ruler.active_point) + sizeof(canvas->perspective_ruler.angle) + sizeof(canvas->perspective_ruler.eye_level.height)
		+ sizeof(canvas->perspective_ruler.eye_level.line_color) + sizeof(uint32);
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);
	(void)write_function(&canvas->perspective_ruler.type, sizeof(canvas->perspective_ruler.type), 1, stream);
	for(i = 0; i < NUM_PERSPECTIVE_RULER_POINTS; i++)
	{
		(void)write_function(&canvas->perspective_ruler.start_point[i][0][0], sizeof(canvas->perspective_ruler.start_point[i][0][0]), 1, stream);
		(void)write_function(&canvas->perspective_ruler.start_point[i][0][1], sizeof(canvas->perspective_ruler.start_point[i][0][1]), 1, stream);
		(void)write_function(&canvas->perspective_ruler.start_point[i][1][0], sizeof(canvas->perspective_ruler.start_point[i][1][0]), 1, stream);
		(void)write_function(&canvas->perspective_ruler.start_point[i][1][1], sizeof(canvas->perspective_ruler.start_point[i][1][1]), 1, stream);
	}
	(void)write_function(&canvas->perspective_ruler.active_point, sizeof(canvas->perspective_ruler.active_point), 1, stream);
	(void)write_function(&canvas->perspective_ruler.angle, sizeof(canvas->perspective_ruler.angle), 1, stream);
	(void)write_function(&canvas->perspective_ruler.eye_level.height, sizeof(canvas->perspective_ruler.eye_level.height), 1, stream);
	(void)write_function(canvas->perspective_ruler.eye_level.line_color, sizeof(canvas->perspective_ruler.eye_level.line_color), 1, stream);
	size_t_temp = canvas->perspective_ruler.flags;
	(void)write_function(&size_t_temp, sizeof(size_t_temp), 1, stream);

	(void)DeleteMemoryStream(image);
}

/*
* ReadAdjustmentLayerData�֐�
* �������C���[�̃f�[�^��ǂݍ���
* ����
* stream		: �ǂݍ��݌��̃f�[�^
* read_function	: �ǂݍ��݂Ɏg���֐��|�C���^
* layer			: �ǂݍ��ݐ�̒������C���[
*/
void ReadAdjustmentLayerData(
	void* stream,
	stream_func_t read_function,
	ADJUSTMENT_LAYER* layer
)
{
	int32 read_data;
	(void)read_function(&layer->target, sizeof(layer->target), 1, stream);
	(void)read_function(&layer->type, sizeof(layer->target), 1, stream);

	switch(layer->type)
	{
	case ADJUSTMENT_LAYER_TYPE_BRIGHT_CONTRAST:
		(void)read_function(&read_data, sizeof(read_data), 1, stream);
		layer->filter_data.bright_contrast.bright = read_data;
		(void)read_function(&read_data, sizeof(read_data), 1, stream);
		layer->filter_data.bright_contrast.contrast = read_data;
		break;
	case ADJUSTMENT_LAYER_TYPE_HUE_SATURATION:
		(void)read_function(&read_data, sizeof(read_data), 1, stream);
		layer->filter_data.hue_saturation.h = read_data;
		(void)read_function(&read_data, sizeof(read_data), 1, stream);
		layer->filter_data.hue_saturation.s = read_data;
		(void)read_function(&read_data, sizeof(read_data), 1, stream);
		layer->filter_data.hue_saturation.v = read_data;
		break;
	}
}

/*
* ReadOriginalFormatLayer�֐�
* ���C���[1�����̃f�[�^��ǂݍ���
* ����
* stream			: �ǂݍ��݌��X�g���[��
* previous_layer	: ���̃��C���[
* canvas			: �ǂݍ��ޑΏۂ̃L�����o�X
* layer_index		: ��ԉ����琔�������C���[�̔ԍ�
* hierarchy			: ���C���[�Z�b�g�̊K�w�f�[�^
* before_error	:	: �O�̃��C���[�ǂݍ��ݒ��ɃG���[�����L�����L�^����A�h���X
* �Ԃ�l
*	�ǂݍ��񂾃��C���[	
*/
LAYER* ReadOriginalFormatLayer(
	MEMORY_STREAM_PTR stream,
	DRAW_WINDOW* canvas,
	LAYER* previous_layer,
	int layer_index,
	uint8* hierarchy,
	int* before_error
)
{
	// �Ԃ�l
	LAYER *layer;
	// ���C���[�쐬���̏�̃��C���[
	LAYER *next_layer = NULL;
	// ���C���[�̊�{���ǂݍ��ݗp
	LAYER_BASE_DATA base;
	// ���C���[�̖��O�Ƃ��̕�����o�C�g��
	char *name;
	uint16 name_length;

	int i;

	uint32 data_size, next_data_point;

	if(previous_layer != NULL)
	{
		next_layer = previous_layer->next;
	}

	if(*before_error == FALSE)
	{	// ���C���[�̊�{�f�[�^�ǂݍ���
			// ���O
		(void)MemRead(&name_length, sizeof(name_length), 1, stream);
		name = (char*)MEM_ALLOC_FUNC(name_length);
		(void)MemRead(name, 1, name_length, stream);
		// ���O�ȊO�̊�{���
		(void)MemRead(&base.layer_type, sizeof(base.layer_type), 1, stream);
		(void)MemRead(&base.layer_mode, sizeof(base.layer_mode), 1, stream);
		(void)MemRead(&base.x, sizeof(base.x), 1, stream);
		(void)MemRead(&base.y, sizeof(base.y), 1, stream);
		(void)MemRead(&base.width, sizeof(base.width), 1, stream);
		(void)MemRead(&base.height, sizeof(base.height), 1, stream);
		(void)MemRead(&base.flags, sizeof(base.flags), 1, stream);
		(void)MemRead(&base.alpha, sizeof(base.alpha), 1, stream);
		(void)MemRead(&base.channel, sizeof(base.channel), 1, stream);
		(void)MemRead(&base.layer_set, sizeof(base.layer_set), 1, stream);

		if(base.layer_type >= NUM_LAYER_TYPE)
		{
			*before_error = TRUE;
			MEM_FREE_FUNC(name);
			goto RETURN_ERROR_LAYER;
		}

		// �l�̃Z�b�g
		layer = CreateLayer(base.x, base.y, base.width, base.height, base.channel,
							base.layer_type, previous_layer, next_layer, name, canvas);
		layer->alpha = base.alpha;
		layer->layer_mode = base.layer_mode;
		layer->flags = base.flags;
		*hierarchy = base.layer_set;
		MEM_FREE_FUNC(name);
	}
	else
	{
		name = MEM_STRDUP_FUNC("Read Error Layer");
		base.layer_type = TYPE_NORMAL_LAYER;
		base.layer_mode = LAYER_BLEND_NORMAL;
		base.x = 0,	base.y = 0;
		base.width = canvas->width,	base.height = canvas->height;
		base.flags = 0;
		base.alpha = 100;
		base.channel = 4;
		base.layer_set = 0;
		*before_error = FALSE;

		// �G���[���C���[�쐬����
		layer = CreateLayer(base.x, base.y, base.width, base.height, base.channel,
							base.layer_type, previous_layer, next_layer, name, canvas);

		MEM_FREE_FUNC(name);
		layer->alpha = base.alpha;
		layer->layer_mode = base.layer_mode;
		layer->flags = base.flags;
		*hierarchy = 0;
		stream->data_point -= 4;
	}

	// ���C���[�^�C�v�ʂ̓ǂݍ��ݏ�������
	switch(base.layer_type)
	{
	case TYPE_NORMAL_LAYER:
		{	// PNG���k���ꂽ�s�N�Z���f�[�^��W�J���ēǂݍ���
			MEMORY_STREAM_PTR image;
			uint8 *pixels;
			int32 width, height, stride;
			(void)MemRead(&data_size, sizeof(data_size), 1, stream);
			next_data_point = (uint32)(stream->data_point + data_size);
			image = CreateMemoryStream(data_size);
			(void)MemRead(image->buff_ptr, 1, data_size, stream);
			pixels = ReadPNGStream(image, (stream_func_t)MemRead,
									&width, &height, &stride);
			(void)DeleteMemoryStream(image);
			if(pixels != NULL)
			{
				(void)memcpy(layer->pixels, pixels, height * stride);
				MEM_FREE_FUNC(pixels);
			}
			else
			{
				*before_error = TRUE;
				return layer;
			}
		}
		break;
	case TYPE_VECTOR_LAYER:
		{	// �L�^���ꂽ����ǂݍ���
			VECTOR_LAYER_RECTANGLE rectangle = {0, 0, base.width, base.height};
			const VECTOR_LAYER zero_vector_layer = {0};

			layer->layer_data.vector_layer =
				(VECTOR_LAYER*)MEM_ALLOC_FUNC(sizeof(*layer->layer_data.vector_layer));
			*layer->layer_data.vector_layer = zero_vector_layer;
			// �S�Ẵx�N�g���f�[�^�����X�^���C�Y���č����������C���[
			(void)memset(canvas->work_layer->pixels, 0, canvas->pixel_buf_size);
			layer->layer_data.vector_layer->mix =
				CreateVectorLineLayer(canvas->work_layer, NULL, &rectangle);

			// ��ԉ��ɂ͏�ɋ�̃��C���[��u��
			layer->layer_data.vector_layer->base =
				(VECTOR_DATA*)CreateVectorLine(NULL, NULL);
			(void)memset(layer->layer_data.vector_layer->base, 0, sizeof(VECTOR_LINE));
			layer->layer_data.vector_layer->base->line.base_data.layer
				= CreateVectorLineLayer(canvas->work_layer, NULL, &rectangle);
			layer->layer_data.vector_layer->top_data
				= layer->layer_data.vector_layer->base;

			// �f�[�^�̓ǂݍ��ݎ��s
			next_data_point = (uint32)(stream->data_point +
				ReadVectorLineData(&stream->buff_ptr[stream->data_point], layer));

			// �x�N�g���f�[�^�����X�^���C�Y
			layer->layer_data.vector_layer->flags =
				(VECTOR_LAYER_FIX_LINE | VECTOR_LAYER_RASTERIZE_ALL);
			RasterizeVectorLayer(canvas, layer, layer->layer_data.vector_layer);
		}
		break;
	case TYPE_TEXT_LAYER:
		ASSERT(0);
		break;
	case TYPE_LAYER_SET:
		{
		LAYER* target = layer->prev;
		uint8 current_hierarchy;
		uint8 *h = hierarchy;
		int i, j, k;

		next_data_point = (uint32)stream->data_point;
		current_hierarchy = *h + 1;

		for(k = layer_index - 1, h = hierarchy-1;
				k >= 0 && current_hierarchy == *h; k--, h--)
		{
			*h = 0;
			target->layer_set = layer;
			target = target->prev;
		}

		if(stream->buff_ptr[stream->data_point] > 20)
		{
			*before_error = TRUE;
			while(stream->buff_ptr[stream->data_point] != 0x0)
			{
				stream->data_point++;
			}
			next_data_point = (uint32)stream->data_point;
		}
		}
		break;
	case TYPE_3D_LAYER:
		ASSERT(0);
		break;
	case TYPE_ADJUSTMENT_LAYER:
		ReadAdjustmentLayerData(layer, (stream_func_t)MemRead, layer->layer_data.adjustment_layer);
		break;
	}

	// �ǉ����f�[�^�Ɉړ�
	(void)MemSeek(stream, (long)next_data_point, SEEK_SET);
	// �ǉ�����ǂݍ���
	(void)MemRead(&layer->num_extra_data, sizeof(layer->num_extra_data), 1, stream);

	for(i = 0; i < layer->num_extra_data; i++)
	{
		uint32 size_t_temp;

		// �f�[�^�̖��O��ǂݍ���
		(void)MemRead(&name_length, sizeof(name_length), 1, stream);
		if(name_length >= 8192)
		{
			break;
		}
		layer->extra_data[i].name = (char*)MEM_ALLOC_FUNC(name_length);
		(void)MemRead(layer->extra_data[i].name, 1, name_length, stream);
		(void)MemRead(&size_t_temp, sizeof(size_t_temp), 1, stream);
		if((int)size_t_temp >= layer->stride * layer->height * 3)
		{
			break;
		}
		layer->extra_data[i].data_size = size_t_temp;
		layer->extra_data[i].data = MEM_ALLOC_FUNC(layer->extra_data[i].data_size);
		(void)MemRead(layer->extra_data[i].data, 1, layer->extra_data[i].data_size, stream);
	}

	return layer;

RETURN_ERROR_LAYER:
	layer = CreateLayer(0, 0, canvas->width, canvas->height, 4, TYPE_NORMAL_LAYER,
						previous_layer, next_layer, "Read Error Layer", canvas);
	return layer;
}

/*
* ReadOriginalFormatLayers�֐�
* �w�薇���̂����C���[�f�[�^��ǂݍ���
* ����
* stream	: �ǂݍ��݌��X�g���[��
* canvas	: �ǂݍ��ޑΏۂ̃L�����o�X
* app		: �ǂݍ��ݐi����\�����邽�߂�
* num_layer	: �ǂݍ��ރ��C���[�̐�
* �Ԃ�l
*	�ǂݍ��񂾃��C���[�f�[�^
*/
LAYER* ReadOriginaFormatLayers(
	MEMORY_STREAM_PTR stream,
	DRAW_WINDOW* canvas,
	APPLICATION* app,
	uint16 num_layer
)
{
	LAYER *read_layer;
	LAYER *bottom_layer = NULL, *layer = NULL;
	uint8 *hierarychy;
	int before_error = FALSE;
	int i;

	hierarychy = (uint8*)MEM_CALLOC_FUNC(num_layer+1, sizeof(uint8));

	for(i=0; i<num_layer; i++)
	{
		read_layer = ReadOriginalFormatLayer(stream, canvas,
						layer, i, hierarychy, &before_error);
		if(bottom_layer == NULL)
		{
			bottom_layer = read_layer;
		}
		layer = read_layer;
	}

	MEM_FREE_FUNC(hierarychy);

	return bottom_layer;
}

/*
* ReadOriginalFormat�֐�
* �Ǝ��`���̃f�[�^��ǂݍ���
* ����
* stream		: �ǂݍ��݌��f�[�^�X�g���[��
* read_function	: �ǂݍ��ݗp�֐�
* stream_size	: �f�[�^�̑��o�C�g��
* app			: �ǂݍ��ݐi���\��&�L�����o�X�쐬�ׁ̈A�A�v���P�[�V�����Ǘ��f�[�^��n��
* data_name		: �t�@�C����
* �Ԃ�l
*	�ǂݍ��񂾃f�[�^��K�p�����L�����o�X�B���s������NULL
*/
DRAW_WINDOW* ReadOriginalFormat(
	void* stream,
	stream_func_t read_function,
	size_t stream_size,
	APPLICATION* app,
	const char* data_name
)
{
	DRAW_WINDOW *canvas = NULL;
	// ���C���[�r���[�֒ǉ�����ׂɃA�h���X�ۑ�
	LAYER *layer;
	// �f�[�^����x�S�ă������ɓǂݍ���ł�����e�m�F����
	MEMORY_STREAM_PTR mem_stream = CreateMemoryStream(stream_size);
	// �s�N�Z���f�[�^��PNG���k�����f�[�^��ǂݍ��ނ��߂̃X�g���[��	
	MEMORY_STREAM_PTR image;
	// �L�����o�X�̕��ƍ���(�I���W�i��)
	int32 original_width, original_height;
	// �L�����o�X�̕��ƍ���(4�̔{��)
	int32 width, height;
	// �摜��s���̃o�C�g��
	int32 stride;
	// ���C���[�̐�
	uint16 num_layer;
	// �`�����l�����A�J���[���[�h
	uint8 channel, color_mode;
	// �摜�f�[�^
	uint8 *pixels;
	// �摜�f�[�^�̃o�C�g��
	uint32 data_size;
	// �K���f�[�^����p�̕�����
	const char format_string[] = "Paint Soft KABURAGI";
	// �t�@�C���o�[�W����
	uint32 file_version;

	int i;

	(void)read_function(mem_stream->buff_ptr, 1, stream_size, stream);

	// �t�@�C���ɖ��ߍ��񂾕�����ŃI���W�i���t�H�[�}�b�g�̃f�[�^�ł��邩�m�F
	if(memcmp(mem_stream->buff_ptr, format_string, sizeof(format_string)/sizeof(format_string[0])) != 0)
	{
		goto ERROR_END;
	}

	// �t�@�C���t�H�[�}�b�g�o�[�W��������ǂݍ���
	mem_stream->data_point = sizeof(format_string) / sizeof(format_string[0]);
	(void)MemRead(&file_version, sizeof(file_version), 1, mem_stream);

	{	// �T���l�C���L���̊m�F
		uint8 has_thumbnail;
		(void)MemRead(&has_thumbnail, sizeof(has_thumbnail), 1, mem_stream);

		// �T���l�C���L��
		if(has_thumbnail)
		{
			uint32 skip_bytes;
			(void)MemRead(&skip_bytes, sizeof(skip_bytes), 1, mem_stream);
			(void)MemSeek(mem_stream, skip_bytes, SEEK_CUR);
		}
	}

	// �`�����l���ƃJ���[���[�h��ǂݍ���	
	(void)MemRead(&channel, sizeof(channel), 1, mem_stream);
	(void)MemRead(&color_mode, sizeof(color_mode), 1, mem_stream);

	// (4�̔{���ɂ���O��)�I���W�i���̕��A������ǂݍ���	
	(void)MemRead(&original_width, sizeof(original_width), 1, mem_stream);
	(void)MemRead(&original_height, sizeof(original_height), 1, mem_stream);

	// 4�̔{���ɂ����L�����o�X�̕��A������ǂݍ���	
	(void)MemRead(&width, sizeof(width), 1, mem_stream);
	(void)MemRead(&height, sizeof(height), 1, mem_stream);

	// ���C���[�̐���ǂݍ���
	(void)MemRead(&num_layer, sizeof(num_layer), 1, mem_stream);

	canvas = CreateDrawWindow(original_width, original_height, channel, data_name,
				app->widgets, app->window_num+1, app);
	app->window_num++;

	// �w�i�摜�f�[�^��ǂݍ���
	(void)MemRead(&data_size, sizeof(data_size), 1, mem_stream);
	image = CreateMemoryStream(data_size);
	(void)MemRead(image->buff_ptr, 1, data_size, mem_stream);
	pixels = ReadPNGStream(image, (stream_func_t)MemRead, &width, &height, &stride);
	// �L�����o�X�̔w�i�f�[�^�փR�s�[
	(void)memcpy(canvas->back_ground, pixels, height * stride);
	// �������J��
	(void)DeleteMemoryStream(image);
	MEM_FREE_FUNC(pixels);

	// ���C���[�̏��ǂݎ��
		// ��ԉ��Ɏ����ō���郌�C���[���폜
	DeleteLayer(&canvas->layer);
	if(file_version == FILE_VERSION)
	{
		canvas->layer = ReadOriginaFormatLayers(mem_stream, canvas, app, num_layer);
	}
	canvas->num_layer = num_layer;

	return canvas;

ERROR_END:
	(void)DeleteMemoryStream(mem_stream);
	return NULL;
}

#ifdef __cplusplus
}
#endif
