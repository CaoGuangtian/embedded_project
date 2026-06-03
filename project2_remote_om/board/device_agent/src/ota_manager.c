#include "ota_manager.h"

#include "service_manager.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct install_target {
	const char *name;
	const char *install_path;
};

static const struct install_target g_install_targets[] = {
	{ "power_manager", "/opt/project2/bin/power_manager" },
	{ "collector_demo", "/opt/project2/bin/collector_demo" },
	{ "app_service", "/opt/project2/bin/app_service" },
};

static int target_allowed(const char *target)
{
	return !strcmp(target, "device_agent") ||
	       !strcmp(target, "power_manager") ||
	       !strcmp(target, "collector_demo") ||
	       !strcmp(target, "app_service");
}

static const struct install_target *find_install_target(const char *target)
{
	size_t i;

	for (i = 0; i < sizeof(g_install_targets) / sizeof(g_install_targets[0]);
	     i++) {
		if (!strcmp(target, g_install_targets[i].name))
			return &g_install_targets[i];
	}

	return NULL;
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

static int run_rm_rf(const char *path)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/bin/rm", "rm", "-rf", path, (char *)NULL);
		execl("/usr/bin/rm", "rm", "-rf", path, (char *)NULL);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (!WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}

static int run_tar_extract(const char *package_path, const char *extract_dir)
{
	pid_t pid;
	int status;

	if (mkdir(extract_dir, 0755) != 0)
		return -1;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/bin/tar", "tar", "-xzf", package_path, "-C",
		      extract_dir, (char *)NULL);
		execl("/usr/bin/tar", "tar", "-xzf", package_path, "-C",
		      extract_dir, (char *)NULL);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (!WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}

static int path_has_parent_ref(const char *path)
{
	const char *p = path;

	while (*p) {
		const char *start = p;
		size_t len;

		while (*p && *p != '/')
			p++;

		len = (size_t)(p - start);
		if (len == 2 && start[0] == '.' && start[1] == '.')
			return 1;

		while (*p == '/')
			p++;
	}

	return 0;
}

static int safe_tar_entry_path(const char *path)
{
	if (!path || !path[0])
		return 0;
	if (path[0] == '/')
		return 0;
	if (path_has_parent_ref(path))
		return 0;

	return 1;
}

static int validate_tar_entries(const char *package_path)
{
	int pipefd[2];
	pid_t pid;
	int status;
	FILE *fp;
	char line[512];
	int saw_entry = 0;
	int unsafe = 0;

	if (pipe(pipefd) < 0)
		return -1;

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		execl("/bin/tar", "tar", "-tzf", package_path, (char *)NULL);
		execl("/usr/bin/tar", "tar", "-tzf", package_path,
		      (char *)NULL);
		_exit(127);
	}

	close(pipefd[1]);
	fp = fdopen(pipefd[0], "r");
	if (!fp) {
		close(pipefd[0]);
		waitpid(pid, &status, 0);
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		size_t len = strlen(line);
		int complete = 0;

		while (len > 0 && (line[len - 1] == '\n' ||
				   line[len - 1] == '\r')) {
			line[--len] = '\0';
			complete = 1;
		}

		if (!complete && len + 1 == sizeof(line)) {
			int ch;

			unsafe = 1;
			while ((ch = fgetc(fp)) != EOF && ch != '\n')
				;
		}

		saw_entry = 1;
		if (!safe_tar_entry_path(line))
			unsafe = 1;
	}

	fclose(fp);
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (!WIFEXITED(status))
		return -1;
	if (WEXITSTATUS(status) != 0)
		return WEXITSTATUS(status);
	if (unsafe || !saw_entry)
		return -2;

	return 0;
}

static int run_cp(const char *src, const char *dst)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/bin/cp", "cp", src, dst, (char *)NULL);
		execl("/usr/bin/cp", "cp", src, dst, (char *)NULL);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (!WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}

static int run_chmod_exec(const char *path)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/bin/chmod", "chmod", "+x", path, (char *)NULL);
		execl("/usr/bin/chmod", "chmod", "+x", path, (char *)NULL);
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
	int pipefd[2];
	pid_t pid;
	int status;
	ssize_t n;
	int ret = -1;

	if (out_len < 65)
		return -1;

	if (pipe(pipefd) < 0)
		return -1;

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		execl("/bin/sha256sum", "sha256sum", path, (char *)NULL);
		execl("/usr/bin/sha256sum", "sha256sum", path, (char *)NULL);
		_exit(127);
	}

	close(pipefd[1]);
	n = read(pipefd[0], out, out_len - 1);
	close(pipefd[0]);
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;

	if (n >= 64) {
		out[n] = '\0';
		out[64] = '\0';
		ret = 0;
	}

	return ret;
}

static int check_target_binary(const char *extract_dir, const char *target)
{
	char path[300];
	struct stat st;

	snprintf(path, sizeof(path), "%s/bin/%s", extract_dir, target);
	if (stat(path, &st) < 0)
		return -1;
	if (!S_ISREG(st.st_mode))
		return -1;

	return 0;
}

static int check_version_file(const char *extract_dir, const char *version)
{
	char path[300];
	char line[128];
	FILE *fp;
	size_t len;

	snprintf(path, sizeof(path), "%s/version", extract_dir);
	fp = fopen(path, "r");
	if (!fp)
		return -1;

	if (!fgets(line, sizeof(line), fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
			   line[len - 1] == ' ' || line[len - 1] == '\t')) {
		line[--len] = '\0';
	}

	return strcmp(line, version) == 0 ? 0 : 1;
}

