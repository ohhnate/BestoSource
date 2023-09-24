/**************************************************************************/
/*  editor_log.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "editor_log.h"

#include "core/debugger/debugger_marshalls.h"
#include "core/object/undo_redo.h"
#include "core/os/keyboard.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/debugger/script_editor_debugger.h"
#include "scene/gui/center_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/resources/font.h"

void EditorLog::_error_handler(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
	EditorLog *self = static_cast<EditorLog *>(p_self);

	String err_str;
	if (p_errorexp && p_errorexp[0]) {
		err_str = String::utf8(p_errorexp);
	} else {
		err_str = String::utf8(p_file) + ":" + itos(p_line) + " - " + String::utf8(p_error);
	}

	if (p_editor_notify) {
		err_str += " (User)";
	}

	MessageType message_type = p_type == ERR_HANDLER_WARNING ? MSG_TYPE_WARNING : MSG_TYPE_ERROR;

	if (self->current != Thread::get_caller_id()) {
		callable_mp(self, &EditorLog::add_message).bind(err_str, message_type).call_deferred();
	}
	else {
		self->add_message(err_str, message_type);
	}
}

void EditorLog::_update_theme() {
	type_filter_map[MSG_TYPE_STD]->toggle_button->set_icon(get_editor_theme_icon(SNAME("Popup")));
	type_filter_map[MSG_TYPE_ERROR]->toggle_button->set_icon(get_editor_theme_icon(SNAME("StatusError")));
	type_filter_map[MSG_TYPE_WARNING]->toggle_button->set_icon(get_editor_theme_icon(SNAME("StatusWarning")));
	type_filter_map[MSG_TYPE_EDITOR]->toggle_button->set_icon(get_editor_theme_icon(SNAME("Edit")));

	type_filter_map[MSG_TYPE_STD]->toggle_button->set_theme_type_variation("EditorLogFilterButton");
	type_filter_map[MSG_TYPE_ERROR]->toggle_button->set_theme_type_variation("EditorLogFilterButton");
	type_filter_map[MSG_TYPE_WARNING]->toggle_button->set_theme_type_variation("EditorLogFilterButton");
	type_filter_map[MSG_TYPE_EDITOR]->toggle_button->set_theme_type_variation("EditorLogFilterButton");

	btn_clear->set_icon(get_editor_theme_icon(SNAME("Clear")));
	collapse_button->set_icon(get_editor_theme_icon(SNAME("CombineLines")));
	search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));

	theme_cache.error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	theme_cache.error_icon = get_editor_theme_icon(SNAME("Error"));
	theme_cache.warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
	theme_cache.warning_icon = get_editor_theme_icon(SNAME("Warning"));
	theme_cache.message_color = get_theme_color(SNAME("font_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.6);

	const Ref<Font> normal_font = get_theme_font(SNAME("output_source"), EditorStringName(EditorFonts));
	if (normal_font.is_valid()) {
		log_stack_trace_display->add_theme_font_override("normal_font", normal_font);
	}

	const Ref<Font> bold_font = get_theme_font(SNAME("output_source_bold"), EditorStringName(EditorFonts));
	if (bold_font.is_valid()) {
		log_stack_trace_display->add_theme_font_override("bold_font", bold_font);
	}

	const Ref<Font> italics_font = get_theme_font(SNAME("output_source_italic"), EditorStringName(EditorFonts));
	if (italics_font.is_valid()) {
		log_stack_trace_display->add_theme_font_override("italics_font", italics_font);
	}

	const Ref<Font> bold_italics_font = get_theme_font(SNAME("output_source_bold_italic"), EditorStringName(EditorFonts));
	if (bold_italics_font.is_valid()) {
		log_stack_trace_display->add_theme_font_override("bold_italics_font", bold_italics_font);
	}

	const Ref<Font> mono_font = get_theme_font(SNAME("output_source_mono"), EditorStringName(EditorFonts));
	if (mono_font.is_valid()) {
		log_stack_trace_display->add_theme_font_override("mono_font", mono_font);
	}

	// Disable padding for highlighted background/foreground to prevent highlights from overlapping on close lines.
	// This also better matches terminal output, which does not use any form of padding.
	log_stack_trace_display->add_theme_constant_override("text_highlight_h_padding", 0);
	log_stack_trace_display->add_theme_constant_override("text_highlight_v_padding", 0);

	const int font_size = get_theme_font_size(SNAME("output_source_size"), EditorStringName(EditorFonts));
	log_stack_trace_display->add_theme_font_size_override("normal_font_size", font_size);
	log_stack_trace_display->add_theme_font_size_override("bold_font_size", font_size);
	log_stack_trace_display->add_theme_font_size_override("italics_font_size", font_size);
	log_stack_trace_display->add_theme_font_size_override("mono_font_size", font_size);
}

void EditorLog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_theme();
			_load_state();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
			_rebuild_log();
		} break;
	}
}

void EditorLog::_set_collapse(bool p_collapse) {
	collapse = p_collapse;
	_start_state_save_timer();
	_rebuild_log();
}

void EditorLog::_start_state_save_timer() {
	if (!is_loading_state) {
		save_state_timer->start();
	}
}

void EditorLog::_save_state() {
	Ref<ConfigFile> config;
	config.instantiate();
	// Load and amend existing config if it exists.
	config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));

	const String section = "editor_log";
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		config->set_value(section, "log_filter_" + itos(E.key), E.value->is_active());
	}

	config->set_value(section, "collapse", collapse);
	config->set_value(section, "show_search", search_box->is_visible());

	config->save(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
}

void EditorLog::_load_state() {
	is_loading_state = true;

	Ref<ConfigFile> config;
	config.instantiate();
	config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));

	// Run the below code even if config->load returns an error, since we want the defaults to be set even if the file does not exist yet.
	const String section = "editor_log";
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		E.value->set_active(config->get_value(section, "log_filter_" + itos(E.key), true));
	}

	collapse = config->get_value(section, "collapse", false);
	bool show_search = config->get_value(section, "show_search", true);
	search_box->set_visible(show_search);

	is_loading_state = false;
}

void EditorLog::_clear_request() {
	messages.clear();
	_reset_message_counts();
	log_stack_trace_display->set_text("");
	/*log->clear();
	tool_button->set_icon(Ref<Texture2D>());*/
	int child_count = log_buttons_holder->get_child_count();
	for (int i = 0; i < child_count; i++)
	{
		Node *childref = log_buttons_holder->get_child(0);
		log_buttons_holder->remove_child(childref);
		childref->queue_free();
	}
}

