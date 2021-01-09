#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <wlr/render/egl.h>
#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>

/**
 * Usage: keyboard-shortcuts-inhibit
 * Creates a xdg-toplevel using the keyboard-shortcuts-inhibit protocol.
 * It will be solid green, when it has an keyboard shortcuts inhibitor, and
 * solid yellow if it does not.
 * Left click with a pointer will toggle this state. (Touch is not supported
 * for now).
 * The compositor (de-)activating the inhibitor will also toggle state.
 * With a compositor supporting the protocol, compositor shortcuts will be
 * suspended while the inhibitor is active and the window has focus.
 */

static int width = 500, height = 300;

static struct wl_compositor *compositor = NULL;
static struct wl_seat *seat = NULL;
static struct xdg_wm_base *wm_base = NULL;
static struct zwp_keyboard_shortcuts_inhibit_manager_v1 *
	keyboard_shortcuts_inhibit_manager = NULL;
static struct zwp_keyboard_shortcuts_inhibitor_v1 *
	keyboard_shortcuts_inhibitor = NULL;
static bool active = false;

struct wlr_egl egl;
struct wl_egl_window *egl_window;
struct wlr_egl_surface *egl_surface;

static void draw(void) {
	eglMakeCurrent(egl.display, egl_surface, egl_surface, egl.context);

	float color[] = {1.0, 1.0, 0.0, 1.0};
	if (keyboard_shortcuts_inhibitor) {
		color[0] = 0.0;
	}

	glViewport(0, 0, width, height);
	glClearColor(color[0], color[1], color[2], 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	eglSwapBuffers(egl.display, egl_surface);
}

static void keyboard_shortcuts_inhibit_handle_active(void *data,
		struct zwp_keyboard_shortcuts_inhibitor_v1 *
			zwp_keyboard_shortcuts_inhibitor_v1) {
	active = 1;
	draw();
}

static void keyboard_shortcuts_inhibit_handle_inactive(void *data,
		struct zwp_keyboard_shortcuts_inhibitor_v1 *
			zwp_keyboard_shortcuts_inhibitor_v1) {
	active = 0;
	draw();
}

static const struct zwp_keyboard_shortcuts_inhibitor_v1_listener
keyboard_shortcuts_inhibitor_listener = {
	.active = keyboard_shortcuts_inhibit_handle_active,
	.inactive = keyboard_shortcuts_inhibit_handle_inactive,
};

static void pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
		uint32_t time, uint32_t button, uint32_t state_w) {
	struct wl_surface *surface = data;

	if (button == BTN_LEFT && state_w == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (keyboard_shortcuts_inhibitor) {
			zwp_keyboard_shortcuts_inhibitor_v1_destroy(
					keyboard_shortcuts_inhibitor);
			keyboard_shortcuts_inhibitor = NULL;
			active = false;
		} else {
			keyboard_shortcuts_inhibitor =
				zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(
					keyboard_shortcuts_inhibit_manager, surface, seat);
			zwp_keyboard_shortcuts_inhibitor_v1_add_listener(
					keyboard_shortcuts_inhibitor,
					&keyboard_shortcuts_inhibitor_listener, NULL);
		}
	}

	draw();
}

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// This space intentionally left blank
}

static void pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	// This space intentionally left blank
}

static void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// This space intentionally left blank
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value) {
	// This space intentionally left blank
}

static void pointer_handle_frame(void *data, struct wl_pointer *wl_pointer) {
	// This space intentionally left blank
}

static void pointer_handle_axis_source(void *data,
		struct wl_pointer *wl_pointer, uint32_t axis_source) {
	// This space intentionally left blank
}

static void pointer_handle_axis_stop(void *data,
		struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
	// This space intentionally left blank
}

static void pointer_handle_axis_discrete(void *data,
		struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
	// This space intentionally left blank
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.motion = pointer_handle_motion,
	.button = pointer_handle_button,
	.axis = pointer_handle_axis,
	.frame = pointer_handle_frame,
	.axis_source = pointer_handle_axis_source,
	.axis_stop = pointer_handle_axis_stop,
	.axis_discrete = pointer_handle_axis_discrete,
};

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
	wl_egl_window_resize(egl_window, width, height, 0, 0);
	draw();
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {
	width = w;
	height = h;
}

static void xdg_toplevel_handle_close(void *data,
		struct xdg_toplevel *xdg_toplevel) {
	exit(EXIT_SUCCESS);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, "wl_compositor") == 0) {
		compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	} else if (strcmp(interface, zwp_keyboard_shortcuts_inhibit_manager_v1_interface.name) == 0) {
		keyboard_shortcuts_inhibit_manager = wl_registry_bind(registry, name,
			&zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

int main(int argc, char **argv) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "Failed to create display\n");
		return EXIT_FAILURE;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (compositor == NULL) {
		fprintf(stderr, "wl-compositor not available\n");
		return EXIT_FAILURE;
	}
	if (wm_base == NULL) {
		fprintf(stderr, "xdg-shell not available\n");
		return EXIT_FAILURE;
	}
	if (keyboard_shortcuts_inhibit_manager == NULL) {
		fprintf(stderr, "keyboard-shortcuts-inhibit not available\n");
		return EXIT_FAILURE;
	}

	EGLint attribs[] = { EGL_NONE };
	wlr_egl_init(&egl, EGL_PLATFORM_WAYLAND_EXT, display, attribs);

	struct wl_surface *surface = wl_compositor_create_surface(compositor);
	struct xdg_surface *xdg_surface =
		xdg_wm_base_get_xdg_surface(wm_base, surface);
	struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

	struct wl_pointer *pointer = wl_seat_get_pointer(seat);
	wl_pointer_add_listener(pointer, &pointer_listener, surface);

	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

	wl_surface_commit(surface);

	egl_window = wl_egl_window_create(surface, width, height);
	egl_surface = wlr_egl_create_surface(&egl, egl_window);

	wl_display_roundtrip(display);

	keyboard_shortcuts_inhibitor =
		zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(
				keyboard_shortcuts_inhibit_manager, surface, seat);
	zwp_keyboard_shortcuts_inhibitor_v1_add_listener(
			keyboard_shortcuts_inhibitor,
			&keyboard_shortcuts_inhibitor_listener, NULL);

	draw();

	while (wl_display_dispatch(display) != -1) {
		// This space intentionally left blank
	}

	return EXIT_SUCCESS;
}
