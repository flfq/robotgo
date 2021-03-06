// Copyright 2016 The go-vgo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://www.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../base/os.h"

#if defined(IS_MACOSX)
	#include "hook/darwin/input_helper_c.h"
	#include "hook/darwin/input_hook_c.h"
	#include "hook/darwin/post_event_c.h"
	#include "hook/darwin/system_properties_c.h"
#elif defined(USE_X11)
	//#define USE_XKBCOMMON 0
	#include "hook/x11/input_helper_c.h"
	#include "hook/x11/input_hook_c.h"
	#include "hook/x11/post_event_c.h"
	#include "hook/x11/system_properties_c.h"
#elif defined(IS_WINDOWS)
	#include "hook/windows/input_helper_c.h"
	#include "hook/windows/input_hook_c.h"
	#include "hook/windows/post_event_c.h"
	#include "hook/windows/system_properties_c.h"
#endif

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// #include <uiohook.h>
#include "hook/uiohook.h"

int aStop();
int aEvent(char *aevent);

bool logger_proc(unsigned int level, const char *format, ...) {
	bool status = false;

	va_list args;
	switch (level) {
		#ifdef USE_DEBUG
		case LOG_LEVEL_DEBUG:
		case LOG_LEVEL_INFO:
			va_start(args, format);
			status = vfprintf(stdout, format, args) >= 0;
			va_end(args);
			break;
		#endif

		case LOG_LEVEL_WARN:
		case LOG_LEVEL_ERROR:
			va_start(args, format);
			status = vfprintf(stderr, format, args) >= 0;
			va_end(args);
			break;
	}

	return status;
}

// NOTE: The following callback executes on the same thread that hook_run() is called
// from.  This is important because hook_run() attaches to the operating systems
// event dispatcher and may delay event delivery to the target application.
// Furthermore, some operating systems may choose to disable your hook if it
// takes to long to process.  If you need to do any extended processing, please
// do so by copying the event to your own queued dispatch thread.
struct _MEvent {
	uint8_t id;
	size_t mask;
	uint16_t keychar;
	// char *keychar;
	size_t x;
	uint8_t y;
	uint8_t bytesPerPixel;
};

typedef struct _MEvent MEvent;
// typedef MMBitmap *MMBitmapRef;
char *cevent;
// uint16_t *cevent;
int cstatus=1;

MEvent mEvent;
void dispatch_proc(uiohook_event * const event) {
	char buffer[256] = { 0 };
	size_t length = snprintf(buffer, sizeof(buffer),
			"id=%i,when=%" PRIu64 ",mask=0x%X",
			event->type, event->time, event->mask);

	switch (event->type) {
		case EVENT_KEY_PRESSED:
			// If the escape key is pressed, naturally terminate the program.
			if (event->data.keyboard.keycode == VC_ESCAPE) {
				int status = hook_stop();
				switch (status) {
					// System level errors.
					case UIOHOOK_ERROR_OUT_OF_MEMORY:
						logger_proc(LOG_LEVEL_ERROR, "Failed to allocate memory. (%#X)", status);
						break;

					case UIOHOOK_ERROR_X_RECORD_GET_CONTEXT:
						// NOTE This is the only platform specific error that occurs on hook_stop().
						logger_proc(LOG_LEVEL_ERROR, "Failed to get XRecord context. (%#X)", status);
						break;

					// Default error.
					case UIOHOOK_FAILURE:
					default:
						logger_proc(LOG_LEVEL_ERROR, "An unknown hook error occurred. (%#X)", status);
						break;
				}
			}
		case EVENT_KEY_RELEASED:
			snprintf(buffer + length, sizeof(buffer) - length,
				",keycode=%u,rawcode=0x%X",
				event->data.keyboard.keycode, event->data.keyboard.rawcode);
			break;

		case EVENT_KEY_TYPED:
			snprintf(buffer + length, sizeof(buffer) - length,
				",keychar=%lc,rawcode=%u",
				(uint16_t) event->data.keyboard.keychar,//wint_t
				event->data.keyboard.rawcode);
				
				#ifdef  WE_REALLY_WANT_A_POINTER
					char *buf = malloc (6);
				#else
					char buf[6];
				#endif

					sprintf(buf, "%lc",(uint16_t) event->data.keyboard.keychar);

				#ifdef WE_REALLY_WANT_A_POINTER
					free (buf);
				#endif

				if (strcmp(buf, cevent) == 0){
					int astop=aStop();
					printf("%d\n",astop);
					cstatus=0;
				}
				// return (char*) event->data.keyboard.keychar;
			break;

		case EVENT_MOUSE_PRESSED:
		case EVENT_MOUSE_RELEASED:
		case EVENT_MOUSE_CLICKED:
		case EVENT_MOUSE_MOVED:
		case EVENT_MOUSE_DRAGGED:
			snprintf(buffer + length, sizeof(buffer) - length,
				",x=%i,y=%i,button=%i,clicks=%i",
				event->data.mouse.x, event->data.mouse.y,
				event->data.mouse.button, event->data.mouse.clicks);
				int abutton=event->data.mouse.button;
				int aclicks=event->data.mouse.clicks;
				int amouse;
				if (strcmp(cevent,"mleft") == 0){
					amouse=1;
				}
				if (strcmp(cevent,"mright") == 0){
					amouse=2;
				}
				if (strcmp(cevent,"wheelDown") == 0){
					amouse=4;
				}
				if (strcmp(cevent,"wheelUp") == 0){
					amouse=5;
				}
				if (strcmp(cevent,"wheelLeft") == 0){
					amouse=6;
				}
				if (strcmp(cevent,"wheelRight") == 0){
					amouse=7;
				}
				if (abutton==amouse && aclicks==1){
					int astop=aStop();
					cstatus=0;
				}

			break;

		case EVENT_MOUSE_WHEEL:
			snprintf(buffer + length, sizeof(buffer) - length,
				",type=%i,amount=%i,rotation=%i",
				event->data.wheel.type, event->data.wheel.amount,
				event->data.wheel.rotation);
			break;

		default:
			break;
	}

	// fprintf(stdout, "----%s\n",	 buffer);
}