void EditorLog::clear() {
	_clear_request();
}

void EditorLog::_process_message(const String &p_msg, MessageType p_type) {
	if (messages.size() > 0 && messages[messages.size() - 1].text == p_msg && messages[messages.size() - 1].type == p_type) {
		// If previous message is the same as the new one, increase previous count rather than adding another
		// instance to the messages list.
		LogMessage &previous = messages.write[messages.size() - 1];
		previous.count++;

		_add_log_line(previous, collapse);
	} else {
		// Different message to the previous one received.
		LogMessage message(p_msg, p_type);
		_add_log_line(message);
		messages.push_back(message);
	}

	type_filter_map[p_type]->set_message_count(type_filter_map[p_type]->get_message_count() + 1);
}

void EditorLog::add_message(const String &p_msg, MessageType p_type) {
	// Rin Iota:
	// Removed spliting in to multiple lines.
	// Not an issue for a BEConsole, only causes the message to be split in to 4 of them, which is fucked af.
	_process_message(p_msg, p_type, false);
}

void EditorLog::set_tool_button(Button *p_tool_button) {
	tool_button = p_tool_button;
}

void EditorLog::register_undo_redo(UndoRedo *p_undo_redo) {
	p_undo_redo->set_commit_notify_callback(_undo_redo_cbk, this);
}

void EditorLog::_undo_redo_cbk(void *p_self, const String &p_name) {
	EditorLog *self = static_cast<EditorLog *>(p_self);
	self->add_message(p_name, EditorLog::MSG_TYPE_EDITOR);
}