int ota_manager_prepare_package(const char *target, const char *version,
				const char *url, const char *sha256,
				char *msg, size_t msg_len)
{
	char path[256];
	char extract_dir[256];
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
	snprintf(extract_dir, sizeof(extract_dir), "/tmp/project2_ota_%s",
		 target);

	unlink(path);
	run_rm_rf(extract_dir);

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

	ret = validate_tar_entries(path);
	if (ret == 127) {
		snprintf(msg, msg_len, "tar not found");
		unlink(path);
		return -1;
	}
	if (ret == -2) {
		snprintf(msg, msg_len, "unsafe tar path");
		unlink(path);
		return -1;
	}
	if (ret != 0) {
		snprintf(msg, msg_len, "tar list failed code=%d", ret);
		unlink(path);
		return -1;
	}

	ret = run_tar_extract(path, extract_dir);
	if (ret == 127) {
		snprintf(msg, msg_len, "tar not found");
		run_rm_rf(extract_dir);
		return -1;
	}
	if (ret != 0) {
		snprintf(msg, msg_len, "tar failed code=%d", ret);
		run_rm_rf(extract_dir);
		return -1;
	}

	if (check_target_binary(extract_dir, target)) {
		snprintf(msg, msg_len, "missing target binary");
		run_rm_rf(extract_dir);
		return -1;
	}

	ret = check_version_file(extract_dir, version);
	if (ret < 0) {
		snprintf(msg, msg_len, "missing version");
		run_rm_rf(extract_dir);
		return -1;
	}
	if (ret > 0) {
		snprintf(msg, msg_len, "version mismatch");
		run_rm_rf(extract_dir);
		return -1;
	}

	snprintf(msg, msg_len, "ota package prepared");
	return 0;
}

static int regular_file_exists(const char *path)
{
	struct stat st;

	if (stat(path, &st) < 0)
		return 0;

	return S_ISREG(st.st_mode);
}

static int service_running_msg(const char *msg)
{
	return strstr(msg, " running") != NULL;
}

static int restart_and_check(const char *target, char *msg, size_t msg_len)
{
	char status_msg[128];

	if (service_manager_handle(target, "restart", msg, msg_len) != 0)
		return -1;

	sleep(1);
	if (service_manager_handle(target, "status", status_msg,
				   sizeof(status_msg)) != 0)
		return -1;

	if (!service_running_msg(status_msg)) {
		snprintf(msg, msg_len, "health check failed");
		return -1;
	}

	return 0;
}

static int rollback_binary(const char *target, const char *install_path,
			   const char *backup_path, char *msg, size_t msg_len)
{
	if (run_cp(backup_path, install_path) != 0) {
		snprintf(msg, msg_len, "rollback failed");
		return -1;
	}

	run_chmod_exec(install_path);
	if (service_manager_handle(target, "restart", msg, msg_len) != 0) {
		snprintf(msg, msg_len, "rollback restart failed");
		return -1;
	}

	snprintf(msg, msg_len, "rollback ok");
	return 0;
}

int ota_manager_install_prepared(const char *target, char *msg,
				 size_t msg_len)
{
	const struct install_target *entry;
	char prepared_bin[300];
	char backup_path[300];
	char install_dir[256];
	char *slash;

	if (!target_allowed(target)) {
		snprintf(msg, msg_len, "target not allowed");
		return -1;
	}

	if (!strcmp(target, "device_agent")) {
		snprintf(msg, msg_len, "self upgrade not supported yet");
		return -1;
	}

	entry = find_install_target(target);
	if (!entry) {
		snprintf(msg, msg_len, "target not installable");
		return -1;
	}

	snprintf(prepared_bin, sizeof(prepared_bin),
		 "/tmp/project2_ota_%s/bin/%s", target, target);
	if (!regular_file_exists(prepared_bin)) {
		snprintf(msg, msg_len, "missing target binary");
		return -1;
	}

	snprintf(install_dir, sizeof(install_dir), "%s", entry->install_path);
	slash = strrchr(install_dir, '/');
	if (!slash) {
		snprintf(msg, msg_len, "bad install path");
		return -1;
	}
	*slash = '\0';
	if (access(install_dir, X_OK) != 0) {
		snprintf(msg, msg_len, "install dir not found");
		return -1;
	}

	if (!regular_file_exists(entry->install_path)) {
		snprintf(msg, msg_len, "old binary not found");
		return -1;
	}

	snprintf(backup_path, sizeof(backup_path), "%s.bak",
		 entry->install_path);
	if (run_cp(entry->install_path, backup_path) != 0) {
		snprintf(msg, msg_len, "backup failed");
		return -1;
	}

	if (run_cp(prepared_bin, entry->install_path) != 0 ||
	    run_chmod_exec(entry->install_path) != 0) {
		rollback_binary(target, entry->install_path, backup_path,
				msg, msg_len);
		return -1;
	}

	if (restart_and_check(target, msg, msg_len) != 0) {
		char rollback_msg[128];

		if (rollback_binary(target, entry->install_path, backup_path,
				    rollback_msg, sizeof(rollback_msg)) == 0) {
			snprintf(msg, msg_len, "health check failed, %s",
				 rollback_msg);
		}
		return -1;
	}

	snprintf(msg, msg_len, "install ok");
	return 0;
}
