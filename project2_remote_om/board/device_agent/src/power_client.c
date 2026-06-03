#include "power_client.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define POWER_MANAGER_BIN "/opt/project2/bin/power_manager"
#define POWER_MANAGER_CONF "/etc/power_manager/power_manager.conf"

static int mode_allowed(const char *mode)
{
	return !strcmp(mode, "normal") ||
	       !strcmp(mode, "idle") ||
	       !strcmp(mode, "low_power") ||
	       !strcmp(mode, "sleep") ||
	       !strcmp(mode, "maintenance");
}

static int run_power_manager(const char *arg, const char *value,
			     char *out, size_t out_len)
{
	int pipefd[2];
	pid_t pid;
	int status;
	ssize_t n;

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
		if (value) {
			execl(POWER_MANAGER_BIN, POWER_MANAGER_BIN, "-c",
			      POWER_MANAGER_CONF, arg, value, (char *)NULL);
		} else {
			execl(POWER_MANAGER_BIN, POWER_MANAGER_BIN, "-c",
			      POWER_MANAGER_CONF, arg, (char *)NULL);
		}
		_exit(127);
	}

	close(pipefd[1]);
	n = read(pipefd[0], out, out_len > 0 ? out_len - 1 : 0);
	close(pipefd[0]);
	if (waitpid(pid, &status, 0) < 0)
		return -1;

	if (out_len > 0) {
		if (n > 0)
			out[n] = '\0';
		else
			out[0] = '\0';
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;

	return 0;
}

static void trim_line(char *text)
{
	size_t len = strlen(text);

	while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' ||
			   text[len - 1] == ' ' || text[len - 1] == '\t')) {
		text[--len] = '\0';
	}
}

int power_client_get_mode(char *msg, size_t msg_len)
{
	char out[128];

	if (run_power_manager("--get-mode", NULL, out, sizeof(out))) {
		snprintf(msg, msg_len, "power get failed");
		return -1;
	}

	trim_line(out);
	snprintf(msg, msg_len, "mode=%s", out[0] ? out : "unknown");
	return 0;
}

int power_client_set_mode(const char *mode, char *msg, size_t msg_len)
{
	char out[128];

	if (!mode_allowed(mode)) {
		snprintf(msg, msg_len, "bad mode");
		return -1;
	}

	if (run_power_manager("--set-mode", mode, out, sizeof(out))) {
		snprintf(msg, msg_len, "power set failed");
		return -1;
	}

	trim_line(out);
	snprintf(msg, msg_len, "mode=%s", out[0] ? out : mode);
	return 0;
}

