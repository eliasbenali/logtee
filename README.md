# C99 header-only library for extensible logging facilities.
## Featuring
* Tees : multiple logging targets, anything that can be turned into a ```FILE*```
* Loglevels: extensible log levels, with predefined Info, Warning, Error and Fatal (terminating) levels.
* Priority levels: log targets are configured with a minmum level threshold
* [```perror()```](https://pubs.opengroup.org/onlinepubs/9699919799/functions/perror.html)-like equivalents: PLOG{I,W,E,F}
* Callback function for prefix

## Internals
```
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
```
