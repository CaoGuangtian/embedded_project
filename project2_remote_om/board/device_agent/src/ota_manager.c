#include "ota_manager.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

static int target_allowed(const char *target)
{
	return !strcmp(target, "device_agent") ||
	       !strcmp(target, "power_manager") ||
	       !strcmp(target, "collector_demo") ||
	       !strcmp(target, "app_service");
}

static int safe_token(const char *text)
{
	size_t i;

	if (!text || !text[0])
		return 0;

	for (i = 0; text[i]; i++) {
		unsigned char ch = (unsigned char)text[i];

		if (!(isalnum(ch) || ch == '_' || ch == '-' || ch == '.'))
			return 0;
	}

	return 1;
}

static int valid_url(const char *url)
{
	if (!url || !url[0])
		return 0;

	return !strncmp(url, "http://", 7) ||
	       !strncmp(url, "https://", 8);
}

static int valid_sha256(const char *sha256)
{
	size_t i;

	if (!sha256 || strlen(sha256) != 64)
		return 0;

	for (i = 0; i < 64; i++) {
		if (!isxdigit((unsigned char)sha256[i]))
			return 0;
	}

	return 1;
}

static int run_wget(const char *url, const char *out_path)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/bin/wget", "wget", "-q", "-O", out_path, url,
		      (char *)NULL);
		execl("/usr/bin/wget", "wget", "-q", "-O", out_path, url,
		      (char *)NULL);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (!WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}

static int read_sha256sum(const char *path, char *out, size_t out_len)
{
	char cmd[300];
	FILE *fp;
	int ret = -1;

	if (out_len < 65)
		return -1;

	snprintf(cmd, sizeof(cmd), "sha256sum '%s'", path);
	fp = popen(cmd, "r");
	if (!fp)
		return -1;

	if (fgets(out, (int)out_len, fp) && strlen(out) >= 64) {
		out[64] = '\0';
		ret = 0;
	}

	pclose(fp);
	return ret;
}

int ota_manager_upgrade_check_only(const char *target, const char *version,
				   const char *url, const char *sha256,
				   char *msg, size_t msg_len)
{
	char path[256];
	char actual[80];
	int ret;

	if (!target_allowed(target)) {
		snprintf(msg, msg_len, "target not allowed");
		return -1;
	}

	if (!safe_token(target) || !safe_token(version)) {
		snprintf(msg, msg_len, "bad target or version");
		return -1;
	}

	if (!valid_url(url)) {
		snprintf(msg, msg_len, "bad url");
		return -1;
	}

	if (!valid_sha256(sha256)) {
		snprintf(msg, msg_len, "bad sha256");
		return -1;
	}

	snprintf(path, sizeof(path), "/tmp/project2_ota_%s.tar.gz", target);
	unlink(path);

	ret = run_wget(url, path);
	if (ret == 127) {
		snprintf(msg, msg_len, "wget not found");
		return -1;
	}
	if (ret != 0) {
		snprintf(msg, msg_len, "download failed code=%d", ret);
		unlink(path);
		return -1;
	}

	if (read_sha256sum(path, actual, sizeof(actual))) {
		snprintf(msg, msg_len, "sha256sum failed");
		unlink(path);
		return -1;
	}

	if (strcasecmp(actual, sha256)) {
		snprintf(msg, msg_len, "sha256 mismatch");
		unlink(path);
		return -1;
	}

	snprintf(msg, msg_len, "download and sha256 ok");
	return 0;
}
