#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include "logtee.h"

const char *cback() {
	static char buf[128];
	snprintf(buf, sizeof buf, "[%zu]: ", time(NULL));
	return buf;
}

int main() {
	LOG_teefile(stderr, 0);
	LOG_prefixcallback(cback);

	LOGI("%s, %s!\n", "Hello", "World");
	LOGE("Nooo!\n");
	LOGW("Hmm...\n");

	LOG_teefile(stderr, 1);
	LOG_teefile(stderr, 2);
	LOGI("Info 2\n"); // 1 copy of this
	LOGW("Warn 2\n"); // 2 copies of this
	LOGE("Err 2\n");  // 3 copies of t his

	LOG_reset();
	LOGI("What?\n"); // 0 copies of this

	LOG_teefile(stderr, 0);
	LOG_teepath("log.txt", 0);

	unlink("/");
	PLOGE("unlink"); // perror()-like

	LOGI("Info 3\n");
	LOGF("Fatal\n");
	LOGI("Not reached\n"); // not reached
}
