/*
 * Copyright 2026 Red Hat, Inc.
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "xf86drm.h"

#define DRM_EVENT_KMS_CONSTRAINTS_LIST_CHANGED 0x04

struct constraints_event {
	struct drm_event base;
	uint32_t crtc_id;
	uint32_t flags;
	uint64_t generation;
	uint64_t reserved;
};

_Static_assert(sizeof(struct constraints_event) == 32,
	       "KMS constraints event layout must match the kernel UAPI");

struct callback_state {
	uint32_t crtc_id;
	uint32_t flags;
	uint64_t generation;
	unsigned int calls;
};

static void
handle_constraints_list_changed(int fd, uint32_t crtc_id, uint32_t flags,
				uint64_t generation, void *user_data)
{
	struct callback_state *state = user_data;

	assert(fd >= 0);
	state->crtc_id = crtc_id;
	state->flags = flags;
	state->generation = generation;
	state->calls++;
}

static int
dispatch_event(const struct constraints_event *event, drmEventContext *context)
{
	int pipe_fds[2];
	int result;

	assert(pipe(pipe_fds) == 0);
	assert(write(pipe_fds[1], event, sizeof(*event)) == sizeof(*event));
	result = drmHandleEvent(pipe_fds[0], context);
	close(pipe_fds[0]);
	close(pipe_fds[1]);
	return result;
}

int
main(void)
{
	struct constraints_event event = {
		.base = {
			.type = DRM_EVENT_KMS_CONSTRAINTS_LIST_CHANGED,
			.length = sizeof(event),
		},
		.crtc_id = 7,
		.generation = 11,
	};
	drmEventContext context = { 0 };
	struct callback_state state = { 0 };

	context.version = DRM_EVENT_CONTEXT_VERSION;
	context.kms_constraints_list_changed_handler =
		handle_constraints_list_changed;
	context.kms_constraints_list_changed_handler_data = &state;

	assert(dispatch_event(&event, &context) == 0);
	assert(state.calls == 1);
	assert(state.crtc_id == 7);
	assert(state.flags == 0);
	assert(state.generation == 11);

	event.flags = DRM_KMS_CONSTRAINTS_LIST_CLOSED;
	event.generation = 0;
	assert(dispatch_event(&event, &context) == 0);
	assert(state.calls == 2);
	assert(state.flags == DRM_KMS_CONSTRAINTS_LIST_CLOSED);
	assert(state.generation == 0);

	event.flags = 2;
	assert(dispatch_event(&event, &context) == -1);
	assert(errno == EINVAL);
	assert(state.calls == 2);
	event.flags = 0;
	event.reserved = 1;
	assert(dispatch_event(&event, &context) == -1);
	assert(errno == EINVAL);
	assert(state.calls == 2);
	event.reserved = 0;
	event.base.length--;
	assert(dispatch_event(&event, &context) == -1);
	assert(errno == EINVAL);
	assert(state.calls == 2);
	event.base.length = sizeof(event);
	event.crtc_id = 0;
	assert(dispatch_event(&event, &context) == -1);
	assert(errno == EINVAL);
	assert(state.calls == 2);
	event.crtc_id = 7;
	assert(dispatch_event(&event, &context) == -1);
	assert(errno == EINVAL);
	assert(state.calls == 2);
	event.generation = 11;
	event.flags = DRM_KMS_CONSTRAINTS_LIST_CLOSED;
	assert(dispatch_event(&event, &context) == -1);
	assert(errno == EINVAL);
	assert(state.calls == 2);

	event.flags = 0;
	event.generation = 11;
	context.version = 4;
	assert(dispatch_event(&event, &context) == 0);
	assert(state.calls == 2);
	context.version = DRM_EVENT_CONTEXT_VERSION;
	event.base.type = 0x7fffffff;
	assert(dispatch_event(&event, &context) == 0);
	assert(state.calls == 2);

	return 0;
}