void EditorLog::_rebuild_log() {
	int child_count = log_buttons_holder->get_child_count();
	for (int i = 0; i < child_count; i++)
	{
		Node* childref = log_buttons_holder->get_child(0);
		log_buttons_holder->remove_child(childref);
		childref->queue_free();
	}

	for (int msg_idx = 0; msg_idx < messages.size(); msg_idx++) {
		LogMessage msg = messages[msg_idx];

		if (collapse) {
			// If collapsing, only log one instance of the message.
			_add_log_line(msg);
		} else {
			// If not collapsing, log each instance on a line.
			for (int i = 0; i < msg.count; i++) {
				_add_log_line(msg);
			}
		}
	}
}

void EditorLog::_add_log_line(LogMessage &p_message, bool p_replace_previous) {
	if (!is_inside_tree()) {
		// The log will be built all at once when it enters the tree and has its theme items.
		return;
	}

	// Only add the message to the log if it passes the filters.
	bool filter_active = type_filter_map[p_message.type]->is_active();
	String search_text = search_box->get_text();
	bool search_match = search_text.is_empty() || p_message.text.findn(search_text) > -1;

	if (!filter_active || !search_match) {
		return;
	}

	switch (p_message.type) {
		case MSG_TYPE_STD: {
			_config_log_button(memnew(RichTextLabel), p_message);
		} break;
		case MSG_TYPE_STD_RICH: {
			_config_log_button(memnew(RichTextLabel), p_message);
		} break;
		case MSG_TYPE_ERROR: {
			_config_log_button(memnew(RichTextLabel), p_message);
		} break;
		case MSG_TYPE_WARNING: {
			_config_log_button(memnew(RichTextLabel), p_message);
		} break;
		case MSG_TYPE_EDITOR: {
			_config_log_button(memnew(RichTextLabel), p_message);
		} break;
	}


	if (p_replace_previous) {
		// Force sync last line update (skip if number of unprocessed log messages is too large to avoid editor lag).
		while (!log_buttons_holder->is_ready()) {
			::OS::get_singleton()->delay_usec(1);
		}
	}
}

void EditorLog::_set_filter_active(bool p_active, MessageType p_message_type) {
	type_filter_map[p_message_type]->set_active(p_active);
	_start_state_save_timer();
	_rebuild_log();
}

void EditorLog::_set_search_visible(bool p_visible) {
	search_box->set_visible(p_visible);
	if (p_visible) {
		search_box->grab_focus();
	}
	_start_state_save_timer();
}

void EditorLog::_search_changed(const String &p_text) {
	_rebuild_log();
}

void EditorLog::_reset_message_counts() {
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		E.value->set_message_count(0);
	}
}

void EditorLog::_log_button_clicked(String value) {
	log_stack_trace_display->set_text(value);
}

