#include "service_manager.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum service_kind {
	SERVICE_KIND_DAEMON = 0,
	SERVICE_KIND_COMMAND_HELPER,
};

struct service_entry {
	const char *name;
	const char *script;
	const char *process;
	enum service_kind kind;
};

static const struct service_entry g_services[] = {
	{ "collector_demo", "/etc/init.d/S99collector_demo", "collector_demo",
	  SERVICE_KIND_DAEMON },
	{ "power_manager", "/etc/init.d/S98power_manager", "power_manager",
	  SERVICE_KIND_COMMAND_HELPER },
	{ "network_monitor", "/etc/init.d/S97network_monitor", "network_monitor",
	  SERVICE_KIND_DAEMON },
	{ "app_service", "/etc/init.d/S96app_service", "app_service",
	  SERVICE_KIND_DAEMON },
};

static const struct service_entry *find_service(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(g_services) / sizeof(g_services[0]); i++) {
		if (!strcmp(name, g_services[i].name))
			return &g_services[i];
	}

	return NULL;
}

static int action_allowed(const char *action)
{
	return !strcmp(action, "status") ||
	       !strcmp(action, "start") ||
	       !strcmp(action, "stop") ||
	       !strcmp(action, "restart");
}

static int process_running(const char *process)
{
	pid_t pid;
	int ret;

	pid = fork();
	if (pid < 0)
		return 0;
	if (pid == 0) {
		execl("/bin/pidof", "pidof", process, (char *)NULL);
		execl("/sbin/pidof", "pidof", process, (char *)NULL);
		execl("/usr/bin/pidof", "pidof", process, (char *)NULL);
		_exit(127);
	}

	if (waitpid(pid, &ret, 0) < 0)
		return 0;
	return WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
}

static void trim_line(char *text)
{
	size_t len = strlen(text);

	while (len > 0 && (text[len - 1] == '\n' ||
			   text[len - 1] == '\r' ||
			   text[len - 1] == ' ' ||
			   text[len - 1] == '\t')) {
		text[--len] = '\0';
	}
}

static int run_service_script(const struct service_entry *entry,
			      const char *action)
{
	pid_t pid;
	int status;

	if (access(entry->script, X_OK) != 0)
		return 127;

	pid = fork();
	if (pid < 0)
		return 126;
	if (pid == 0) {
		execl(entry->script, entry->script, action, (char *)NULL);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) < 0)
		return 126;
	if (!WIFEXITED(status))
		return 125;

	return WEXITSTATUS(status);
}

static int run_service_script_capture(const struct service_entry *entry,
				      const char *action, char *out,
				      size_t out_len)
{
	int pipefd[2];
	pid_t pid;
	int status;
	ssize_t n;

	if (out_len == 0)
		return 126;
	out[0] = '\0';

	if (access(entry->script, X_OK) != 0)
		return 127;

	if (pipe(pipefd) < 0)
		return 126;

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return 126;
	}

	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);
		execl(entry->script, entry->script, action, (char *)NULL);
		_exit(127);
	}

	close(pipefd[1]);
	n = read(pipefd[0], out, out_len - 1);
	close(pipefd[0]);
	if (n > 0)
		out[n] = '\0';
	else
		out[0] = '\0';

	if (waitpid(pid, &status, 0) < 0)
		return 126;
	if (!WIFEXITED(status))
		return 125;

	trim_line(out);
	return WEXITSTATUS(status);
}

int service_manager_handle(const char *service, const char *action,
			   char *msg, size_t msg_len)
{
	const struct service_entry *entry;
	char status_msg[128];
	int ret;

	entry = find_service(service);
	if (!entry) {
		snprintf(msg, msg_len, "service not allowed");
		return -1;
	}

	if (!action_allowed(action)) {
		snprintf(msg, msg_len, "action not allowed");
		return -1;
	}

	if (!strcmp(action, "status")) {
		if (entry->kind == SERVICE_KIND_COMMAND_HELPER) {
			ret = run_service_script_capture(entry, action,
							 status_msg,
							 sizeof(status_msg));
			if (ret == 127) {
				snprintf(msg, msg_len,
					 "service script not found");
				return -1;
			}
			if (ret != 0) {
				snprintf(msg, msg_len, "%s status failed code=%d",
					 service, ret);
				return -1;
			}
			snprintf(msg, msg_len, "%s", status_msg);
			return 0;
		}

		snprintf(msg, msg_len, "%s %s", service,
			 process_running(entry->process) ? "running" : "stopped");
		return 0;
	}

	if (entry->kind == SERVICE_KIND_COMMAND_HELPER &&
	    !strcmp(action, "stop")) {
		snprintf(msg, msg_len, "%s stop unsupported", service);
		return -1;
	}

	ret = run_service_script(entry, action);
	if (ret == 127) {
		snprintf(msg, msg_len, "service script not found");
		return -1;
	}
	if (ret != 0) {
		snprintf(msg, msg_len, "%s %s failed code=%d",
			 service, action, ret);
		return -1;
	}

	snprintf(msg, msg_len, "%s %s ok", service, action);
	return 0;
}
