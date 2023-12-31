/**************************************************************************/
/*  test_viewport.h                                                       */
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

#ifndef TEST_VIEWPORT_H
#define TEST_VIEWPORT_H

#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestViewport {

class NotificationControl : public Control {
	GDCLASS(NotificationControl, Control);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_MOUSE_ENTER: {
				mouse_over = true;
			} break;

			case NOTIFICATION_MOUSE_EXIT: {
				mouse_over = false;
			} break;
		}
	}

public:
	bool mouse_over = false;
};

// `NotificationControl`-derived class that additionally
// - allows start Dragging
// - stores mouse information of last event
class DragStart : public NotificationControl {
	GDCLASS(DragStart, NotificationControl);

public:
	MouseButton last_mouse_button;
	Point2i last_mouse_move_position;
	StringName drag_data_name = SNAME("Drag Data");

	virtual Variant get_drag_data(const Point2 &p_point) override {
		return drag_data_name;
	}

	virtual void gui_input(const Ref<InputEvent> &p_event) override {
		Ref<InputEventMouseButton> mb = p_event;
		if (mb.is_valid()) {
			last_mouse_button = mb->get_button_index();
			return;
		}

		Ref<InputEventMouseMotion> mm = p_event;
		if (mm.is_valid()) {
			last_mouse_move_position = mm->get_position();
			return;
		}
	}
};

// `NotificationControl`-derived class that acts as a Drag and Drop target.
class DragTarget : public NotificationControl {
	GDCLASS(DragTarget, NotificationControl);

public:
	Variant drag_data;
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override {
		StringName string_data = p_data;
		// Verify drag data is compatible.
		if (string_data != SNAME("Drag Data")) {
			return false;
		}
		// Only the left half is droppable area.
		if (p_point.x * 2 > get_size().x) {
			return false;
		}
		return true;
	}

	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override {
		drag_data = p_data;
	}
};