void EditorLog::_config_log_button(RichTextLabel *log_button, LogMessage &p_message) {

	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);

	// Color the message's time in to appropriate color (depending on message type)
	String count = "";
	String color_start = "";
	String color_end = "[/color]";

	switch (p_message.type) {
		case MSG_TYPE_STD: {
			color_start = "[color=white]";
		} break;
		case MSG_TYPE_STD_RICH: {
			color_start = "[color=white]";
		} break;
		case MSG_TYPE_EDITOR: {
			color_start = "[color=cyan]";
		} break;
		case MSG_TYPE_ERROR: {
			color_start = "[color=red]";
		} break;
		case MSG_TYPE_WARNING: {
			color_start = "[color=yellow]";
		} break;
	}

	if (p_message.count > 1 && collapse)
	{
		count = "[b][i](" + itos(p_message.count) + ")[/i][/b] ";
	}

	String text_to_process = "";
	String stack_trace_bit = "";
	Vector<String> console_message = p_message.text.split("||");
	if (console_message.size() > 0 && console_message.size() > 1)
	{
		text_to_process = console_message[0];
		stack_trace_bit = console_message[0] + "\n" + console_message[1];
	}
	else
	{
		text_to_process = p_message.text;
		stack_trace_bit = p_message.text;
	}

	log_button->set_use_bbcode(true);
	log_button->set_fit_content(true);
	log_button->set_h_size_flags(SIZE_EXPAND_FILL);
	log_button->set_v_size_flags(SIZE_SHRINK_CENTER);
	log_button->set_anchors_preset(PRESET_TOP_WIDE);
	log_button->set_size(Size2(1280, 720));
	log_button->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	log_button->set_text(color_start + count + "[" + (String)buf + "]" + color_end + " " + text_to_process);
	Button* log_actual_button = memnew(Button);
	log_button->add_child(log_actual_button);
	log_actual_button->set_flat(true);
	log_actual_button->set_h_size_flags(SIZE_EXPAND_FILL);
	log_actual_button->set_v_size_flags(SIZE_EXPAND_FILL);
	log_actual_button->set_anchors_preset(PRESET_FULL_RECT);
	log_actual_button->connect("pressed", callable_mp(this, &EditorLog::_set_trace_text).bind(stack_trace_bit));

	const Ref<Font> normal_font = get_theme_font(SNAME("output_source"), EditorStringName(EditorFonts));
	if (normal_font.is_valid()) {
		log_button->add_theme_font_override("normal_font", normal_font);
	}

	const Ref<Font> bold_font = get_theme_font(SNAME("output_source_bold"), EditorStringName(EditorFonts));
	if (bold_font.is_valid()) {
		log_button->add_theme_font_override("bold_font", bold_font);
	}

	const Ref<Font> italics_font = get_theme_font(SNAME("output_source_italic"), EditorStringName(EditorFonts));
	if (italics_font.is_valid()) {
		log_button->add_theme_font_override("italics_font", italics_font);
	}

	const Ref<Font> bold_italics_font = get_theme_font(SNAME("output_source_bold_italic"), EditorStringName(EditorFonts));
	if (bold_italics_font.is_valid()) {
		log_button->add_theme_font_override("bold_italics_font", bold_italics_font);
	}

	const Ref<Font> mono_font = get_theme_font(SNAME("output_source_mono"), EditorStringName(EditorFonts));
	if (mono_font.is_valid()) {
		log_button->add_theme_font_override("mono_font", mono_font);
	}

	// Disable padding for highlighted background/foreground to prevent highlights from overlapping on close lines.
	// This also better matches terminal output, which does not use any form of padding.
	log_button->add_theme_constant_override("text_highlight_h_padding", 0);
	log_button->add_theme_constant_override("text_highlight_v_padding", 0);

	const int font_size = get_theme_font_size(SNAME("output_source_size"), EditorStringName(EditorFonts));
	log_button->add_theme_font_size_override("normal_font_size", font_size);
	log_button->add_theme_font_size_override("bold_font_size", font_size);
	log_button->add_theme_font_size_override("italics_font_size", font_size);
	log_button->add_theme_font_size_override("mono_font_size", font_size);

	log_buttons_holder->add_child(log_button);
	log_buttons_holder->move_child(log_button, 0);
}

void EditorLog::_set_trace_text(String text)
{
	String text_but_no_line = text.replace(":line ", ":").replace(":line", ":");
	String reconstructed_string = "";
	bool detected_a_link = false;

	for (int i = 0; i < text_but_no_line.length(); i++)
	{
		if (i < text_but_no_line.length() + 3 && text_but_no_line[i + 1] == ':' && text_but_no_line[i + 2] == '\\')
		{
			if (detected_a_link == false)
			{
				detected_a_link = true;
				String link_color = "[color=ADD8E6][url]";
				for (int j = 0; j < link_color.length(); j++)
				{
					reconstructed_string += link_color[j];
				}
			}
		}

		if (detected_a_link == true && text_but_no_line[i] == '\n')
		{
			String link_color = "[/url][/color]";
			for (int j = 0; j < link_color.length(); j++)
			{
				reconstructed_string += link_color[j];
			}

			detected_a_link = false;
		}
		reconstructed_string += text_but_no_line[i];
	}

	log_stack_trace_display->set_text(reconstructed_string);
}