int aEvent(char *aevent) {
	// (uint16_t *)
	cevent=aevent;
	// Set the logger callback for library output.
	hook_set_logger_proc(&logger_proc);

	// Set the event callback for uiohook events.
	hook_set_dispatch_proc(&dispatch_proc);
	// Start the hook and block.
	// NOTE If EVENT_HOOK_ENABLED was delivered, the status will always succeed.
	int status = hook_run();

	switch (status) {
		case UIOHOOK_SUCCESS:
			// Everything is ok.
			break;

		// System level errors.
		case UIOHOOK_ERROR_OUT_OF_MEMORY:
			logger_proc(LOG_LEVEL_ERROR, "Failed to allocate memory. (%#X)", status);
			break;


		// X11 specific errors.
		case UIOHOOK_ERROR_X_OPEN_DISPLAY:
			logger_proc(LOG_LEVEL_ERROR, "Failed to open X11 display. (%#X)", status);
			break;

		case UIOHOOK_ERROR_X_RECORD_NOT_FOUND:
			logger_proc(LOG_LEVEL_ERROR, "Unable to locate XRecord extension. (%#X)", status);
			break;

		case UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE:
			logger_proc(LOG_LEVEL_ERROR, "Unable to allocate XRecord range. (%#X)", status);
			break;

		case UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT:
			logger_proc(LOG_LEVEL_ERROR, "Unable to allocate XRecord context. (%#X)", status);
			break;

		case UIOHOOK_ERROR_X_RECORD_ENABLE_CONTEXT:
			logger_proc(LOG_LEVEL_ERROR, "Failed to enable XRecord context. (%#X)", status);
			break;


		// Windows specific errors.
		case UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX:
			logger_proc(LOG_LEVEL_ERROR, "Failed to register low level windows hook. (%#X)", status);
			break;


		// Darwin specific errors.
		case UIOHOOK_ERROR_AXAPI_DISABLED:
			logger_proc(LOG_LEVEL_ERROR, "Failed to enable access for assistive devices. (%#X)", status);
			break;

		case UIOHOOK_ERROR_CREATE_EVENT_PORT:
			logger_proc(LOG_LEVEL_ERROR, "Failed to create apple event port. (%#X)", status);
			break;

		case UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE:
			logger_proc(LOG_LEVEL_ERROR, "Failed to create apple run loop source. (%#X)", status);
			break;

		case UIOHOOK_ERROR_GET_RUNLOOP:
			logger_proc(LOG_LEVEL_ERROR, "Failed to acquire apple run loop. (%#X)", status);
			break;

		case UIOHOOK_ERROR_CREATE_OBSERVER:
			logger_proc(LOG_LEVEL_ERROR, "Failed to create apple run loop observer. (%#X)", status);
			break;

		// Default error.
		case UIOHOOK_FAILURE:
		default:
			logger_proc(LOG_LEVEL_ERROR, "An unknown hook error occurred. (%#X)", status);
			break;
	}

	// return status;
	printf("%d\n",status);
	return cstatus;
}

int aStop(){
	int status = hook_stop();
	switch (status) {
		// System level errors.
		case UIOHOOK_ERROR_OUT_OF_MEMORY:
			logger_proc(LOG_LEVEL_ERROR, "Failed to allocate memory. (%#X)", status);
			break;

		case UIOHOOK_ERROR_X_RECORD_GET_CONTEXT:
			// NOTE This is the only platform specific error that occurs on hook_stop().
			logger_proc(LOG_LEVEL_ERROR, "Failed to get XRecord context. (%#X)", status);
			break;

				// Default error.
		case UIOHOOK_FAILURE:
			default:
			// logger_proc(LOG_LEVEL_ERROR, "An unknown hook error occurred. (%#X)", status);
			break;
	}

	return status;
}