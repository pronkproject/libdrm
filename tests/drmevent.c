/*
 * Copyright 2026 Red Hat, Inc.
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "xf86drm.h"

struct test_event {
	struct drm_event base;
	uint64_t payload;
};

struct callback_state {
	int fd;
	uint64_t payload;
	unsigned int calls;
};

static void
handle_unhandled_event(int fd, const struct drm_event *event, void *user_data)
{
	const struct test_event *test_event = (const struct test_event *)event;
	struct callback_state *state = user_data;

	assert(fd == state->fd);
	assert(event->type == 0x7fffffff);
	assert(event->length == sizeof(*test_event));
	state->payload = test_event->payload;
	state->calls++;
}

int
main(void)
{
	const struct test_event event = {
		.base = {
			.type = 0x7fffffff,
			.length = sizeof(event),
		},
		.payload = UINT64_C(0x123456789abcdef0),
	};
	drmEventContext context;
	struct callback_state state = { 0 };
	int pipe_fds[2];
	ssize_t written;

	assert(pipe(pipe_fds) == 0);
	state.fd = pipe_fds[0];
	memset(&context, 0, sizeof(context));
	context.version = DRM_EVENT_CONTEXT_VERSION;
	context.unhandled_event_handler = handle_unhandled_event;
	context.unhandled_event_handler_data = &state;

	written = write(pipe_fds[1], &event, sizeof(event));
	assert(written == sizeof(event));
	assert(drmHandleEvent(pipe_fds[0], &context) == 0);
	assert(state.calls == 1);
	assert(state.payload == event.payload);

	close(pipe_fds[0]);
	close(pipe_fds[1]);
	return 0;
}
