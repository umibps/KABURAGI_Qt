#include <string.h>
#include <stdlib.h>
#include <QWidget>
#include "../../labels.h"
#include "../../fractal_label.h"
#include "../../memory.h"

static void LoadFractalLabels(
	FRACTAL_LABEL* labels,
	APPLICATION_LABELS* app_label
);

void LoadLabels(
	APPLICATION_LABELS* labels,
	FRACTAL_LABEL* fractal_labels,
	const char* lang_file_path
)
{
#define MAX_STR_SIZE 512
	int i;

	// 言語名を取得
	labels->language_name = MEM_STRDUP_FUNC(QWidget::tr("Language").toUtf8().data());

	// ウィンドウの基本キャプション
	labels->window.ok = MEM_STRDUP_FUNC(QWidget::tr("OK").toUtf8().data());
	labels->window.apply = MEM_STRDUP_FUNC(QWidget::tr("Apply").toUtf8().data());
	labels->window.cancel = MEM_STRDUP_FUNC(QWidget::tr("Cancel").toUtf8().data());
	labels->window.normal = MEM_STRDUP_FUNC(QWidget::tr("Normal").toUtf8().data());
	labels->window.reverse = MEM_STRDUP_FUNC(QWidget::tr("Reverse").toUtf8().data());
	labels->window.edit_selection = MEM_STRDUP_FUNC(QWidget::tr("Edit Selection").toUtf8().data());
	labels->window.window = MEM_STRDUP_FUNC(QWidget::tr("Window").toUtf8().data());
	labels->window.close_window = MEM_STRDUP_FUNC(QWidget::tr("Close Window").toUtf8().data());
	labels->window.place_left = MEM_STRDUP_FUNC(QWidget::tr("Dock Left").toUtf8().data());
	labels->window.place_right = MEM_STRDUP_FUNC(QWidget::tr("Dock Right").toUtf8().data());
	labels->window.fullscreen = MEM_STRDUP_FUNC(QWidget::tr("Fullscreen").toUtf8().data());
	labels->window.reference = MEM_STRDUP_FUNC(QWidget::tr("Reference").toUtf8().data());
	labels->window.move_top_left = MEM_STRDUP_FUNC(QWidget::tr("Move Top Left").toUtf8().data());
	labels->window.hot_key = MEM_STRDUP_FUNC(QWidget::tr("Hot Key").toUtf8().data());
	labels->window.loading = MEM_STRDUP_FUNC(QWidget::tr("Loading").toUtf8().data());
	labels->window.saving = MEM_STRDUP_FUNC(QWidget::tr("Saving").toUtf8().data());

	// 単位
	labels->unit.pixel = MEM_STRDUP_FUNC(QWidget::tr("Pixel").toUtf8().data());
	labels->unit.length = MEM_STRDUP_FUNC(QWidget::tr("Length").toUtf8().data());
	labels->unit.angle = MEM_STRDUP_FUNC(QWidget::tr("Angle").toUtf8().data());
	labels->unit.bg = MEM_STRDUP_FUNC(QWidget::tr("Background").toUtf8().data());
	labels->unit.repeat = MEM_STRDUP_FUNC(QWidget::tr("Repeat").toUtf8().data());
	labels->unit.preview = MEM_STRDUP_FUNC(QWidget::tr("Preview").toUtf8().data());
	labels->unit.interval = MEM_STRDUP_FUNC(QWidget::tr("Interval").toUtf8().data());
	labels->unit.minute = MEM_STRDUP_FUNC(QWidget::tr("Minute").toUtf8().data());
	labels->unit.detail = MEM_STRDUP_FUNC(QWidget::tr("Detail").toUtf8().data());
	labels->unit.target = MEM_STRDUP_FUNC(QWidget::tr("Target").toUtf8().data());
	labels->unit.clip_board = MEM_STRDUP_FUNC(QWidget::tr("Clipboard").toUtf8().data());
	labels->unit.name = MEM_STRDUP_FUNC(QWidget::tr("Name").toUtf8().data());
	labels->unit.type = MEM_STRDUP_FUNC(QWidget::tr("Type").toUtf8().data());
	labels->unit.resolution = MEM_STRDUP_FUNC(QWidget::tr("Resolution").toUtf8().data());
	labels->unit.center = MEM_STRDUP_FUNC(QWidget::tr("Center").toUtf8().data());
	labels->unit.key = MEM_STRDUP_FUNC(QWidget::tr("Key").toUtf8().data());
	labels->unit.straight = MEM_STRDUP_FUNC(QWidget::tr("Straight").toUtf8().data());
	labels->unit.extend = MEM_STRDUP_FUNC(QWidget::tr("Extend").toUtf8().data());
	labels->unit.mode = MEM_STRDUP_FUNC(QWidget::tr("Mode").toUtf8().data());
	labels->unit.red = MEM_STRDUP_FUNC(QWidget::tr("Red").toUtf8().data());
	labels->unit.green = MEM_STRDUP_FUNC(QWidget::tr("Green").toUtf8().data());
	labels->unit.blue = MEM_STRDUP_FUNC(QWidget::tr("Blue").toUtf8().data());
	labels->unit.cyan = MEM_STRDUP_FUNC(QWidget::tr("Cyan").toUtf8().data());
	labels->unit.magenta = MEM_STRDUP_FUNC(QWidget::tr("Magenta").toUtf8().data());
	labels->unit.yellow = MEM_STRDUP_FUNC(QWidget::tr("Yellow").toUtf8().data());
	labels->unit.key_plate = MEM_STRDUP_FUNC(QWidget::tr("Keyplate").toUtf8().data());
	labels->unit.add = MEM_STRDUP_FUNC(QWidget::tr("Add").toUtf8().data());
	labels->unit._delete = MEM_STRDUP_FUNC(QWidget::tr("Delete").toUtf8().data());

	// メニューバー
		// ファイルメニュー
	labels->menu.file = MEM_STRDUP_FUNC(QWidget::tr("File").toUtf8().data());
	labels->menu.make_new = MEM_STRDUP_FUNC(QWidget::tr("New").toUtf8().data());
	labels->menu.open = MEM_STRDUP_FUNC(QWidget::tr("Open").toUtf8().data());
	labels->menu.open_as_layer = MEM_STRDUP_FUNC(QWidget::tr("Open as Layer").toUtf8().data());
	labels->menu.save = MEM_STRDUP_FUNC(QWidget::tr("Save").toUtf8().data());
	labels->menu.save_as = MEM_STRDUP_FUNC(QWidget::tr("Save as").toUtf8().data());
	labels->menu.close = MEM_STRDUP_FUNC(QWidget::tr("Close").toUtf8().data());
	labels->menu.quit = MEM_STRDUP_FUNC(QWidget::tr("Quit").toUtf8().data());

	// 編集メニュー
	labels->menu.edit = MEM_STRDUP_FUNC(QWidget::tr("Edit").toUtf8().data());
	labels->menu.undo = MEM_STRDUP_FUNC(QWidget::tr("Undo").toUtf8().data());
	labels->menu.redo = MEM_STRDUP_FUNC(QWidget::tr("Redo").toUtf8().data());
	labels->menu.copy = MEM_STRDUP_FUNC(QWidget::tr("Copy").toUtf8().data());
	labels->menu.copy_visible = MEM_STRDUP_FUNC(QWidget::tr("Copy Visible").toUtf8().data());
	labels->menu.cut = MEM_STRDUP_FUNC(QWidget::tr("Cut").toUtf8().data());
	labels->menu.paste = MEM_STRDUP_FUNC(QWidget::tr("Paste").toUtf8().data());
	labels->menu.clip_board = MEM_STRDUP_FUNC(QWidget::tr("Clipboard").toUtf8().data());
	labels->menu.transform = MEM_STRDUP_FUNC(QWidget::tr("Transform").toUtf8().data());
	labels->menu.projection = MEM_STRDUP_FUNC(QWidget::tr("Projection").toUtf8().data());

	// キャンバスメニュー
	labels->menu.canvas = MEM_STRDUP_FUNC(QWidget::tr("Canvas").toUtf8().data());
	labels->menu.change_resolution = MEM_STRDUP_FUNC(QWidget::tr("Change Resolution").toUtf8().data());
	labels->menu.change_canvas_size = MEM_STRDUP_FUNC(QWidget::tr("Change Canvas Size").toUtf8().data());
	labels->menu.flip_canvas_horizontally = MEM_STRDUP_FUNC(QWidget::tr("Flip Horizontally").toUtf8().data());
	labels->menu.flip_canvas_vertically = MEM_STRDUP_FUNC(QWidget::tr("Flip Vertically").toUtf8().data());
	labels->menu.change_2nd_bg_color = MEM_STRDUP_FUNC(QWidget::tr("Change Second Background Color").toUtf8().data());
	labels->menu.switch_bg_color = MEM_STRDUP_FUNC(QWidget::tr("Switch Background Color").toUtf8().data());
	labels->menu.canvas_icc = MEM_STRDUP_FUNC(QWidget::tr("Change ICC Profile").toUtf8().data());

	// レイヤーメニュー
	labels->menu.layer = MEM_STRDUP_FUNC(QWidget::tr("Layer").toUtf8().data());
	labels->menu.new_color = MEM_STRDUP_FUNC(QWidget::tr("New(Color)").toUtf8().data());
	labels->menu.new_vector = MEM_STRDUP_FUNC(QWidget::tr("New(Vector)").toUtf8().data());
	labels->menu.new_layer_set = MEM_STRDUP_FUNC(QWidget::tr("New Layer Set").toUtf8().data());
	labels->menu.new_3d_modeling = MEM_STRDUP_FUNC(QWidget::tr("New 3D Modeling").toUtf8().data());
	labels->menu.new_adjustment_layer = MEM_STRDUP_FUNC(QWidget::tr("New Adjustment Layer").toUtf8().data());
	labels->menu.new_layer_group = MEM_STRDUP_FUNC(QWidget::tr("New Layer Group").toUtf8().data());
	labels->menu.copy_layer = MEM_STRDUP_FUNC(QWidget::tr("Copy Layer").toUtf8().data());
	labels->menu.delete_layer = MEM_STRDUP_FUNC(QWidget::tr("Delete Layer").toUtf8().data());
	labels->menu.merge_down_layer = MEM_STRDUP_FUNC(QWidget::tr("Merge Down Layer").toUtf8().data());
	labels->menu.merge_all_layer = MEM_STRDUP_FUNC(QWidget::tr("Merge All Layer").toUtf8().data());
	labels->menu.fill_layer_fg_color = MEM_STRDUP_FUNC(QWidget::tr("Fill Layer with Foreground Color").toUtf8().data());
	labels->menu.fill_layer_pattern = MEM_STRDUP_FUNC(QWidget::tr("Fill Layer with Pattern").toUtf8().data());
	labels->menu.rasterize_layer = MEM_STRDUP_FUNC(QWidget::tr("Rasterize Layer").toUtf8().data());
	labels->layer_window.alpha_to_select = MEM_STRDUP_FUNC(QWidget::tr("Alpha to Selection Area").toUtf8().data());
	labels->layer_window.alpha_add_select = MEM_STRDUP_FUNC(QWidget::tr("Alpha Add to Selection Area").toUtf8().data());
	labels->layer_window.pasted_layer = MEM_STRDUP_FUNC(QWidget::tr("Pasted Layer").toUtf8().data());
	labels->layer_window.under_layer = MEM_STRDUP_FUNC(QWidget::tr("Under Layer").toUtf8().data());
	labels->layer_window.mixed_under_layer = MEM_STRDUP_FUNC(QWidget::tr("Mixed Under Layer").toUtf8().data());
	labels->menu.visible2layer = MEM_STRDUP_FUNC(QWidget::tr("Visible to Layer").toUtf8().data());
	labels->menu.visible_copy = MEM_STRDUP_FUNC(QWidget::tr("Visible Copy").toUtf8().data());

	// 選択メニュー
	labels->menu.select = MEM_STRDUP_FUNC(QWidget::tr("Selection").toUtf8().data());
	labels->menu.select_none = MEM_STRDUP_FUNC(QWidget::tr("None").toUtf8().data());
	labels->menu.select_invert = MEM_STRDUP_FUNC(QWidget::tr("Invert Selection Area").toUtf8().data());
	labels->menu.select_all = MEM_STRDUP_FUNC(QWidget::tr("Select All").toUtf8().data());
	labels->menu.selection_extend = MEM_STRDUP_FUNC(QWidget::tr("Extend Selection Area").toUtf8().data());
	labels->menu.selection_reduct = MEM_STRDUP_FUNC(QWidget::tr("Reduct Selection Area").toUtf8().data());

	// ビューメニュー
	labels->menu.view = MEM_STRDUP_FUNC(QWidget::tr("View").toUtf8().data());
	labels->menu.zoom = MEM_STRDUP_FUNC(QWidget::tr("Zoom").toUtf8().data());
	labels->menu.zoom_in = MEM_STRDUP_FUNC(QWidget::tr("Zoom in").toUtf8().data());
	labels->menu.zoom_out = MEM_STRDUP_FUNC(QWidget::tr("Zoom out").toUtf8().data());
	labels->menu.zoom_reset = MEM_STRDUP_FUNC(QWidget::tr("Reset Zoom").toUtf8().data());
	labels->menu.rotate = MEM_STRDUP_FUNC(QWidget::tr("Rotate").toUtf8().data());
	labels->menu.reset_rotate = MEM_STRDUP_FUNC(QWidget::tr("Reset Rotate").toUtf8().data());
	labels->menu.reverse_horizontally = MEM_STRDUP_FUNC(QWidget::tr("Reverse View Horizontally").toUtf8().data());
	labels->menu.display_filter = MEM_STRDUP_FUNC(QWidget::tr("Display Filter").toUtf8().data());
	labels->menu.no_filter = MEM_STRDUP_FUNC(QWidget::tr("View No Filter").toUtf8().data());
	labels->menu.gray_scale = MEM_STRDUP_FUNC(QWidget::tr("Gray Scale").toUtf8().data());
	labels->menu.gray_scale_yiq = MEM_STRDUP_FUNC(QWidget::tr("Gray Scale YIQ").toUtf8().data());

	// フィルタメニュー
	labels->menu.filters = MEM_STRDUP_FUNC(QWidget::tr("Filters").toUtf8().data());
	labels->menu.blur = MEM_STRDUP_FUNC(QWidget::tr("Blur").toUtf8().data());
	labels->menu.motion_blur = MEM_STRDUP_FUNC(QWidget::tr("Motion Blur").toUtf8().data());
	labels->menu.gaussian_blur = MEM_STRDUP_FUNC(QWidget::tr("Gaussian Blur").toUtf8().data());
	labels->filter.straight_random = MEM_STRDUP_FUNC(QWidget::tr("Straight Random").toUtf8().data());
	labels->filter.bidirection = MEM_STRDUP_FUNC(QWidget::tr("Bidirection").toUtf8().data());
	labels->menu.bright_contrast = MEM_STRDUP_FUNC(QWidget::tr("Bright Contrast").toUtf8().data());
	labels->menu.hue_saturtion = MEM_STRDUP_FUNC(QWidget::tr("Hue Saturation").toUtf8().data());
	labels->menu.color_levels = MEM_STRDUP_FUNC(QWidget::tr("Color Levels").toUtf8().data());
	labels->menu.tone_curve = MEM_STRDUP_FUNC(QWidget::tr("Tone Curve").toUtf8().data());
	labels->menu.luminosity2opacity = MEM_STRDUP_FUNC(QWidget::tr("Luminosity to Opacity").toUtf8().data());
	labels->menu.color2alpha = MEM_STRDUP_FUNC(QWidget::tr("Color to Alpha").toUtf8().data());
	labels->menu.colorize_with_under = MEM_STRDUP_FUNC(QWidget::tr("Colorize with Under").toUtf8().data());
	labels->menu.gradation_map = MEM_STRDUP_FUNC(QWidget::tr("Gradation Map").toUtf8().data());
	labels->menu.detect_max = MEM_STRDUP_FUNC(QWidget::tr("Detect Max").toUtf8().data());
	labels->menu.tranparancy_as_white = MEM_STRDUP_FUNC(QWidget::tr("Transparency as White").toUtf8().data());
	labels->menu.fill_with_vector = MEM_STRDUP_FUNC(QWidget::tr("Fill with Vector").toUtf8().data());
	labels->menu.render = MEM_STRDUP_FUNC(QWidget::tr("Render").toUtf8().data());
	labels->menu.cloud = MEM_STRDUP_FUNC(QWidget::tr("Cloud").toUtf8().data());
	labels->menu.fractal = MEM_STRDUP_FUNC(QWidget::tr("Fractal").toUtf8().data());
	labels->menu.trace_pixels = MEM_STRDUP_FUNC(QWidget::tr("Trace Pixel").toUtf8().data());

	// 新規作成ウィンドウ
	labels->make_new.title = MEM_STRDUP_FUNC(QWidget::tr("New").toUtf8().data());
	labels->make_new.name = MEM_STRDUP_FUNC(QWidget::tr("Name").toUtf8().data());
	labels->make_new.width = MEM_STRDUP_FUNC(QWidget::tr("Width").toUtf8().data());
	labels->make_new.height = MEM_STRDUP_FUNC(QWidget::tr("Height").toUtf8().data());
	labels->make_new.second_bg_color = MEM_STRDUP_FUNC(QWidget::tr("Second Background Color").toUtf8().data());
	labels->make_new.adopt_icc_profile = MEM_STRDUP_FUNC(QWidget::tr("Adopt ICC Profile").toUtf8().data());
	labels->make_new.preset = MEM_STRDUP_FUNC(QWidget::tr("Preset").toUtf8().data());
	labels->make_new.add_preset = MEM_STRDUP_FUNC(QWidget::tr("Add Preset").toUtf8().data());
	labels->make_new.swap_height_and_width = MEM_STRDUP_FUNC(QWidget::tr("Swap Height and Width").toUtf8().data());

	// ツールボックス
	labels->tool_box.title = MEM_STRDUP_FUNC(QWidget::tr("Tool Box").toUtf8().data());
	labels->tool_box.initialize = MEM_STRDUP_FUNC(QWidget::tr("Initialize").toUtf8().data());
	labels->tool_box.new_brush = MEM_STRDUP_FUNC(QWidget::tr("New Brush").toUtf8().data());
	labels->tool_box.smooth = MEM_STRDUP_FUNC(QWidget::tr("Smooth").toUtf8().data());
	labels->tool_box.smooth_quality = MEM_STRDUP_FUNC(QWidget::tr("Smooth Quality").toUtf8().data());
	labels->tool_box.smooth_rate = MEM_STRDUP_FUNC(QWidget::tr("Smooth Rate").toUtf8().data());
	labels->tool_box.smooth_gaussian = MEM_STRDUP_FUNC(QWidget::tr("Smooth Gaussian").toUtf8().data());
	labels->tool_box.smooth_average = MEM_STRDUP_FUNC(QWidget::tr("Smooth Average").toUtf8().data());
	labels->tool_box.base_scale = MEM_STRDUP_FUNC(QWidget::tr("Magnification").toUtf8().data());
	labels->tool_box.brush_scale = MEM_STRDUP_FUNC(QWidget::tr("Brush Size").toUtf8().data());
	labels->tool_box.scale = MEM_STRDUP_FUNC(QWidget::tr("Scale").toUtf8().data());
	labels->tool_box.flow = MEM_STRDUP_FUNC(QWidget::tr("Flow").toUtf8().data());
	labels->tool_box.pressure = MEM_STRDUP_FUNC(QWidget::tr("Pressure").toUtf8().data());
	labels->tool_box.extend = MEM_STRDUP_FUNC(QWidget::tr("Range Extend").toUtf8().data());
	labels->tool_box.blur = MEM_STRDUP_FUNC(QWidget::tr("Blur").toUtf8().data());
	labels->tool_box.outline_hardness = MEM_STRDUP_FUNC(QWidget::tr("Outline Hardness").toUtf8().data());
	labels->tool_box.color_extend = MEM_STRDUP_FUNC(QWidget::tr("Color Extend").toUtf8().data());
	labels->tool_box.select_move_start = MEM_STRDUP_FUNC(QWidget::tr("Select Move Start").toUtf8().data());
	labels->tool_box.anti_alias = MEM_STRDUP_FUNC(QWidget::tr("Anti Alias").toUtf8().data());
	labels->tool_box.select.rgb = MEM_STRDUP_FUNC(QWidget::tr("Select RGB").toUtf8().data());
	labels->tool_box.select.rgba = MEM_STRDUP_FUNC(QWidget::tr("Select RGBA").toUtf8().data());
	labels->tool_box.select.alpha = MEM_STRDUP_FUNC(QWidget::tr("Select Alpha").toUtf8().data());
	labels->tool_box.select.active_layer = MEM_STRDUP_FUNC(QWidget::tr("Select Active layer").toUtf8().data());
	labels->tool_box.select.canvas = MEM_STRDUP_FUNC(QWidget::tr("Select Canvas").toUtf8().data());
	labels->tool_box.select.under_layer = MEM_STRDUP_FUNC(QWidget::tr("Select Under layer").toUtf8().data());
	labels->tool_box.select.threshold = MEM_STRDUP_FUNC(QWidget::tr("Select Threshold").toUtf8().data());
	labels->tool_box.select.target = MEM_STRDUP_FUNC(QWidget::tr("Select Target").toUtf8().data());
	labels->tool_box.select.area = MEM_STRDUP_FUNC(QWidget::tr("Select Area").toUtf8().data());
	labels->tool_box.select.detect_area = MEM_STRDUP_FUNC(QWidget::tr("Select Area Size").toUtf8().data());
	labels->tool_box.select.area_normal = MEM_STRDUP_FUNC(QWidget::tr("Area Normal").toUtf8().data());
	labels->tool_box.select.area_large = MEM_STRDUP_FUNC(QWidget::tr("Area Large").toUtf8().data());
	labels->tool_box.change_text_color = MEM_STRDUP_FUNC(QWidget::tr("Change Text Color").toUtf8().data());
	labels->tool_box.text_horizonal = MEM_STRDUP_FUNC(QWidget::tr("Text Horizonal").toUtf8().data());
	labels->tool_box.text_vertical = MEM_STRDUP_FUNC(QWidget::tr("Text Vertical").toUtf8().data());
	labels->tool_box.text_style = MEM_STRDUP_FUNC(QWidget::tr("Text Style").toUtf8().data());
	labels->tool_box.text_bold = MEM_STRDUP_FUNC(QWidget::tr("Text Bold").toUtf8().data());
	labels->tool_box.text_normal = MEM_STRDUP_FUNC(QWidget::tr("Text Normal").toUtf8().data());
	labels->tool_box.text_italic = MEM_STRDUP_FUNC(QWidget::tr("Text Italic").toUtf8().data());
	labels->tool_box.text_oblique = MEM_STRDUP_FUNC(QWidget::tr("Text Oblique").toUtf8().data());
	labels->tool_box.has_balloon = MEM_STRDUP_FUNC(QWidget::tr("Has Balloon").toUtf8().data());
	labels->tool_box.balloon_has_edge = MEM_STRDUP_FUNC(QWidget::tr("Balloon has Edge").toUtf8().data());
	labels->tool_box.line_color = MEM_STRDUP_FUNC(QWidget::tr("Line Color").toUtf8().data());
	labels->tool_box.fill_color = MEM_STRDUP_FUNC(QWidget::tr("Fill Color").toUtf8().data());
	labels->tool_box.line_width = MEM_STRDUP_FUNC(QWidget::tr("Line Width").toUtf8().data());
	labels->tool_box.centering_horizontally = MEM_STRDUP_FUNC(QWidget::tr("Centering Horizontally").toUtf8().data());
	labels->tool_box.centering_vertically = MEM_STRDUP_FUNC(QWidget::tr("Centering Vertically").toUtf8().data());
	labels->tool_box.adjust_range_to_text = MEM_STRDUP_FUNC(QWidget::tr("Adjust Range to Text").toUtf8().data());
	labels->tool_box.num_edge = MEM_STRDUP_FUNC(QWidget::tr("Num Edge").toUtf8().data());
	labels->tool_box.edge_size = MEM_STRDUP_FUNC(QWidget::tr("Edge Size").toUtf8().data());
	labels->tool_box.random_edge_size = MEM_STRDUP_FUNC(QWidget::tr("Edge Size Random").toUtf8().data());
	labels->tool_box.random_edge_distance = MEM_STRDUP_FUNC(QWidget::tr("Edge Distance Random").toUtf8().data());
	labels->tool_box.num_children = MEM_STRDUP_FUNC(QWidget::tr("Num Children").toUtf8().data());
	labels->tool_box.start_child_size = MEM_STRDUP_FUNC(QWidget::tr("Start Child Size").toUtf8().data());
	labels->tool_box.end_child_size = MEM_STRDUP_FUNC(QWidget::tr("End Child Size").toUtf8().data());
	labels->tool_box.reverse = MEM_STRDUP_FUNC(QWidget::tr("Reverse").toUtf8().data());
	labels->tool_box.reverse_horizontally = MEM_STRDUP_FUNC(QWidget::tr("Reverse Horizontally").toUtf8().data());
	labels->tool_box.reverse_vertically = MEM_STRDUP_FUNC(QWidget::tr("Reverse Vertically").toUtf8().data());
	labels->tool_box.blend_mode = MEM_STRDUP_FUNC(QWidget::tr("Blend Mode").toUtf8().data());
	labels->tool_box.hue = MEM_STRDUP_FUNC(QWidget::tr("Hue").toUtf8().data());
	labels->tool_box.saturation = MEM_STRDUP_FUNC(QWidget::tr("Saturation").toUtf8().data());
	labels->tool_box.brightness = MEM_STRDUP_FUNC(QWidget::tr("Brightness").toUtf8().data());
	labels->tool_box.contrast = MEM_STRDUP_FUNC(QWidget::tr("Cotrast").toUtf8().data());
	labels->tool_box.distance = MEM_STRDUP_FUNC(QWidget::tr("Distance").toUtf8().data());
	labels->tool_box.rotate_start = MEM_STRDUP_FUNC(QWidget::tr("Rotate Start").toUtf8().data());
	labels->tool_box.rotate_speed = MEM_STRDUP_FUNC(QWidget::tr("Rotate Speed").toUtf8().data());
	labels->tool_box.rotate_to_brush_direction = MEM_STRDUP_FUNC(QWidget::tr("Rotate to Brush Direction").toUtf8().data());
	labels->tool_box.random_rotate = MEM_STRDUP_FUNC(QWidget::tr("Random Rotate").toUtf8().data());
	labels->tool_box.rotate_range = MEM_STRDUP_FUNC(QWidget::tr("Rotate Range").toUtf8().data());
	labels->tool_box.random_size = MEM_STRDUP_FUNC(QWidget::tr("Random Size").toUtf8().data());
	labels->tool_box.size_range = MEM_STRDUP_FUNC(QWidget::tr("Size Range").toUtf8().data());
	labels->tool_box.pick_mode = MEM_STRDUP_FUNC(QWidget::tr("Pick Mode").toUtf8().data());
	labels->tool_box.single_pixels = MEM_STRDUP_FUNC(QWidget::tr("Single Pixel").toUtf8().data());
	labels->tool_box.average_color = MEM_STRDUP_FUNC(QWidget::tr("Average Color").toUtf8().data());
	labels->tool_box.clockwise = MEM_STRDUP_FUNC(QWidget::tr("Clockwise").toUtf8().data());
	labels->tool_box.counter_clockwise = MEM_STRDUP_FUNC(QWidget::tr("Counter Clockwise").toUtf8().data());
	labels->tool_box.min_degree = MEM_STRDUP_FUNC(QWidget::tr("Minimum Degree").toUtf8().data());
	labels->tool_box.min_distance = MEM_STRDUP_FUNC(QWidget::tr("Minimum Distance").toUtf8().data());
	labels->tool_box.min_pressure = MEM_STRDUP_FUNC(QWidget::tr("Minimum Pressure").toUtf8().data());
	labels->tool_box.enter = MEM_STRDUP_FUNC(QWidget::tr("Enter").toUtf8().data());
	labels->tool_box.out = MEM_STRDUP_FUNC(QWidget::tr("Out").toUtf8().data());
	labels->tool_box.enter_out = MEM_STRDUP_FUNC(QWidget::tr("Enter and Out").toUtf8().data());
	labels->tool_box.mix = MEM_STRDUP_FUNC(QWidget::tr("Mix").toUtf8().data());
	labels->tool_box.gradation_reverse = MEM_STRDUP_FUNC(QWidget::tr("Reverse Foreground to Background").toUtf8().data());
	labels->tool_box.devide_stroke = MEM_STRDUP_FUNC(QWidget::tr("Devide Stroke").toUtf8().data());
	labels->tool_box.delete_stroke = MEM_STRDUP_FUNC(QWidget::tr("Delete Stroke").toUtf8().data());
	labels->tool_box.open_path = MEM_STRDUP_FUNC(QWidget::tr("Open Path").toUtf8().data());
	labels->tool_box.close_path = MEM_STRDUP_FUNC(QWidget::tr("Close Path").toUtf8().data());
	labels->tool_box.target = MEM_STRDUP_FUNC(QWidget::tr("Target").toUtf8().data());
	labels->tool_box.stroke = MEM_STRDUP_FUNC(QWidget::tr("Stroke").toUtf8().data());
	labels->tool_box.prior_angle = MEM_STRDUP_FUNC(QWidget::tr("Prior Angle").toUtf8().data());
	labels->tool_box.transform_free = MEM_STRDUP_FUNC(QWidget::tr("Transform Free").toUtf8().data());
	labels->tool_box.transform_scale = MEM_STRDUP_FUNC(QWidget::tr("Transform Scale").toUtf8().data());
	labels->tool_box.transform_free_shape = MEM_STRDUP_FUNC(QWidget::tr("Transform Free Shape").toUtf8().data());
	labels->tool_box.transform_rotate = MEM_STRDUP_FUNC(QWidget::tr("Transform Rotate").toUtf8().data());
	labels->tool_box.preference = MEM_STRDUP_FUNC(QWidget::tr("Preference").toUtf8().data());
	labels->tool_box.name = MEM_STRDUP_FUNC(QWidget::tr("Name").toUtf8().data());
	labels->tool_box.copy_brush = MEM_STRDUP_FUNC(QWidget::tr("Copy Brush").toUtf8().data());
	labels->tool_box.change_brush = MEM_STRDUP_FUNC(QWidget::tr("Change Brush").toUtf8().data());
	labels->tool_box.delete_brush = MEM_STRDUP_FUNC(QWidget::tr("Delete Brush").toUtf8().data());
	labels->tool_box.control_point = MEM_STRDUP_FUNC(QWidget::tr("Control Point").toUtf8().data());
	labels->tool_box.control.select = MEM_STRDUP_FUNC(QWidget::tr("Control Select Point").toUtf8().data());
	labels->tool_box.control.move = MEM_STRDUP_FUNC(QWidget::tr("Control Move Point").toUtf8().data());
	labels->tool_box.control.change_pressure = MEM_STRDUP_FUNC(QWidget::tr("Control Point Change Pressure").toUtf8().data());
	labels->tool_box.control.delete_point = MEM_STRDUP_FUNC(QWidget::tr("Delete Control Point").toUtf8().data());
	labels->tool_box.control.move_stroke = MEM_STRDUP_FUNC(QWidget::tr("Move Stroke").toUtf8().data());
	labels->tool_box.control.copy_stroke = MEM_STRDUP_FUNC(QWidget::tr("Copy and Move Sroke").toUtf8().data());
	labels->tool_box.control.joint_stroke = MEM_STRDUP_FUNC(QWidget::tr("Joint Stroke").toUtf8().data());
	labels->tool_box.texture = MEM_STRDUP_FUNC(QWidget::tr("Texture").toUtf8().data());
	labels->tool_box.texture_strength = MEM_STRDUP_FUNC(QWidget::tr("Strength of Texture").toUtf8().data());
	labels->tool_box.no_texture = MEM_STRDUP_FUNC(QWidget::tr("No Texture").toUtf8().data());
	labels->tool_box.pallete_add = MEM_STRDUP_FUNC(QWidget::tr("Add Color").toUtf8().data());
	labels->tool_box.pallete_delete = MEM_STRDUP_FUNC(QWidget::tr("Delete Color").toUtf8().data());
	labels->tool_box.load_pallete = MEM_STRDUP_FUNC(QWidget::tr("Load Pallete").toUtf8().data());
	labels->tool_box.load_pallete_after = MEM_STRDUP_FUNC(QWidget::tr("Add Pallete").toUtf8().data());
	labels->tool_box.write_pallete = MEM_STRDUP_FUNC(QWidget::tr("Load Pallete").toUtf8().data());
	labels->tool_box.clear_pallete = MEM_STRDUP_FUNC(QWidget::tr("Clear Pallete").toUtf8().data());
	labels->tool_box.update = MEM_STRDUP_FUNC(QWidget::tr("Update").toUtf8().data());
	labels->tool_box.frequency = MEM_STRDUP_FUNC(QWidget::tr("Frequency").toUtf8().data());
	labels->tool_box.cloud_color = MEM_STRDUP_FUNC(QWidget::tr("Color of Cloud").toUtf8().data());
	labels->tool_box.persistence = MEM_STRDUP_FUNC(QWidget::tr("Persistence").toUtf8().data());
	labels->tool_box.rand_seed = MEM_STRDUP_FUNC(QWidget::tr("Random Seed").toUtf8().data());
	labels->tool_box.use_random = MEM_STRDUP_FUNC(QWidget::tr("Use Random").toUtf8().data());
	labels->tool_box.update_immediately = MEM_STRDUP_FUNC(QWidget::tr("Update Immediately").toUtf8().data());
	labels->tool_box.num_octaves = MEM_STRDUP_FUNC(QWidget::tr("Number of Octaves").toUtf8().data());
	labels->tool_box.linear = MEM_STRDUP_FUNC(QWidget::tr("Linear").toUtf8().data());
	labels->tool_box.cosine = MEM_STRDUP_FUNC(QWidget::tr("Cosine").toUtf8().data());
	labels->tool_box.cubic = MEM_STRDUP_FUNC(QWidget::tr("Cubic").toUtf8().data());
	labels->tool_box.colorize = MEM_STRDUP_FUNC(QWidget::tr("Colorize").toUtf8().data());
	labels->tool_box.start_edit_3d = MEM_STRDUP_FUNC(QWidget::tr("Start Edit 3D Model").toUtf8().data());
	labels->tool_box.end_edit_3d = MEM_STRDUP_FUNC(QWidget::tr("End Edit 3D Model").toUtf8().data());
	labels->tool_box.scatter = MEM_STRDUP_FUNC(QWidget::tr("Scatter").toUtf8().data());
	labels->tool_box.scatter_amount = MEM_STRDUP_FUNC(QWidget::tr("Scatter Amount").toUtf8().data());
	labels->tool_box.scatter_size = MEM_STRDUP_FUNC(QWidget::tr("Scatter Size").toUtf8().data());
	labels->tool_box.scatter_range = MEM_STRDUP_FUNC(QWidget::tr("Scatter Range").toUtf8().data());
	labels->tool_box.scatter_random_size = MEM_STRDUP_FUNC(QWidget::tr("Scatter Random Size").toUtf8().data());
	labels->tool_box.scatter_random_flow = MEM_STRDUP_FUNC(QWidget::tr("Scatter Random Flow").toUtf8().data());
	labels->tool_box.draw_scatter_only = MEM_STRDUP_FUNC(QWidget::tr("Draw Scatter Only").toUtf8().data());
	labels->tool_box.shape.circle = MEM_STRDUP_FUNC(QWidget::tr("Shape Circle").toUtf8().data());
	labels->tool_box.shape.eclipse = MEM_STRDUP_FUNC(QWidget::tr("Shape Eclipse").toUtf8().data());
	labels->tool_box.shape.triangle = MEM_STRDUP_FUNC(QWidget::tr("Shape Triangle").toUtf8().data());
	labels->tool_box.shape.square = MEM_STRDUP_FUNC(QWidget::tr("Shape Square").toUtf8().data());
	labels->tool_box.shape.rhombus = MEM_STRDUP_FUNC(QWidget::tr("Shape Rhombus").toUtf8().data());
	labels->tool_box.shape.hexagon = MEM_STRDUP_FUNC(QWidget::tr("Shape Hexagon").toUtf8().data());
	labels->tool_box.shape.star = MEM_STRDUP_FUNC(QWidget::tr("Shape Star").toUtf8().data());
	labels->tool_box.shape.pattern = MEM_STRDUP_FUNC(QWidget::tr("Shape Pattern").toUtf8().data());
	labels->tool_box.shape.image = MEM_STRDUP_FUNC(QWidget::tr("Shape Image").toUtf8().data());
	labels->tool_box.bevel = MEM_STRDUP_FUNC(QWidget::tr("Bevel").toUtf8().data());
	labels->tool_box.round = MEM_STRDUP_FUNC(QWidget::tr("Round").toUtf8().data());
	labels->tool_box.miter = MEM_STRDUP_FUNC(QWidget::tr("Miter").toUtf8().data());
	labels->tool_box.manually_set = MEM_STRDUP_FUNC(QWidget::tr("Manually Set").toUtf8().data());
	labels->tool_box.brush_chain = MEM_STRDUP_FUNC(QWidget::tr("Brush Chain").toUtf8().data());
	labels->tool_box.change_brush_chain_key = MEM_STRDUP_FUNC(QWidget::tr("Change Brush Chain Key").toUtf8().data());
	labels->tool_box.use_old_anti_alias = MEM_STRDUP_FUNC(QWidget::tr("Use Old Anti Alias").toUtf8().data());

	// レイヤーウィンドウ
	labels->layer_window.title = MEM_STRDUP_FUNC(QWidget::tr("Layer View").toUtf8().data());
	labels->layer_window.new_layer = MEM_STRDUP_FUNC(QWidget::tr("Normal Layer").toUtf8().data());
	labels->layer_window.new_vector = MEM_STRDUP_FUNC(QWidget::tr("Vector Layer").toUtf8().data());
	labels->layer_window.new_layer_set = MEM_STRDUP_FUNC(QWidget::tr("Layer Set").toUtf8().data());
	labels->layer_window.new_3d_modeling = MEM_STRDUP_FUNC(QWidget::tr("3D Modeling").toUtf8().data());
	labels->layer_window.new_text = MEM_STRDUP_FUNC(QWidget::tr("Text Layer").toUtf8().data());
	labels->layer_window.new_adjsutment_layer = MEM_STRDUP_FUNC(QWidget::tr("Adjustment Layer").toUtf8().data());
	labels->layer_window.add_normal = MEM_STRDUP_FUNC(QWidget::tr("Add Normal Layer").toUtf8().data());
	labels->layer_window.add_vector = MEM_STRDUP_FUNC(QWidget::tr("Add Vector Layer").toUtf8().data());
	labels->layer_window.add_layer_set = MEM_STRDUP_FUNC(QWidget::tr("Add Layer Set").toUtf8().data());
	labels->layer_window.add_3d_modeling = MEM_STRDUP_FUNC(QWidget::tr("Add 3D Modeling").toUtf8().data());
	labels->layer_window.add_adjustment_layer = MEM_STRDUP_FUNC(QWidget::tr("Add Adjustment Layer").toUtf8().data());
	labels->layer_window.rename = MEM_STRDUP_FUNC(QWidget::tr("Rename").toUtf8().data());
	labels->layer_window.reorder = MEM_STRDUP_FUNC(QWidget::tr("Reorder").toUtf8().data());
	labels->layer_window.blend_mode = MEM_STRDUP_FUNC(QWidget::tr("Blend Mode").toUtf8().data());
	labels->layer_window.opacity = MEM_STRDUP_FUNC(QWidget::tr("Opacity").toUtf8().data());
	labels->layer_window.mask_with_under = MEM_STRDUP_FUNC(QWidget::tr("Mask with Under layer").toUtf8().data());
	labels->layer_window.lock_opacity = MEM_STRDUP_FUNC(QWidget::tr("Lock Opacity").toUtf8().data());
	labels->layer_window.add_under_active_layer = MEM_STRDUP_FUNC(QWidget::tr("Add Under Active Layer").toUtf8().data());
	labels->layer_window.layer_group_must_have_name = MEM_STRDUP_FUNC(QWidget::tr("Layer group must have name").toUtf8().data());
	labels->layer_window.same_group_name = MEM_STRDUP_FUNC(QWidget::tr("Same group name").toUtf8().data());
	labels->layer_window.same_layer_name = MEM_STRDUP_FUNC(QWidget::tr("Same Layer Name").toUtf8().data());
	// レイヤーの合成モード
	labels->layer_window.blend_modes[LAYER_BLEND_NORMAL] = MEM_STRDUP_FUNC(QWidget::tr("Normal").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_ADD] = MEM_STRDUP_FUNC(QWidget::tr("Add").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_MULTIPLY] = MEM_STRDUP_FUNC(QWidget::tr("Multiply").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_SCREEN] = MEM_STRDUP_FUNC(QWidget::tr("Screen").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_OVERLAY] = MEM_STRDUP_FUNC(QWidget::tr("Overlay").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_LIGHTEN] = MEM_STRDUP_FUNC(QWidget::tr("Lighten").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_DARKEN] = MEM_STRDUP_FUNC(QWidget::tr("Darken").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_DODGE] = MEM_STRDUP_FUNC(QWidget::tr("Dodge").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_BURN] = MEM_STRDUP_FUNC(QWidget::tr("Burn").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_HARD_LIGHT] = MEM_STRDUP_FUNC(QWidget::tr("Hard Light").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_SOFT_LIGHT] = MEM_STRDUP_FUNC(QWidget::tr("Soft Light").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_DIFFERENCE] = MEM_STRDUP_FUNC(QWidget::tr("Difference").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_EXCLUSION] = MEM_STRDUP_FUNC(QWidget::tr("Exclusion").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_HSL_HUE] = MEM_STRDUP_FUNC(QWidget::tr("Hue").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_HSL_SATURATION] = MEM_STRDUP_FUNC(QWidget::tr("Saturation").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_HSL_COLOR] = MEM_STRDUP_FUNC(QWidget::tr("Color").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_HSL_LUMINOSITY] = MEM_STRDUP_FUNC(QWidget::tr("Luminosity").toUtf8().data());
	labels->layer_window.blend_modes[LAYER_BLEND_BINALIZE] = MEM_STRDUP_FUNC(QWidget::tr("Binalize").toUtf8().data());

	// 保存ウィンドウ用
	labels->save.compress = MEM_STRDUP_FUNC(QWidget::tr("Compress").toUtf8().data());
	labels->save.quality = MEM_STRDUP_FUNC(QWidget::tr("Quality").toUtf8().data());
	labels->save.write_alpha = MEM_STRDUP_FUNC(QWidget::tr("Write Alpha").toUtf8().data());
	labels->save.write_profile = MEM_STRDUP_FUNC(QWidget::tr("Write Profile").toUtf8().data());
	labels->save.close_with_unsaved_chage = MEM_STRDUP_FUNC(QWidget::tr("Close with unsaved change?").toUtf8().data());
	labels->save.quit_with_unsaved_change = MEM_STRDUP_FUNC(QWidget::tr("Quit with unsaved change?").toUtf8().data());
	labels->save.recover_backup = MEM_STRDUP_FUNC(QWidget::tr("There are backup files. Do you recover?").toUtf8().data());

	// ナビゲーション用
	labels->navigation.title = MEM_STRDUP_FUNC(QWidget::tr("Navigation").toUtf8().data());

	// プラグイン
	labels->menu.plug_in = MEM_STRDUP_FUNC(QWidget::tr("Plug in").toUtf8().data());

	// スクリプト
	labels->menu.script = MEM_STRDUP_FUNC(QWidget::tr("Script").toUtf8().data());

	// ヘルプメニュー
	labels->menu.help = MEM_STRDUP_FUNC(QWidget::tr("Help").toUtf8().data());
	labels->menu.version = MEM_STRDUP_FUNC(QWidget::tr("Version Infomation").toUtf8().data());

	// 環境設定
	labels->preference.title = MEM_STRDUP_FUNC(QWidget::tr("Preference").toUtf8().data());
	labels->preference.base_setting = MEM_STRDUP_FUNC(QWidget::tr("Basic").toUtf8().data());
	labels->preference.theme = MEM_STRDUP_FUNC(QWidget::tr("Theme").toUtf8().data());
	labels->preference.default_theme = MEM_STRDUP_FUNC(QWidget::tr("Default").toUtf8().data());
	labels->preference.auto_save = MEM_STRDUP_FUNC(QWidget::tr("Auto Save").toUtf8().data());
	labels->status_bar.auto_save = MEM_STRDUP_FUNC(QWidget::tr("Auto Saving...").toUtf8().data());
	labels->preference.conflict_hot_key = MEM_STRDUP_FUNC(QWidget::tr("This shortcut key is being used").toUtf8().data());
	labels->preference.language = MEM_STRDUP_FUNC(QWidget::tr("Language").toUtf8().data());
	labels->preference.set_back_ground = MEM_STRDUP_FUNC(QWidget::tr("Change Canvas Background Color").toUtf8().data());
	labels->preference.show_preview_on_taskbar = MEM_STRDUP_FUNC(QWidget::tr("Show Preview on Taskbar").toUtf8().data());
	labels->preference.backup_path = MEM_STRDUP_FUNC(QWidget::tr("Backup Folder").toUtf8().data());
	labels->preference.motion_buffer_size = MEM_STRDUP_FUNC(QWidget::tr("Cursor motion Buffer Size").toUtf8().data());
	labels->preference.scale_and_move_with_touch = MEM_STRDUP_FUNC(QWidget::tr("Scale and Move with Touch").toUtf8().data());
	labels->preference.draw_with_touch = MEM_STRDUP_FUNC(QWidget::tr("Draw with Touch").toUtf8().data());
	labels->preference.layer_window_scrollbar_place_left = MEM_STRDUP_FUNC(QWidget::tr("Layer Window's Scrollbar places Left").toUtf8().data());
	labels->preference.gui_scale = MEM_STRDUP_FUNC(QWidget::tr("GUI Scale").toUtf8().data());
	labels->preference.add_brush_chain = MEM_STRDUP_FUNC(QWidget::tr("Add Brush Set").toUtf8().data());


	// ブラシのデフォルト名
	for(i=0; i<NUM_BRUSH_TYPE; i++)
	{
		switch(i)
		{
		case BRUSH_TYPE_PENCIL:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Pencil").toUtf8().data());
			break;
		case BRUSH_TYPE_HARD_PEN:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Hard Pen").toUtf8().data());
			break;
		case BRUSH_TYPE_AIR_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Air Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_BLEND_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Blend Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_OLD_AIR_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Old Air Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_WATER_COLOR_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Water Color Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_PICKER_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Picker Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_ERASER:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Eraser").toUtf8().data());
			break;
		case BRUSH_TYPE_BUCKET:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Bucket").toUtf8().data());
			break;
		case BRUSH_TYPE_PATTERN_FILL:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Pattern Fill").toUtf8().data());
			break;
		case BRUSH_TYPE_BLUR:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Blur").toUtf8().data());
			break;
		case BRUSH_TYPE_SMUDGE:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Smudge").toUtf8().data());
			break;
		case BRUSH_TYPE_MIX_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Mix Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_GRADATION:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Gradation").toUtf8().data());
			break;
		case BRUSH_TYPE_TEXT:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Text").toUtf8().data());
			break;
		case BRUSH_TYPE_STAMP_TOOL:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Stamp Tool").toUtf8().data());
			break;
		case BRUSH_TYPE_IMAGE_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Image Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_BLEND_IMAGE_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Blend Image Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_PICKER_IMAGE_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Picker Image Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_SCRIPT_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Script Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_CUSTOM_BRUSH:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Custom Brush").toUtf8().data());
			break;
		case BRUSH_TYPE_PLUG_IN:
			labels->tool_box.brush_default_names[i] = MEM_STRDUP_FUNC(QWidget::tr("Plug-in Brush").toUtf8().data());
			break;
		}
	}

	LoadFractalLabels(fractal_labels, labels);
}

