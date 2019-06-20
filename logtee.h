/**
 *  C99 Header-only library for extensible logging facilities
 *  Copyright (c) 2019 Elias Benali <stackptr@users.sourceforge.net>
 *  Distributed under the terms of the MIT License
 */

/**
 * Any number of log targets can be specified in a `Tee' with LOG_teepath()
 * and LOG_teefile(FILE*). Output within the Tee is unordered.
 *
 * LOG_reset() removes all logging targets (in which case logging is a no-op)
 *
 * Comes with predefined log levels and each log target has a setting that
 * controls the minimum priority levels for output to make it into the log.
 * Levels (-Infinity,+Infinity) proceded with increasing numbers denoting
 * output with higher priority.
 *
 * Predefined Info, Warning, Error and Fatal loging priorities with appropriate
 * prefixes and behavior are implemented in terms of levels.
 *
 * Levels can be extended.
 *
 * Callback can be set to provide a prefix to each line (such as timestamps)
 *
 * LOG() is the workhorse of the library but is normally abstracted from in
 * user code with wrappers with predefined semantics. LOGW(char*,...) marks
 * output as a Warning while for fatal conditions LOGF(...) will forward
 * its arguments to LOGE to be logged as errors then terminate the process.
 *
 * POSIX-like analogs of the LOGX() macros are predefined and will append
 * a textual description of the current errno value, much line perror()
 * For example, the following two statements produce the same output:
 * PLOGE("error: read(2)") ; LOGE("error: read(2): %s", strerror(errno))
 */

#pragma once
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__cplusplus)
extern "C" {
#endif

	static struct _l_fplist {
		FILE                    *fp;
		int                     level;
		struct _l_fplist        *next;
	} _fplist = {
		.fp = NULL,
		.level = 0,
		.next = NULL,
	};

	static struct _l_loglevel {
		int level;
		const char *prefix;
	} *_loglevels;

	static const struct _l_loglevel _builtin_levels[4] = {  // default formats reminiscent of Xorg logs...
		{ 0, "(II): " },  /* Info    */
		{ 1, "(WW): " },  /* Warning */
		{ 2, "(EE): " },  /* Error   */
		{ 3, "(FF): " },  /* Fatal   */
	};

	static size_t _numlevels = 0;

	// Called at each invocation of LOG() and its output prepended to the line
	static const char * (*_prefix_callback)() = NULL;

#       if !defined(LINE_MAX)
#         define LINE_MAX               2048
#       endif

#       define LOGI(fmt,...) LOG(0, fmt, ##__VA_ARGS__)
#       define LOGW(fmt,...) LOG(1, fmt, ##__VA_ARGS__)
#       define LOGE(fmt,...) LOG(2, fmt, ##__VA_ARGS__)
#       define LOGF(fmt,...) (void)(LOG(3, fmt, ##__VA_ARGS__), exit(EXIT_FAILURE))

#       define PLOGI(fmt,...) LOGI(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#       define PLOGW(fmt,...) LOGW(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#       define PLOGE(fmt,...) LOGE(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#       define PLOGF(fmt,...) LOGF(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))

	inline static void LOG(int level, const char *fmt, ...) {
		if (_loglevels == NULL) { // initialize state if necessary
			if ((_loglevels = malloc(sizeof(_builtin_levels))) == NULL)
				goto malloc_fail;
			memcpy(_loglevels, _builtin_levels, sizeof(_builtin_levels));
			_numlevels = 4;
		}

		static char *logline = NULL; // allocated once, freed at exit
		if (logline == NULL && (logline = malloc(sizeof(char) * LINE_MAX)) == NULL)
			goto malloc_fail;
		*logline = '\0';

		// Bail out early when no log targets are defined
		if (_fplist.fp == NULL)
			return;

		va_list ap;
		va_start(ap, fmt);
		vsnprintf(logline, LINE_MAX, fmt, ap);
		va_end(ap);

		// Determine level, if no such level then no extra annnotation included
		struct _l_loglevel *llev = NULL;
		for (size_t i=0; _loglevels != NULL && i < _numlevels; ++i)
			if (_loglevels[i].level == level)
				llev = _loglevels+i;

		for (struct _l_fplist *lfp = &_fplist; lfp != NULL; lfp = lfp->next) {
			if (lfp->fp == NULL || lfp->level > level)
				continue;
			fprintf(lfp->fp, "%s%s%s", _prefix_callback ? _prefix_callback() : "",
					llev ? llev->prefix : "", logline);
		}

		return;
malloc_fail:
		fprintf(stderr, "%s: malloc: %s\n", __func__, strerror(errno));
		return;
	}

/**
 *  Clean slate
 */
inline static void LOG_reset() {
	for (struct _l_fplist *fp = &_fplist; fp; fp = fp->next) {
		if (fileno(fp->fp) != STDOUT_FILENO && fileno(fp->fp) != STDERR_FILENO)
			fclose(fp->fp);
		fp->fp = NULL;
		fp->level = 0;
	}

	_prefix_callback = NULL;

	if ((_loglevels = malloc(sizeof(_builtin_levels))) == NULL)
		fprintf(stderr, "%s: malloc: %s\n", __func__, strerror(errno));
	else
		memcpy(_loglevels, _builtin_levels, sizeof(_builtin_levels));
	_numlevels = 4;
}

inline static void LOG_teefile(FILE *file, int level) {
	if (file == NULL) return;
	fseek(file, 0, SEEK_END);
	for (struct _l_fplist *fp = &_fplist; fp; fp = fp->next) {
		if (fp->fp == NULL) { // empty slot
			fp->fp = file;
			fp->level = level;
			break;
		} else if (fp->next == NULL) { // new entry
			if ((fp->next = malloc(sizeof(*fp))) == NULL) {
				LOGE("%s: malloc: %s.\n", __func__, strerror(errno));
				return;
			}
			fp->next->next = NULL;
			fp->next->fp = file;
			fp->next->level = level;
			break;
		}
	}
}

inline static void LOG_teepath(const char *path, int level) {
	FILE *fp;

	if (path == NULL)
		fp = stderr;
	else if (strcmp(path, "-") == 0)
		fp = stdout;
	else if ((fp = fopen(path, "a")) == NULL)
		LOGW("%s: can't open '%s' for logging: %s.\n",
				__func__, path, strerror(errno));
	LOG_teefile(fp, level);
}

inline static void LOG_addlevel(int level, const char *prefix) {
	if (prefix == NULL || *prefix == '\0') {
		LOGW("%s: invalid prefix.\n", __func__);
		return;
	}
	_loglevels = realloc( _loglevels, sizeof(*_loglevels) * ++_numlevels );
	if (_loglevels == NULL) {
		PLOGE("%s: realloc", __func__);
		_numlevels = 0;
	} else {
		_loglevels[_numlevels-1].level = level;
		_loglevels[_numlevels-1].prefix = strdup(prefix);
	}
}

inline static void LOG_prefixcallback(const char *(*cback)()) {
	_prefix_callback = cback ? cback : _prefix_callback;
}

#if defined(__cplusplus)
} // extern "C"
#endif
