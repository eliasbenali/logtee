/**
 *  C99 Header-only library for extensible logging facilities
 *  Copyright (c) 2019 Elias Benali <stackptr@users.sourceforge.net>
 *  Distributed under the terms of the MIT License
 */

/**
 * Library specifies some process-wide unique state data structures. Exactly
 * one translation unit must define LOGTEE_UNIQUE_STATE prior to including
 * the header.
 *
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
 * Predefined loging priorities with appropriate prefixes and behavior are
 * implemented in terms of levels.
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
#include <fcntl.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef		LOGTEE_UNIQUE_STATE
# define	USTATE(T,id,...) T id = __VA_ARGS__;
#else
# define	USTATE(T,id,...) extern T id;
#endif

	struct _l_fplist {
		FILE                    *fp;
		int                     level;
		struct _l_fplist        *next;
	}; USTATE(struct _l_fplist, _fplist, {
			.fp = NULL,
			.level = 0,
			.next = NULL,
			});

	struct _l_loglevel {
		int level;
		const char *prefix;
	}; USTATE(struct _l_loglevel, *_loglevels, NULL);

	USTATE(const struct _l_loglevel, _builtin_levels[5], {
			// default formats reminiscent of Xorg logs...
			{ -1, "(DD): " }, /* Debug   */
			{ 0, "(II): " },  /* Info    */
			{ 1, "(WW): " },  /* Warning */
			{ 2, "(EE): " },  /* Error   */
			{ 3, "(FF): " },  /* Fatal   */
			});

	USTATE(size_t, _numlevels, 0);

	// Called at each invocation of LOG() and its output prepended to the line
	typedef const char *(*_prefix_callback_t)();
	USTATE(_prefix_callback_t, _prefix_callback, NULL);

#       if !defined(LINE_MAX)
#         define LINE_MAX               2048
#       endif

#	define LOGD(fmt,...) LOG(-1, fmt, ##__VA_ARGS__)
#       define LOGI(fmt,...) LOG(0, fmt, ##__VA_ARGS__)
#       define LOGW(fmt,...) LOG(1, fmt, ##__VA_ARGS__)
#       define LOGE(fmt,...) LOG(2, fmt, ##__VA_ARGS__)
#       define LOGF(fmt,...) (void)(LOG(3, fmt, ##__VA_ARGS__), exit(EXIT_FAILURE))

#       define PLOGI(fmt,...) LOGI(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#       define PLOGW(fmt,...) LOGW(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#       define PLOGE(fmt,...) LOGE(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#       define PLOGF(fmt,...) LOGF(fmt ": %s\n", ##__VA_ARGS__, strerror(errno))

	static void _LOG_cleanup() {
		for (struct _l_fplist *fpl = &_fplist; fpl != NULL; fpl = fpl->next)
			if (fpl->fp != NULL && fileno(fpl->fp) != STDIN_FILENO
					&& fileno(fpl->fp) != STDERR_FILENO)
				fclose(fpl->fp);
	}

	inline static void __attribute__(( format(printf, 2, 3) ))
		LOG(int level, const char *fmt, ...) {
			if (_loglevels == NULL) { // initialize state if necessary
				atexit(_LOG_cleanup);
				if ((_loglevels = malloc(sizeof(_builtin_levels))) == NULL)
					goto malloc_fail;
				memcpy(_loglevels, _builtin_levels, sizeof(_builtin_levels));
				_numlevels = sizeof(_builtin_levels)/sizeof(*_loglevels);;
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
				fflush(lfp->fp);
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

		if (_loglevels != NULL) {
			free(_loglevels);
			if ((_loglevels = malloc(sizeof(_builtin_levels))) == NULL)
				fprintf(stderr, "%s: malloc: %s\n", __func__, strerror(errno));
			else
				memcpy(_loglevels, _builtin_levels, sizeof(_builtin_levels));
			_numlevels = sizeof(_builtin_levels) / sizeof(*_loglevels);
		}
	}

	inline static void LOG_teefile(FILE *file, int level) {
		if (file == NULL) return;
		if (fileno(file) != STDOUT_FILENO && fileno(file) != STDERR_FILENO) {
			if (fseek(file, 0, SEEK_END) == -1)
				PLOGW("%s: fseek(SEEK_END)", __func__);
			if (fcntl(fileno(file), F_SETFD, FD_CLOEXEC) == -1)
				PLOGW("%s: fcntl(FD_CLOEXEC)", __func__);
		}
		for (struct _l_fplist *fp = &_fplist; fp; fp = fp->next) {
			if (fp->fp == NULL) { // empty slot
				fp->fp = file;
				fp->level = level;
				break;
			} else if (fp->next == NULL) { // expand by new entry
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

	inline static void LOG_prefixcallback(_prefix_callback_t cback) {
		_prefix_callback = cback ? cback : _prefix_callback;
	}

	/**
	 *  Dump interal state
	 */
	__attribute__((__used__)) static void LOG_fornerds() {
		fprintf(stderr, "LOG: pid=%u, ppid=%u\n", getpid(), getppid());
		fprintf(stderr, "LOG: number of levels: %zu, &levels=%p, &*levelp=%p\n", _numlevels, _loglevels, &_loglevels);
		fprintf(stderr, "LOG: &fplist=%p, log targets: ", &_fplist);
		for (struct _l_fplist *fpl = &_fplist; fpl != NULL; fpl = fpl->next) {
			if (fpl->fp != NULL) {
				fprintf(stderr, "<FILE*=%p(fd%u),level=%i> ", fpl->fp, fileno(fpl->fp), fpl->level);
			}
		}
		fputc('\n', stderr);
	}

#if defined(__cplusplus)
} // extern "C"
#endif

// vim: ts=4 sw=4 ai