TEST_CASE("[SceneTree][Viewport] Controls and InputEvent handling") {
	DragStart *node_a = memnew(DragStart);
	Control *node_b = memnew(Control);
	Node2D *node_c = memnew(Node2D);
	DragTarget *node_d = memnew(DragTarget);
	Control *node_e = memnew(Control);
	Node *node_f = memnew(Node);
	Control *node_g = memnew(Control);

	node_a->set_name(SNAME("NodeA"));
	node_b->set_name(SNAME("NodeB"));
	node_c->set_name(SNAME("NodeC"));
	node_d->set_name(SNAME("NodeD"));
	node_e->set_name(SNAME("NodeE"));
	node_f->set_name(SNAME("NodeF"));
	node_g->set_name(SNAME("NodeG"));

	node_a->set_position(Point2i(0, 0));
	node_b->set_position(Point2i(10, 10));
	node_c->set_position(Point2i(0, 0));
	node_d->set_position(Point2i(10, 10));
	node_e->set_position(Point2i(10, 100));
	node_g->set_position(Point2i(10, 100));
	node_a->set_size(Point2i(30, 30));
	node_b->set_size(Point2i(30, 30));
	node_d->set_size(Point2i(30, 30));
	node_e->set_size(Point2i(10, 10));
	node_g->set_size(Point2i(10, 10));
	node_a->set_focus_mode(Control::FOCUS_CLICK);
	node_b->set_focus_mode(Control::FOCUS_CLICK);
	node_d->set_focus_mode(Control::FOCUS_CLICK);
	node_e->set_focus_mode(Control::FOCUS_CLICK);
	node_g->set_focus_mode(Control::FOCUS_CLICK);
	Window *root = SceneTree::get_singleton()->get_root();
	DisplayServerMock *DS = (DisplayServerMock *)(DisplayServer::get_singleton());

	// Scene tree:
	// - root
	//   - a (Control)
	//   - b (Control)
	//     - c (Node2D)
	//       - d (Control)
	//   - e (Control)
	//     - f (Node)
	//       - g (Control)
	root->add_child(node_a);
	root->add_child(node_b);
	node_b->add_child(node_c);
	node_c->add_child(node_d);
	root->add_child(node_e);
	node_e->add_child(node_f);
	node_f->add_child(node_g);

	Point2i on_a = Point2i(5, 5);
	Point2i on_b = Point2i(15, 15);
	Point2i on_d = Point2i(25, 25);
	Point2i on_e = Point2i(15, 105);
	Point2i on_g = Point2i(15, 105);
	Point2i on_background = Point2i(500, 500);
	Point2i on_outside = Point2i(-1, -1);

	// Unit tests for Viewport::gui_find_control and Viewport::_gui_find_control_at_pos
	SUBCASE("[VIEWPORT][GuiFindControl] Finding Controls at a Viewport-position") {
		// FIXME: It is extremely difficult to create a situation where the Control has a zero determinant.
		// Leaving that if-branch untested.

		SUBCASE("[VIEWPORT][GuiFindControl] Basic position tests") {
			CHECK(root->gui_find_control(on_a) == node_a);
			CHECK(root->gui_find_control(on_b) == node_b);
			CHECK(root->gui_find_control(on_d) == node_d);
			CHECK(root->gui_find_control(on_e) == node_g); // Node F makes G a Root Control at the same position as E
			CHECK(root->gui_find_control(on_g) == node_g);
			CHECK_FALSE(root->gui_find_control(on_background));
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Invisible nodes are not considered as results.") {
			// Non-Root Control
			node_d->hide();
			CHECK(root->gui_find_control(on_d) == node_b);
			// Root Control
			node_b->hide();
			CHECK(root->gui_find_control(on_b) == node_a);
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Root Control with CanvasItem as parent is affected by parent's transform.") {
			node_b->remove_child(node_c);
			node_c->set_position(Point2i(50, 50));
			root->add_child(node_c);
			CHECK(root->gui_find_control(Point2i(65, 65)) == node_d);
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Control Contents Clipping clips accessible position of children.") {
			CHECK_FALSE(node_b->is_clipping_contents());
			CHECK(root->gui_find_control(on_d + Point2i(20, 20)) == node_d);
			node_b->set_clip_contents(true);
			CHECK(root->gui_find_control(on_d) == node_d);
			CHECK_FALSE(root->gui_find_control(on_d + Point2i(20, 20)));
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Top Level Control as descendant of CanvasItem isn't affected by parent's transform.") {
			CHECK(root->gui_find_control(on_d + Point2i(20, 20)) == node_d);
			node_d->set_as_top_level(true);
			CHECK_FALSE(root->gui_find_control(on_d + Point2i(20, 20)));
			CHECK(root->gui_find_control(on_b) == node_d);
		}
	}

	SUBCASE("[Viewport][GuiInputEvent] nullptr as argument doesn't lead to a crash.") {
		ERR_PRINT_OFF;
		root->push_input(nullptr);
		ERR_PRINT_ON;
	}

	// Unit tests for Viewport::_gui_input_event (Mouse Buttons)
	SUBCASE("[Viewport][GuiInputEvent] Mouse Button Down/Up.") {
		SUBCASE("[Viewport][GuiInputEvent] Mouse Button Control Focus Change.") {
			SUBCASE("[Viewport][GuiInputEvent] Grab Focus while no Control has focus.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				// Click on A
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_a->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] Grab Focus from other Control.") {
				node_a->grab_focus();
				CHECK(node_a->has_focus());

				// Click on D
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_d->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] Non-CanvasItem breaks Transform hierarchy.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				// Click on G absolute coordinates
				SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(15, 105), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_g->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(15, 105), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] No Focus change when clicking in background.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_get_focus_owner());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

				node_a->grab_focus();
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] Mouse Button No Focus Steal while other Mouse Button is pressed.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::RIGHT, (int)MouseButtonMask::LEFT | (int)MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::RIGHT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] Allow Focus Steal with LMB while other Mouse Button is held down and was initially pressed without being over a Control.") {
				// TODO: Not sure, if this is intended behavior, but this is an edge case.
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_background, MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, (int)MouseButtonMask::LEFT | (int)MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_a->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::LEFT, (int)MouseButtonMask::LEFT | (int)MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_b->has_focus());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::RIGHT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_b->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] Ignore Focus from Mouse Buttons when mouse-filter is set to ignore.") {
				node_d->grab_focus();
				node_d->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
				CHECK(node_d->has_focus());

				// Click on overlapping area B&D.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_b->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] RMB doesn't grab focus.") {
				node_a->grab_focus();
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] LMB on unfocusable Control doesn't grab focus.") {
				CHECK_FALSE(node_g->has_focus());
				node_g->set_focus_mode(Control::FOCUS_NONE);

				SEND_GUI_MOUSE_BUTTON_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(node_g->has_focus());

				// Now verify the opposite with FOCUS_CLICK
				node_g->set_focus_mode(Control::FOCUS_CLICK);
				SEND_GUI_MOUSE_BUTTON_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_g->has_focus());
				node_g->set_focus_mode(Control::FOCUS_CLICK);
			}

			SUBCASE("[Viewport][GuiInputEvent] Signal 'gui_focus_changed' is only emitted if a previously unfocused Control grabs focus.") {
				SIGNAL_WATCH(root, SNAME("gui_focus_changed"));
				Array node_array;
				node_array.push_back(node_a);
				Array signal_args;
				signal_args.push_back(node_array);

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				SIGNAL_CHECK(SNAME("gui_focus_changed"), signal_args);

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
				SIGNAL_CHECK_FALSE(SNAME("gui_focus_changed"));

				SIGNAL_UNWATCH(root, SNAME("gui_focus_changed"));
			}

			SUBCASE("[Viewport][GuiInputEvent] Focus Propagation to parent items.") {
				SUBCASE("[Viewport][GuiInputEvent] Unfocusable Control with MOUSE_FILTER_PASS propagates focus to parent CanvasItem.") {
					node_d->set_focus_mode(Control::FOCUS_NONE);
					node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_d + Point2i(20, 20), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK(node_b->has_focus());
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d + Point2i(20, 20), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

					// Verify break condition for Root Control.
					node_a->set_focus_mode(Control::FOCUS_NONE);
					node_a->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK(node_b->has_focus());
				}

				SUBCASE("[Viewport][GuiInputEvent] Top Level CanvasItem stops focus propagation.") {
					node_d->set_focus_mode(Control::FOCUS_NONE);
					node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_c->set_as_top_level(true);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_get_focus_owner());

					node_d->set_focus_mode(Control::FOCUS_CLICK);
					SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK(node_d->has_focus());
				}
			}
		}

		SUBCASE("[Viewport][GuiInputEvent] Process-Mode affects, if GUI Mouse Button Events are processed.") {
			node_a->last_mouse_button = MouseButton::NONE;
			node_a->set_process_mode(Node::PROCESS_MODE_DISABLED);
			SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_button == MouseButton::NONE);

			// Now verify that with allowed processing the event is processed.
			node_a->set_process_mode(Node::PROCESS_MODE_ALWAYS);
			SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_button == MouseButton::LEFT);
		}
	}

	// Unit tests for Viewport::_gui_input_event (Mouse Motion)
	SUBCASE("[Viewport][GuiInputEvent] Mouse Motion") {
		// FIXME: Tooltips are not yet tested. They likely require an internal clock.

		SUBCASE("[Viewport][GuiInputEvent] Mouse Motion changes the Control, that it is over.") {
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_a->mouse_over);

			// Move over Control.
			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->mouse_over);

			// No change.
			SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(1, 1), MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->mouse_over);

			// Move over other Control.
			SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_a->mouse_over);
			CHECK(node_d->mouse_over);

			// Move to background
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_d->mouse_over);
		}

		SUBCASE("[Viewport][GuiInputEvent] Window Mouse Enter/Exit signals.") {
			SIGNAL_WATCH(root, SNAME("mouse_entered"));
			SIGNAL_WATCH(root, SNAME("mouse_exited"));
			Array signal_args;
			signal_args.push_back(Array());

			SEND_GUI_MOUSE_MOTION_EVENT(on_outside, MouseButtonMask::NONE, Key::NONE);
			SIGNAL_CHECK_FALSE(SNAME("mouse_entered"));
			SIGNAL_CHECK(SNAME("mouse_exited"), signal_args);

			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			SIGNAL_CHECK(SNAME("mouse_entered"), signal_args);
			SIGNAL_CHECK_FALSE(SNAME("mouse_exited"));

			SIGNAL_UNWATCH(root, SNAME("mouse_entered"));
			SIGNAL_UNWATCH(root, SNAME("mouse_exited"));
		}

		SUBCASE("[Viewport][GuiInputEvent] Process-Mode affects, if GUI Mouse Motion Events are processed.") {
			node_a->last_mouse_move_position = on_outside;
			node_a->set_process_mode(Node::PROCESS_MODE_DISABLED);
			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_move_position == on_outside);

			// Now verify that with allowed processing the event is processed.
			node_a->set_process_mode(Node::PROCESS_MODE_ALWAYS);
			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_move_position == on_a);
		}
	}

	// Unit tests for Viewport::_gui_input_event (Drag and Drop)
	SUBCASE("[Viewport][GuiInputEvent] Drag and Drop") {
		// FIXME: Drag-Preview will likely change. Tests for this part would have to be rewritten anyway.
		// See https://github.com/godotengine/godot/pull/67531#issuecomment-1385353430 for details.
		// FIXME: Testing Drag and Drop with non-embedded windows would require DisplayServerMock additions
		// FIXME: Drag and Drop currently doesn't work with embedded Windows and SubViewports - not testing.
		// See https://github.com/godotengine/godot/issues/28522 for example.
		int min_grab_movement = 11;
		SUBCASE("[Viewport][GuiInputEvent] Drag from one Control to another in the same viewport.") {
			SUBCASE("[Viewport][GuiInputEvent] Perform successful Drag and Drop on a different Control.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Move above a Control, that is a Drop target and allows dropping at this point.
				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				CHECK(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK(root->gui_is_drag_successful());
				CHECK((StringName)node_d->drag_data == SNAME("Drag Data"));
			}

			SUBCASE("[Viewport][GuiInputEvent] Perform unsuccessful drop on Control.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move, but don't trigger DnD yet.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(0, min_grab_movement - 1), MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move and trigger DnD.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Move above a Control, that is not a Drop target.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_FORBIDDEN);

				// Move above a Control, that is a Drop target, but has disallowed this point.
				SEND_GUI_MOUSE_MOTION_EVENT(on_d + Point2i(20, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_FORBIDDEN);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d + Point2i(20, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
			}

			SUBCASE("[Viewport][GuiInputEvent] Perform unsuccessful drop on No-Control.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move, but don't trigger DnD yet.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement - 1, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move and trigger DnD.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Move away from Controls.
				SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW); // This could also be CURSOR_FORBIDDEN.

				CHECK(root->gui_is_dragging());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
			}

			SUBCASE("[Viewport][GuiInputEvent] Perform unsuccessful drop outside of window.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move and trigger DnD.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				// Move outside of window.
				SEND_GUI_MOUSE_MOTION_EVENT(on_outside, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_outside, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
			}

			SUBCASE("[Viewport][GuiInputEvent] Drag and Drop doesn't work with other Mouse Buttons than LMB.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::MIDDLE, MouseButtonMask::MIDDLE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::MIDDLE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::MIDDLE, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] Drag and Drop parent propagation.") {
				Node2D *node_aa = memnew(Node2D);
				Control *node_aaa = memnew(Control);
				Node2D *node_dd = memnew(Node2D);
				Control *node_ddd = memnew(Control);
				node_aaa->set_size(Size2i(10, 10));
				node_aaa->set_position(Point2i(0, 5));
				node_ddd->set_size(Size2i(10, 10));
				node_ddd->set_position(Point2i(0, 5));
				node_a->add_child(node_aa);
				node_aa->add_child(node_aaa);
				node_d->add_child(node_dd);
				node_dd->add_child(node_ddd);
				Point2i on_aaa = on_a + Point2i(-2, 2);
				Point2i on_ddd = on_d + Point2i(-2, 2);

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop propagation to parent Controls.") {
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_aaa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_aaa + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_ddd, MouseButtonMask::LEFT, Key::NONE);

					CHECK(root->gui_is_dragging());
					CHECK_FALSE(root->gui_is_drag_successful());
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_ddd, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());
					CHECK(root->gui_is_drag_successful());

					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_STOP);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop grab-propagation stopped by Top Level.") {
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_aaa->set_as_top_level(true);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_aaa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_aaa + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					node_aaa->set_as_top_level(false);
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop target-propagation stopped by Top Level.") {
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_ddd->set_as_top_level(true);
					node_ddd->set_position(Point2i(30, 100));

					SEND_GUI_MOUSE_BUTTON_EVENT(on_aaa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_aaa + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(Point2i(35, 105), MouseButtonMask::LEFT, Key::NONE);

					CHECK(root->gui_is_dragging());
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(35, 105), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());
					CHECK_FALSE(root->gui_is_drag_successful());

					node_ddd->set_position(Point2i(0, 5));
					node_ddd->set_as_top_level(false);
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_STOP);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop grab-propagation stopped by non-CanvasItem.") {
					node_g->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_MOTION_EVENT(on_g + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					node_g->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop target-propagation stopped by non-CanvasItem.") {
					node_g->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_a - Point2i(1, 1), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE); // Offset for node_aaa.
					SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_g, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					node_g->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				memdelete(node_ddd);
				memdelete(node_dd);
				memdelete(node_aaa);
				memdelete(node_aa);
			}

			SUBCASE("[Viewport][GuiInputEvent] Force Drag and Drop.") {
				SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				node_a->force_drag(SNAME("Drag Data"), nullptr);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);

				// Force Drop doesn't get triggered by mouse Buttons other than LMB.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Force Drop with LMB-Down.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK(root->gui_is_drag_successful());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}
		}
	}

	memdelete(node_g);
	memdelete(node_f);
	memdelete(node_e);
	memdelete(node_d);
	memdelete(node_c);
	memdelete(node_b);
	memdelete(node_a);
}

} // namespace TestViewport

#endif // TEST_VIEWPORT_H