void EditorLog::_open_script_editor(Variant file_path_and_line)
{
	String filePath = ((String)file_path_and_line).replace(" ", "").replace("/", "\\").replace("\n", "");
	String path = EDITOR_GET("text_editor/external/exec_path");

	// File path example
	// C:/Projects/DeezNuts/ass_code.cs:69
	//  first :						   second :
	Vector<String> file_path_real = filePath.split(":");
	String line_string;
	for (int i = 0; i < file_path_real[2].size(); i++)
	{
		if (is_digit(file_path_real[2][i]))
		{
			line_string += file_path_real[2][i];
		}
	}

	List<String> arguments;
	arguments.push_front(file_path_real[0] + ":" + file_path_real[1] + ":" + line_string);
	arguments.push_front("--goto");

	OS::get_singleton()->create_process(path, arguments);
}

EditorLog::EditorLog() {
	save_state_timer = memnew(Timer);
	save_state_timer->set_wait_time(2);
	save_state_timer->set_one_shot(true);
	save_state_timer->connect("timeout", callable_mp(this, &EditorLog::_save_state));
	add_child(save_state_timer);

	// Rin Iota:
	// This is where Console UI get created
	Control *control = this;
	control->set_name("ConsoleUI");

	VBoxContainer* container = memnew(VBoxContainer);
	container->set_name("Vertical Layout");
	control->add_child(container);
	container->set_h_size_flags(SIZE_EXPAND_FILL);
	container->set_v_size_flags(SIZE_EXPAND_FILL);
	container->set_anchors_preset(PRESET_FULL_RECT);


	// Filter buttons
	BoxContainer *top_buttons = memnew(BoxContainer);
	top_buttons->set_name("Top Buttons");
	container->add_child(top_buttons);
	top_buttons->set_h_size_flags(SIZE_EXPAND_FILL);
	top_buttons->set_v_size_flags(SIZE_SHRINK_BEGIN);
	top_buttons->set_anchors_preset(PRESET_TOP_WIDE);
	top_buttons->set_size(Size2(1280, 31));
	top_buttons->set_alignment(ALIGNMENT_END);

#pragma region ConsoleTopButtons
	// Search box
	search_box = memnew(LineEdit);
	top_buttons->add_child(search_box);
	search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search_box->set_placeholder(TTR("Filter Messages"));
	search_box->set_clear_button_enabled(true);
	search_box->set_visible(true);
	search_box->connect("text_changed", callable_mp(this, &EditorLog::_search_changed));

	LogFilter* std_filter = memnew(LogFilter(MSG_TYPE_STD));
	std_filter->initialize_button(TTR("Toggle visibility of standard output messages."), callable_mp(this, &EditorLog::_set_filter_active));
	top_buttons->add_child(std_filter->get_button());
	type_filter_map.insert(MSG_TYPE_STD, std_filter);
	type_filter_map.insert(MSG_TYPE_STD_RICH, std_filter);

	LogFilter* error_filter = memnew(LogFilter(MSG_TYPE_ERROR));
	error_filter->initialize_button(TTR("Toggle visibility of errors."), callable_mp(this, &EditorLog::_set_filter_active));
	top_buttons->add_child(error_filter->get_button());
	type_filter_map.insert(MSG_TYPE_ERROR, error_filter);

	LogFilter* warning_filter = memnew(LogFilter(MSG_TYPE_WARNING));
	warning_filter->initialize_button(TTR("Toggle visibility of warnings."), callable_mp(this, &EditorLog::_set_filter_active));
	top_buttons->add_child(warning_filter->get_button());
	type_filter_map.insert(MSG_TYPE_WARNING, warning_filter);

	LogFilter* editor_filter = memnew(LogFilter(MSG_TYPE_EDITOR));
	editor_filter->initialize_button(TTR("Toggle visibility of editor messages."), callable_mp(this, &EditorLog::_set_filter_active));
	top_buttons->add_child(editor_filter->get_button());
	type_filter_map.insert(MSG_TYPE_EDITOR, editor_filter);

	collapse_button = memnew(Button);
	top_buttons->add_child(collapse_button);
	collapse_button->set_focus_mode(FOCUS_NONE);
	collapse_button->set_tooltip_text(TTR("Collapse duplicate messages into one log entry. Shows number of occurrences."));
	collapse_button->set_toggle_mode(true);
	collapse_button->set_pressed(false);
	collapse_button->set_text("Collapse");
	collapse_button->connect("toggled", callable_mp(this, &EditorLog::_set_collapse));

	btn_clear = memnew(Button);
	top_buttons->add_child(btn_clear);
	btn_clear->set_text("Clear");
	btn_clear->connect("pressed", callable_mp(this, &EditorLog::_clear_request));
	// connect buttons to methods
#pragma endregion

	// ConsoleLogPart
	VSplitContainer *console_log_part = memnew(VSplitContainer);
	container->add_child(console_log_part);
	console_log_part->set_h_size_flags(SIZE_EXPAND_FILL);
	console_log_part->set_v_size_flags(SIZE_EXPAND_FILL);
	console_log_part->set_anchors_preset(PRESET_FULL_RECT);
	console_log_part->set_size(Size2(1280, 682));

	// TopPanel
	PanelContainer *console_top_panel = memnew(PanelContainer);
	console_top_panel->set_h_size_flags(SIZE_FILL);
	console_top_panel->set_v_size_flags(SIZE_EXPAND_FILL);
	console_top_panel->set_anchors_preset(PRESET_FULL_RECT);
	console_top_panel->set_clip_contents(true);
	console_top_panel->set_size(Size2(1152, 470));
	console_log_part->add_child(console_top_panel);

	// BottomPanel
	PanelContainer *console_bottom_panel = memnew(PanelContainer);
	console_bottom_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	console_bottom_panel->set_v_size_flags(SIZE_EXPAND_FILL);
	console_bottom_panel->set_anchors_preset(PRESET_FULL_RECT);
	console_bottom_panel->set_clip_contents(true);
	console_bottom_panel->set_size(Size2(1152, 32));
	console_bottom_panel->set_clip_contents(true);
	console_bottom_panel->set_custom_minimum_size(Size2(0, 16));
	console_log_part->add_child(console_bottom_panel);

	ScrollContainer *scroll_container = memnew(ScrollContainer);
	scroll_container->set_h_size_flags(SIZE_EXPAND_FILL);
	scroll_container->set_anchors_and_offsets_preset(PRESET_CENTER, PRESET_MODE_MINSIZE);
	scroll_container->set_follow_focus(true);
	scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	console_top_panel->add_child(scroll_container);

	log_buttons_holder = memnew(VBoxContainer);
	scroll_container->add_child(log_buttons_holder);
	log_buttons_holder->set_h_size_flags(SIZE_EXPAND_FILL);
	log_buttons_holder->set_anchors_and_offsets_preset(PRESET_TOP_WIDE, PRESET_MODE_MINSIZE);
	//log_buttons_holder->set_anchors_preset(PRESET_FULL_RECT);

	log_stack_trace_display = memnew(RichTextLabel);
	console_bottom_panel->add_child(log_stack_trace_display);
	log_stack_trace_display->set_use_bbcode(true);
	log_stack_trace_display->set_scroll_active(true);
	log_stack_trace_display->set_context_menu_enabled(true);
	log_stack_trace_display->set_selection_enabled(true);
	log_stack_trace_display->connect("meta_clicked", callable_mp(this, &EditorLog::_open_script_editor));

	add_message(VERSION_FULL_NAME " (c) 2007-present Juan Linietsky, Ariel Manzur & Godot Contributors.");

	eh.errfunc = _error_handler;
	eh.userdata = this;
	add_error_handler(&eh);

	current = Thread::get_caller_id();
}

void EditorLog::deinit() {
	remove_error_handler(&eh);
}

EditorLog::~EditorLog() {
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		// MSG_TYPE_STD_RICH is connected to the std_filter button, so we do this
		// to avoid it from being deleted twice, causing a crash on closing.
		if (E.key != MSG_TYPE_STD_RICH) {
			memdelete(E.value);
		}
	}
}