void LoadFractalLabels(
	FRACTAL_LABEL* labels,
	APPLICATION_LABELS* app_label
)
{
	labels->triangle = MEM_STRDUP_FUNC(QWidget::tr("Triangle").toUtf8().data());
	labels->transform = MEM_STRDUP_FUNC(QWidget::tr("Transform").toUtf8().data());
	labels->variations = MEM_STRDUP_FUNC(QWidget::tr("Variations").toUtf8().data());
	labels->colors = MEM_STRDUP_FUNC(QWidget::tr("Colors").toUtf8().data());
	labels->random = MEM_STRDUP_FUNC(QWidget::tr("Random").toUtf8().data());
	labels->weight = MEM_STRDUP_FUNC(QWidget::tr("Weight").toUtf8().data());
	labels->symmetry = MEM_STRDUP_FUNC(QWidget::tr("Symmetry").toUtf8().data());
	labels->linear = MEM_STRDUP_FUNC(QWidget::tr("Linear").toUtf8().data());
	labels->sinusoidal = MEM_STRDUP_FUNC(QWidget::tr("Sinusoidal").toUtf8().data());
	labels->spherical = MEM_STRDUP_FUNC(QWidget::tr("Spherical").toUtf8().data());
	labels->swirl = MEM_STRDUP_FUNC(QWidget::tr("Swirl").toUtf8().data());
	labels->horseshoe = MEM_STRDUP_FUNC(QWidget::tr("Horseshoe").toUtf8().data());
	labels->polar = MEM_STRDUP_FUNC(QWidget::tr("Polar").toUtf8().data());
	labels->handkerchief = MEM_STRDUP_FUNC(QWidget::tr("handkerchief").toUtf8().data());
	labels->heart = MEM_STRDUP_FUNC(QWidget::tr("Heart").toUtf8().data());
	labels->disc = MEM_STRDUP_FUNC(QWidget::tr("Disc").toUtf8().data());
	labels->spiral = MEM_STRDUP_FUNC(QWidget::tr("Spiral").toUtf8().data());
	labels->hyperbolic = MEM_STRDUP_FUNC(QWidget::tr("Hyperbolic").toUtf8().data());
	labels->diamond = MEM_STRDUP_FUNC(QWidget::tr("Diamond").toUtf8().data());
	labels->ex = MEM_STRDUP_FUNC(QWidget::tr("EX").toUtf8().data());
	labels->julia = MEM_STRDUP_FUNC(QWidget::tr("Julia").toUtf8().data());
	labels->bent = MEM_STRDUP_FUNC(QWidget::tr("Bent").toUtf8().data());
	labels->waves = MEM_STRDUP_FUNC(QWidget::tr("Waves").toUtf8().data());
	labels->fisheye = MEM_STRDUP_FUNC(QWidget::tr("Fisheve").toUtf8().data());
	labels->popcorn = MEM_STRDUP_FUNC(QWidget::tr("Popcorn").toUtf8().data());
	labels->preserve_weight = MEM_STRDUP_FUNC(QWidget::tr("Preserve Weight").toUtf8().data());
	labels->update = app_label->tool_box.update;
	labels->update_immediately = app_label->tool_box.update_immediately;
	labels->adjust = MEM_STRDUP_FUNC(QWidget::tr("Adjust").toUtf8().data());
}
