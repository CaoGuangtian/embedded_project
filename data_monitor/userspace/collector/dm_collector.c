#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "dm_protocol.h"
#include "dm_iio.h"

#define DM_RX_BUF_SIZE 1024
#define DM_LINE_SIZE 2048

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

struct dm_config {
	char server_ip[64];
	int server_port;
	int interval_ms;
	int ps_threshold;
	int als_threshold;
	int max_log_kb;
	int filter_alpha_percent;
	char config_path[256];
	char log_path[256];
};

struct dm_runtime {
	struct dm_config cfg;
	enum dm_work_mode mode;
	int led_on;
	int beep_on;
	int sockfd;
	int stop;
	pthread_mutex_t lock;
};

struct dm_sample_record {
	time_t ts;
	int env_ok;
	int imu_ok;
	struct dm_ap3216c_sample env;
	struct dm_icm20608_sample imu;
	double ir_filtered;
	double als_lux;
	double ps_filtered;
	double accel_g[3];
	double temp_c;
	double gyro_dps[3];
};

struct dm_filter_state {
	int initialized;
	double ir;
	double als;
	double ps;
	double accel[3];
	double temp;
	double gyro[3];
};

static struct dm_runtime g_rt;
static struct dm_filter_state g_filter;
static volatile sig_atomic_t g_signal_stop;

static void load_config_file(struct dm_config *cfg);

static long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(int ms)
{
	struct timespec req;

	if (ms < 1)
		ms = 1;

	req.tv_sec = ms / 1000;
	req.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&req, NULL);
}

static void log_msg(const char *fmt, ...)
{
	va_list ap;
	time_t now = time(NULL);

	printf("[%ld] ", (long)now);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
	fflush(stdout);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [-s server_ip] [-p port] [-i interval_ms] "
		"[-l log_path] [--max-log-kb value] "
		"[--config path] [--filter-alpha 0..100] "
		"[--ps value] [--als value]\n",
		prog);
}

static void config_defaults(struct dm_config *cfg)
{
	snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s",
		 DM_DEFAULT_SERVER_IP);
	cfg->server_port = DM_DEFAULT_SERVER_PORT;
	cfg->interval_ms = DM_DEFAULT_INTERVAL_MS;
	cfg->ps_threshold = DM_DEFAULT_PS_THRESHOLD;
	cfg->als_threshold = DM_DEFAULT_ALS_THRESHOLD;
	cfg->max_log_kb = DM_DEFAULT_MAX_LOG_KB;
	cfg->filter_alpha_percent = DM_DEFAULT_FILTER_ALPHA_PERCENT;
	snprintf(cfg->config_path, sizeof(cfg->config_path), "%s",
		 DM_DEFAULT_CONFIG_PATH);
	snprintf(cfg->log_path, sizeof(cfg->log_path), "%s",
		 DM_DEFAULT_LOG_PATH);
}

static int parse_int_arg(const char *text, int *out)
{
	char *end = NULL;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno || end == text || *end != '\0')
		return -1;
	if (value < 0 || value > 100000000)
		return -1;

	*out = (int)value;
	return 0;
}

static int parse_args(int argc, char **argv, struct dm_config *cfg)
{
	int i;

	config_defaults(cfg);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--config") && i + 1 < argc) {
			snprintf(cfg->config_path, sizeof(cfg->config_path), "%s",
				 argv[i + 1]);
			break;
		}
	}
	load_config_file(cfg);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-s") && i + 1 < argc) {
			snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
			if (parse_int_arg(argv[++i], &cfg->server_port))
				return -1;
		} else if (!strcmp(argv[i], "-i") && i + 1 < argc) {
			if (parse_int_arg(argv[++i], &cfg->interval_ms))
				return -1;
		} else if (!strcmp(argv[i], "-l") && i + 1 < argc) {
			snprintf(cfg->log_path, sizeof(cfg->log_path), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "--max-log-kb") && i + 1 < argc) {
			if (parse_int_arg(argv[++i], &cfg->max_log_kb))
				return -1;
		} else if (!strcmp(argv[i], "--config") && i + 1 < argc) {
			snprintf(cfg->config_path, sizeof(cfg->config_path), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "--filter-alpha") && i + 1 < argc) {
			if (parse_int_arg(argv[++i], &cfg->filter_alpha_percent))
				return -1;
		} else if (!strcmp(argv[i], "--ps") && i + 1 < argc) {
			if (parse_int_arg(argv[++i], &cfg->ps_threshold))
				return -1;
		} else if (!strcmp(argv[i], "--als") && i + 1 < argc) {
			if (parse_int_arg(argv[++i], &cfg->als_threshold))
				return -1;
		} else {
			return -1;
		}
	}

	if (cfg->server_port <= 0 || cfg->server_port > 65535)
		return -1;
	if (cfg->interval_ms < 100)
		cfg->interval_ms = 100;
	if (cfg->max_log_kb < 1)
		cfg->max_log_kb = 1;
	if (cfg->filter_alpha_percent < 0)
		cfg->filter_alpha_percent = 0;
	if (cfg->filter_alpha_percent > 100)
		cfg->filter_alpha_percent = 100;

	return 0;
}

static int ensure_parent_dir(const char *path)
{
	char tmp[256];
	char *slash;

	snprintf(tmp, sizeof(tmp), "%s", path);
	slash = strrchr(tmp, '/');
	if (!slash)
		return 0;
	if (slash == tmp)
		return 0;

	*slash = '\0';
	if (mkdir(tmp, 0755) == 0 || errno == EEXIST)
		return 0;

	return -1;
}

static void trim_line(char *text)
{
	char *start = text;
	char *end;

	while (*start == ' ' || *start == '\t')
		start++;
	if (start != text)
		memmove(text, start, strlen(start) + 1);

	end = text + strlen(text);
	while (end > text &&
	       (end[-1] == '\n' || end[-1] == '\r' ||
		end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
}

static void config_set_value(struct dm_config *cfg, const char *key,
			     const char *value)
{
	int parsed;

	if (!strcmp(key, "SERVER_IP")) {
		snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s", value);
	} else if (!strcmp(key, "SERVER_PORT") && !parse_int_arg(value, &parsed)) {
		cfg->server_port = parsed;
	} else if (!strcmp(key, "INTERVAL_MS") && !parse_int_arg(value, &parsed)) {
		cfg->interval_ms = parsed;
	} else if (!strcmp(key, "LOG_PATH")) {
		snprintf(cfg->log_path, sizeof(cfg->log_path), "%s", value);
	} else if (!strcmp(key, "MAX_LOG_KB") && !parse_int_arg(value, &parsed)) {
		cfg->max_log_kb = parsed;
	} else if (!strcmp(key, "PS_THRESHOLD") && !parse_int_arg(value, &parsed)) {
		cfg->ps_threshold = parsed;
	} else if (!strcmp(key, "ALS_THRESHOLD") && !parse_int_arg(value, &parsed)) {
		cfg->als_threshold = parsed;
	} else if (!strcmp(key, "FILTER_ALPHA_PERCENT") &&
		   !parse_int_arg(value, &parsed)) {
		cfg->filter_alpha_percent = parsed;
	}
}

static void load_config_file(struct dm_config *cfg)
{
	FILE *fp;
	char line[512];

	fp = fopen(cfg->config_path, "r");
	if (!fp)
		return;

	while (fgets(line, sizeof(line), fp)) {
		char *eq;
		char *key;
		char *value;

		trim_line(line);
		if (line[0] == '\0' || line[0] == '#')
			continue;

		eq = strchr(line, '=');
		if (!eq)
			continue;

		*eq = '\0';
		key = line;
		value = eq + 1;
		trim_line(key);
		trim_line(value);
		config_set_value(cfg, key, value);
	}

	fclose(fp);
}

static int save_config_file(const struct dm_config *cfg)
{
	FILE *fp;

	ensure_parent_dir(cfg->config_path);

	fp = fopen(cfg->config_path, "w");
	if (!fp)
		return -1;

	fprintf(fp, "SERVER_IP=%s\n", cfg->server_ip);
	fprintf(fp, "SERVER_PORT=%d\n", cfg->server_port);
	fprintf(fp, "INTERVAL_MS=%d\n", cfg->interval_ms);
	fprintf(fp, "LOG_PATH=%s\n", cfg->log_path);
	fprintf(fp, "MAX_LOG_KB=%d\n", cfg->max_log_kb);
	fprintf(fp, "PS_THRESHOLD=%d\n", cfg->ps_threshold);
	fprintf(fp, "ALS_THRESHOLD=%d\n", cfg->als_threshold);
	fprintf(fp, "FILTER_ALPHA_PERCENT=%d\n", cfg->filter_alpha_percent);
	fclose(fp);
	return 0;
}

static int read_ap3216c(struct dm_ap3216c_sample *sample)
{
	int value;
	if (dm_iio_read_attr(DM_IIO_AP3216C, "in_intensity0_raw", &value))
		return -1;
	sample->ir = (uint16_t)value;
	if (dm_iio_read_attr(DM_IIO_AP3216C, "in_illuminance0_raw", &value))
		return -1;
	sample->als = (uint16_t)value;
	if (dm_iio_read_attr(DM_IIO_AP3216C, "in_proximity0_raw", &value))
		return -1;
	sample->ps = (uint16_t)value;
	return 0;
}

static int read_icm20608(struct dm_icm20608_sample *sample)
{
	const char *attrs[] = { "in_accel_x_raw", "in_accel_y_raw",
				"in_accel_z_raw", "in_temp0_raw",
				"in_anglvel_x_raw", "in_anglvel_y_raw",
				"in_anglvel_z_raw" };
	int value, values[7], i;

	for (i = 0; i < 7; i++) {
		if (dm_iio_read_attr(DM_IIO_ICM20608, attrs[i], &value))
			return -1;
		values[i] = value;
	}
	sample->accel_x = (int16_t)values[0];
	sample->accel_y = (int16_t)values[1];
	sample->accel_z = (int16_t)values[2];
	sample->temp = (int16_t)values[3];
	sample->gyro_x = (int16_t)values[4];
	sample->gyro_y = (int16_t)values[5];
	sample->gyro_z = (int16_t)values[6];
	return 0;
}

static int write_output(const char *dev, int on)
{
	int fd = open(dev, O_WRONLY);
	char value = on ? '1' : '0';
	ssize_t n;

	if (fd < 0)
		return -1;

	n = write(fd, &value, 1);
	close(fd);
	return n == 1 ? 0 : -1;
}

static void set_led(int on)
{
	if (write_output(DM_DEV_LED, on) == 0) {
		pthread_mutex_lock(&g_rt.lock);
		g_rt.led_on = on;
		pthread_mutex_unlock(&g_rt.lock);
	}
}

static void set_beep(int on)
{
	if (write_output(DM_DEV_BEEP, on) == 0) {
		pthread_mutex_lock(&g_rt.lock);
		g_rt.beep_on = on;
		pthread_mutex_unlock(&g_rt.lock);
	}
}

static void apply_alarm_policy(const struct dm_sample_record *rec)
{
	int alarm = 0;
	enum dm_work_mode mode;
	int ps_threshold;
	int als_threshold;

	pthread_mutex_lock(&g_rt.lock);
	mode = g_rt.mode;
	ps_threshold = g_rt.cfg.ps_threshold;
	als_threshold = g_rt.cfg.als_threshold;
	pthread_mutex_unlock(&g_rt.lock);

	if (rec->env_ok &&
	    (rec->env.ps > ps_threshold || rec->env.als > als_threshold))
		alarm = 1;

	if (mode == DM_MODE_QUIET)
		alarm = 0;

	set_beep(alarm);
	set_led(mode != DM_MODE_ALARM_ONLY && rec->env_ok && rec->imu_ok);
}

static double ema(double old_value, double new_value, int alpha_percent)
{
	double alpha = alpha_percent / 100.0;

	if (alpha < 0.0)
		alpha = 0.0;
	if (alpha > 1.0)
		alpha = 1.0;

	return old_value * (1.0 - alpha) + new_value * alpha;
}

static void convert_and_filter_sample(struct dm_sample_record *rec)
{
	int alpha;
	double ir;
	double als_lux;
	double ps;
	double accel[3];
	double temp_c;
	double gyro[3];
	int i;

	pthread_mutex_lock(&g_rt.lock);
	alpha = g_rt.cfg.filter_alpha_percent;
	pthread_mutex_unlock(&g_rt.lock);

	ir = rec->env.ir;
	als_lux = rec->env.als * 0.35;
	ps = rec->env.ps;

	accel[0] = rec->imu.accel_x / 2048.0;
	accel[1] = rec->imu.accel_y / 2048.0;
	accel[2] = rec->imu.accel_z / 2048.0;
	temp_c = rec->imu.temp / 326.8 + 25.0;
	gyro[0] = rec->imu.gyro_x / 16.4;
	gyro[1] = rec->imu.gyro_y / 16.4;
	gyro[2] = rec->imu.gyro_z / 16.4;

	if (!g_filter.initialized) {
		g_filter.ir = ir;
		g_filter.als = als_lux;
		g_filter.ps = ps;
		for (i = 0; i < 3; i++) {
			g_filter.accel[i] = accel[i];
			g_filter.gyro[i] = gyro[i];
		}
		g_filter.temp = temp_c;
		g_filter.initialized = 1;
	} else {
		if (rec->env_ok) {
			g_filter.ir = ema(g_filter.ir, ir, alpha);
			g_filter.als = ema(g_filter.als, als_lux, alpha);
			g_filter.ps = ema(g_filter.ps, ps, alpha);
		}
		if (rec->imu_ok) {
			for (i = 0; i < 3; i++) {
				g_filter.accel[i] = ema(g_filter.accel[i],
							accel[i], alpha);
				g_filter.gyro[i] = ema(g_filter.gyro[i],
						       gyro[i], alpha);
			}
			g_filter.temp = ema(g_filter.temp, temp_c, alpha);
		}
	}

	rec->ir_filtered = g_filter.ir;
	rec->als_lux = g_filter.als;
	rec->ps_filtered = g_filter.ps;
	for (i = 0; i < 3; i++) {
		rec->accel_g[i] = g_filter.accel[i];
		rec->gyro_dps[i] = g_filter.gyro[i];
	}
	rec->temp_c = g_filter.temp;
}

static int ensure_log_header(const char *path)
{
	struct stat st;
	FILE *fp;

	ensure_parent_dir(path);

	if (stat(path, &st) == 0 && st.st_size > 0)
		return 0;

	fp = fopen(path, "a");
	if (!fp)
		return -1;

	fprintf(fp,
		"timestamp,mode,env_ok,ir,als,ps,imu_ok,"
		"accel_x,accel_y,accel_z,temp,gyro_x,gyro_y,gyro_z,"
		"ir_filtered,als_lux,ps_filtered,"
		"accel_x_g,accel_y_g,accel_z_g,temp_c,"
		"gyro_x_dps,gyro_y_dps,gyro_z_dps\n");
	fclose(fp);
	return 0;
}

static void rotate_log_if_needed(const char *path, int max_log_kb)
{
	struct stat st;
	char rotated[300];
	long max_bytes;

	if (max_log_kb <= 0)
		return;
	if (stat(path, &st) != 0)
		return;

	max_bytes = (long)max_log_kb * 1024L;
	if (st.st_size < max_bytes)
		return;

	snprintf(rotated, sizeof(rotated), "%s.1", path);
	unlink(rotated);
	rename(path, rotated);
}

static void append_log(const char *path, const struct dm_sample_record *rec)
{
	FILE *fp;
	enum dm_work_mode mode;
	int max_log_kb;

	pthread_mutex_lock(&g_rt.lock);
	max_log_kb = g_rt.cfg.max_log_kb;
	pthread_mutex_unlock(&g_rt.lock);

	rotate_log_if_needed(path, max_log_kb);
	if (ensure_log_header(path))
		return;

	fp = fopen(path, "a");
	if (!fp)
		return;

	pthread_mutex_lock(&g_rt.lock);
	mode = g_rt.mode;
	pthread_mutex_unlock(&g_rt.lock);

	fprintf(fp,
		"%ld,%s,%d,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,"
		"%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.2f\n",
		(long)rec->ts, dm_mode_name(mode), rec->env_ok,
		rec->env.ir, rec->env.als, rec->env.ps, rec->imu_ok,
		rec->imu.accel_x, rec->imu.accel_y, rec->imu.accel_z,
		rec->imu.temp, rec->imu.gyro_x, rec->imu.gyro_y,
		rec->imu.gyro_z, rec->ir_filtered, rec->als_lux,
		rec->ps_filtered, rec->accel_g[0], rec->accel_g[1],
		rec->accel_g[2], rec->temp_c, rec->gyro_dps[0],
		rec->gyro_dps[1], rec->gyro_dps[2]);

	fclose(fp);
}

static int tcp_connect_to_server(const char *ip, int port)
{
	struct sockaddr_in addr;
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		close(fd);
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static void close_socket_locked(void)
{
	if (g_rt.sockfd >= 0) {
		close(g_rt.sockfd);
		g_rt.sockfd = -1;
	}
}

static int send_line(const char *line)
{
	int fd;
	size_t len = strlen(line);
	ssize_t n;

	pthread_mutex_lock(&g_rt.lock);
	fd = g_rt.sockfd;
	pthread_mutex_unlock(&g_rt.lock);

	if (fd < 0)
		return -1;

	n = send(fd, line, len, MSG_NOSIGNAL);
	if (n != (ssize_t)len) {
		pthread_mutex_lock(&g_rt.lock);
		close_socket_locked();
		pthread_mutex_unlock(&g_rt.lock);
		return -1;
	}

	return 0;
}

static void send_register_message(void)
{
	char line[DM_LINE_SIZE];

	snprintf(line, sizeof(line),
		 "{\"type\":\"register\",\"device\":\"imx6ull-datamon\","
		 "\"version\":\"1.0\"}\n");
	send_line(line);
}

static void send_status_message(const struct dm_sample_record *rec)
{
	char line[DM_LINE_SIZE];
	enum dm_work_mode mode;
	int interval;
	int led_on;
	int beep_on;
	int filter_alpha;

	pthread_mutex_lock(&g_rt.lock);
	mode = g_rt.mode;
	interval = g_rt.cfg.interval_ms;
	led_on = g_rt.led_on;
	beep_on = g_rt.beep_on;
	filter_alpha = g_rt.cfg.filter_alpha_percent;
	pthread_mutex_unlock(&g_rt.lock);

	snprintf(line, sizeof(line),
		 "{\"type\":\"status\",\"ts\":%ld,\"mode\":\"%s\","
		 "\"interval_ms\":%d,\"led\":%d,\"beep\":%d,"
		 "\"env_ok\":%d,\"ir\":%u,\"als\":%u,\"ps\":%u,"
		 "\"imu_ok\":%d,\"accel_x\":%d,\"accel_y\":%d,"
		 "\"accel_z\":%d,\"temp\":%d,\"gyro_x\":%d,"
		 "\"gyro_y\":%d,\"gyro_z\":%d,"
		 "\"filter_alpha\":%d,\"ir_filtered\":%.2f,"
		 "\"als_lux\":%.2f,\"ps_filtered\":%.2f,"
		 "\"accel_x_g\":%.4f,\"accel_y_g\":%.4f,"
		 "\"accel_z_g\":%.4f,\"temp_c\":%.2f,"
		 "\"gyro_x_dps\":%.2f,\"gyro_y_dps\":%.2f,"
		 "\"gyro_z_dps\":%.2f}\n",
		 (long)rec->ts, dm_mode_name(mode), interval, led_on, beep_on,
		 rec->env_ok, rec->env.ir, rec->env.als, rec->env.ps,
		 rec->imu_ok, rec->imu.accel_x, rec->imu.accel_y,
		 rec->imu.accel_z, rec->imu.temp, rec->imu.gyro_x,
		 rec->imu.gyro_y, rec->imu.gyro_z, filter_alpha,
		 rec->ir_filtered, rec->als_lux, rec->ps_filtered,
		 rec->accel_g[0], rec->accel_g[1], rec->accel_g[2],
		 rec->temp_c, rec->gyro_dps[0], rec->gyro_dps[1],
		 rec->gyro_dps[2]);

	send_line(line);
}

static void send_ack(const char *cmd, int ok, const char *msg)
{
	char line[DM_LINE_SIZE];

	snprintf(line, sizeof(line),
		 "{\"type\":\"ack\",\"cmd\":\"%s\",\"ok\":%d,"
		 "\"msg\":\"%s\"}\n",
		 cmd ? cmd : "unknown", ok, msg ? msg : "");
	send_line(line);
}

static void json_escape(const char *src, char *dst, size_t dst_len)
{
	size_t i = 0;

	if (!dst_len)
		return;

	while (*src && i + 1 < dst_len) {
		if ((*src == '"' || *src == '\\') && i + 2 < dst_len) {
			dst[i++] = '\\';
			dst[i++] = *src++;
		} else if (*src == '\n' && i + 2 < dst_len) {
			dst[i++] = '\\';
			dst[i++] = 'n';
			src++;
		} else if (*src == '\r') {
			src++;
		} else {
			dst[i++] = *src++;
		}
	}
	dst[i] = '\0';
}

static void send_log_line(int index, const char *text)
{
	char escaped[700];
	char line[DM_LINE_SIZE];

	json_escape(text, escaped, sizeof(escaped));
	snprintf(line, sizeof(line),
		 "{\"type\":\"log_line\",\"index\":%d,\"text\":\"%s\"}\n",
		 index, escaped);
	send_line(line);
}

static void send_log_tail(int lines)
{
	struct dm_config cfg;
	FILE *fp;
	char ring[DM_LOG_QUERY_MAX_LINES][512];
	int count = 0;
	int start;
	int i;
	int index = 0;

	if (lines < 1)
		lines = 1;
	if (lines > DM_LOG_QUERY_MAX_LINES)
		lines = DM_LOG_QUERY_MAX_LINES;

	pthread_mutex_lock(&g_rt.lock);
	cfg = g_rt.cfg;
	pthread_mutex_unlock(&g_rt.lock);

	fp = fopen(cfg.log_path, "r");
	if (!fp) {
		send_ack("get_log", 0, "log open failed");
		return;
	}

	while (fgets(ring[count % lines], sizeof(ring[0]), fp))
		count++;
	fclose(fp);

	start = count > lines ? count - lines : 0;
	for (i = start; i < count; i++) {
		send_log_line(index++, ring[i % lines]);
	}
	send_ack("get_log", 1, "done");
}

static int runtime_should_stop(void)
{
	int stop;

	if (g_signal_stop) {
		pthread_mutex_lock(&g_rt.lock);
		g_rt.stop = 1;
		close_socket_locked();
		pthread_mutex_unlock(&g_rt.lock);
	}

	pthread_mutex_lock(&g_rt.lock);
	stop = g_rt.stop;
	pthread_mutex_unlock(&g_rt.lock);
	return stop;
}

static const char *json_find_value(const char *line, const char *key)
{
	static char pattern[64];
	const char *p;

	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = strstr(line, pattern);
	if (!p)
		return NULL;
	p = strchr(p + strlen(pattern), ':');
	if (!p)
		return NULL;
	return p + 1;
}

static int json_get_string(const char *line, const char *key,
			   char *out, size_t out_len)
{
	const char *p = json_find_value(line, key);
	size_t i = 0;

	if (!p)
		return -1;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '"')
		return -1;
	p++;

	while (*p && *p != '"' && i + 1 < out_len)
		out[i++] = *p++;

	if (*p != '"')
		return -1;

	out[i] = '\0';
	return 0;
}

static int json_get_int(const char *line, const char *key, int *out)
{
	const char *p = json_find_value(line, key);
	char tmp[32];
	size_t i = 0;

	if (!p)
		return -1;
	while (*p == ' ' || *p == '\t')
		p++;

	while ((*p == '-' || (*p >= '0' && *p <= '9')) &&
	       i + 1 < sizeof(tmp))
		tmp[i++] = *p++;
	tmp[i] = '\0';

	if (i == 0)
		return -1;

	return parse_int_arg(tmp, out);
}

static void cycle_mode(void)
{
	enum dm_work_mode mode;

	pthread_mutex_lock(&g_rt.lock);
	mode = g_rt.mode;
	if (mode == DM_MODE_NORMAL)
		g_rt.mode = DM_MODE_QUIET;
	else if (mode == DM_MODE_QUIET)
		g_rt.mode = DM_MODE_ALARM_ONLY;
	else
		g_rt.mode = DM_MODE_NORMAL;
	mode = g_rt.mode;
	pthread_mutex_unlock(&g_rt.lock);

	log_msg("mode changed to %s", dm_mode_name(mode));
}

static int set_mode_by_name(const char *name)
{
	enum dm_work_mode mode;

	if (!strcmp(name, "normal"))
		mode = DM_MODE_NORMAL;
	else if (!strcmp(name, "quiet"))
		mode = DM_MODE_QUIET;
	else if (!strcmp(name, "alarm_only"))
		mode = DM_MODE_ALARM_ONLY;
	else
		return -1;

	pthread_mutex_lock(&g_rt.lock);
	g_rt.mode = mode;
	pthread_mutex_unlock(&g_rt.lock);

	return 0;
}

static void handle_command(const char *line)
{
	char type[64];
	char cmd[64];
	char mode[32];
	int value;

	if (json_get_string(line, "type", type, sizeof(type)) ||
	    strcmp(type, "command") != 0)
		return;

	if (json_get_string(line, "cmd", cmd, sizeof(cmd))) {
		send_ack("unknown", 0, "missing cmd");
		return;
	}

	if (!strcmp(cmd, "set_led")) {
		if (json_get_int(line, "value", &value)) {
			send_ack(cmd, 0, "missing value");
			return;
		}
		set_led(value != 0);
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "set_beep")) {
		if (json_get_int(line, "value", &value)) {
			send_ack(cmd, 0, "missing value");
			return;
		}
		set_beep(value != 0);
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "set_interval")) {
		if (json_get_int(line, "value", &value) || value < 100) {
			send_ack(cmd, 0, "bad interval");
			return;
		}
		pthread_mutex_lock(&g_rt.lock);
		g_rt.cfg.interval_ms = value;
		pthread_mutex_unlock(&g_rt.lock);
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "set_filter")) {
		if (json_get_int(line, "alpha", &value) ||
		    value < 0 || value > 100) {
			send_ack(cmd, 0, "bad alpha");
			return;
		}
		pthread_mutex_lock(&g_rt.lock);
		g_rt.cfg.filter_alpha_percent = value;
		pthread_mutex_unlock(&g_rt.lock);
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "set_threshold")) {
		if (json_get_int(line, "ps", &value) == 0) {
			pthread_mutex_lock(&g_rt.lock);
			g_rt.cfg.ps_threshold = value;
			pthread_mutex_unlock(&g_rt.lock);
		}
		if (json_get_int(line, "als", &value) == 0) {
			pthread_mutex_lock(&g_rt.lock);
			g_rt.cfg.als_threshold = value;
			pthread_mutex_unlock(&g_rt.lock);
		}
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "set_mode")) {
		if (json_get_string(line, "mode", mode, sizeof(mode)) ||
		    set_mode_by_name(mode)) {
			send_ack(cmd, 0, "bad mode");
			return;
		}
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "set_config_path")) {
		char path[256];

		if (json_get_string(line, "path", path, sizeof(path))) {
			send_ack(cmd, 0, "missing path");
			return;
		}
		pthread_mutex_lock(&g_rt.lock);
		snprintf(g_rt.cfg.config_path, sizeof(g_rt.cfg.config_path),
			 "%s", path);
		pthread_mutex_unlock(&g_rt.lock);
		send_ack(cmd, 1, "ok");
	} else if (!strcmp(cmd, "save_config")) {
		struct dm_config cfg;

		pthread_mutex_lock(&g_rt.lock);
		cfg = g_rt.cfg;
		pthread_mutex_unlock(&g_rt.lock);

		if (save_config_file(&cfg))
			send_ack(cmd, 0, "save failed");
		else
			send_ack(cmd, 1, "saved");
	} else if (!strcmp(cmd, "get_log")) {
		int lines = 10;

		json_get_int(line, "lines", &lines);
		send_log_tail(lines);
	} else if (!strcmp(cmd, "shutdown")) {
		pthread_mutex_lock(&g_rt.lock);
		g_rt.stop = 1;
		pthread_mutex_unlock(&g_rt.lock);
		send_ack(cmd, 1, "stopping");
	} else {
		send_ack(cmd, 0, "unsupported command");
	}
}

static void *network_thread(void *arg)
{
	(void)arg;

	while (1) {
		struct dm_config cfg;
		char linebuf[DM_LINE_SIZE];
		size_t line_len;
		int stop;
		int fd;

		pthread_mutex_lock(&g_rt.lock);
		stop = g_rt.stop;
		cfg = g_rt.cfg;
		pthread_mutex_unlock(&g_rt.lock);

		if (stop)
			break;

		fd = tcp_connect_to_server(cfg.server_ip, cfg.server_port);
		if (fd < 0) {
			log_msg("connect %s:%d failed: %s", cfg.server_ip,
				cfg.server_port, strerror(errno));
			sleep_ms(3000);
			continue;
		}

		pthread_mutex_lock(&g_rt.lock);
		g_rt.sockfd = fd;
		pthread_mutex_unlock(&g_rt.lock);

		log_msg("connected to %s:%d", cfg.server_ip, cfg.server_port);
		send_register_message();

		line_len = 0;

		while (1) {
			fd_set rfds;
			struct timeval tv;
			char buf[DM_RX_BUF_SIZE];
			ssize_t n;
			ssize_t i;

			pthread_mutex_lock(&g_rt.lock);
			stop = g_rt.stop;
			fd = g_rt.sockfd;
			pthread_mutex_unlock(&g_rt.lock);

			if (stop || fd < 0)
				break;

			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0)
				continue;

			n = recv(fd, buf, sizeof(buf) - 1, 0);
			if (n <= 0) {
				log_msg("server disconnected");
				pthread_mutex_lock(&g_rt.lock);
				close_socket_locked();
				pthread_mutex_unlock(&g_rt.lock);
				break;
			}

			buf[n] = '\0';
			for (i = 0; i < n; i++) {
				if (buf[i] == '\n') {
					linebuf[line_len] = '\0';
					handle_command(linebuf);
					line_len = 0;
				} else if (line_len + 1 < sizeof(linebuf)) {
					linebuf[line_len++] = buf[i];
				} else {
					line_len = 0;
				}
			}
		}
	}

	return NULL;
}

static void *key_thread(void *arg)
{
	int fd = -1;
	int i;
	char path[64];
	char name[128];

	(void)arg;

	for (i = 0; i < 32; i++) {
		snprintf(path, sizeof(path), "%s%d", DM_DEV_KEY_PREFIX, i);
		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
		    strstr(name, "datamon-key"))
			break;
		close(fd);
		fd = -1;
	}
	if (fd < 0) {
		log_msg("key disabled: datamon input device not found: %s",
			strerror(errno));
		return NULL;
	}

	while (1) {
		struct input_event event;
		ssize_t n;
		int stop;

		stop = runtime_should_stop();
		if (stop)
			break;

		n = read(fd, &event, sizeof(event));
		if (n == sizeof(event) && event.type == EV_KEY && event.value == 1)
			cycle_mode();
		else if (n < 0 && errno != EINTR)
			sleep_ms(100);
	}

	close(fd);
	return NULL;
}

static void collect_once(struct dm_sample_record *rec)
{
	memset(rec, 0, sizeof(*rec));
	rec->ts = time(NULL);
	rec->env_ok = read_ap3216c(&rec->env) == 0;
	rec->imu_ok = read_icm20608(&rec->imu) == 0;
	convert_and_filter_sample(rec);
}

static void print_sample(const struct dm_sample_record *rec)
{
	enum dm_work_mode mode;

	pthread_mutex_lock(&g_rt.lock);
	mode = g_rt.mode;
	pthread_mutex_unlock(&g_rt.lock);

	printf("[%ld] mode=%s ", (long)rec->ts, dm_mode_name(mode));
	if (rec->env_ok)
		printf("env ir=%.1f als_lux=%.1f ps=%.1f ",
		       rec->ir_filtered, rec->als_lux, rec->ps_filtered);
	else
		printf("env=ERR ");

	if (rec->imu_ok)
		printf("imu acc_g=(%.3f,%.3f,%.3f) temp_c=%.2f gyro_dps=(%.2f,%.2f,%.2f)",
		       rec->accel_g[0], rec->accel_g[1], rec->accel_g[2],
		       rec->temp_c, rec->gyro_dps[0], rec->gyro_dps[1],
		       rec->gyro_dps[2]);
	else
		printf("imu=ERR");

	putchar('\n');
	fflush(stdout);
}

static void signal_handler(int signo)
{
	(void)signo;
	g_signal_stop = 1;
}

int main(int argc, char **argv)
{
	pthread_t net_tid;
	pthread_t key_tid;
	int have_net_thread = 0;
	int have_key_thread = 0;

	memset(&g_rt, 0, sizeof(g_rt));
	pthread_mutex_init(&g_rt.lock, NULL);
	g_rt.sockfd = -1;
	g_rt.mode = DM_MODE_NORMAL;

	if (parse_args(argc, argv, &g_rt.cfg)) {
		usage(argv[0]);
		return 1;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	log_msg("collector starting: server=%s:%d interval=%dms log=%s",
		g_rt.cfg.server_ip, g_rt.cfg.server_port, g_rt.cfg.interval_ms,
		g_rt.cfg.log_path);

	if (pthread_create(&net_tid, NULL, network_thread, NULL) == 0)
		have_net_thread = 1;
	else
		log_msg("network thread start failed");

	if (pthread_create(&key_tid, NULL, key_thread, NULL) == 0)
		have_key_thread = 1;
	else
		log_msg("key thread start failed");

	while (1) {
		struct dm_sample_record rec;
		struct dm_config cfg;
		int stop;
		long long start = now_ms();
		long long elapsed;

		stop = runtime_should_stop();
		pthread_mutex_lock(&g_rt.lock);
		cfg = g_rt.cfg;
		pthread_mutex_unlock(&g_rt.lock);

		if (stop)
			break;

		collect_once(&rec);
		apply_alarm_policy(&rec);
		print_sample(&rec);
		append_log(cfg.log_path, &rec);
		send_status_message(&rec);

		elapsed = now_ms() - start;
		if (elapsed < cfg.interval_ms)
			sleep_ms(cfg.interval_ms - (int)elapsed);
	}

	log_msg("collector stopping");
	set_beep(0);
	set_led(0);

	if (have_net_thread)
		pthread_join(net_tid, NULL);
	if (have_key_thread) {
		pthread_cancel(key_tid);
		pthread_join(key_tid, NULL);
	}

	pthread_mutex_destroy(&g_rt.lock);
	return 0;
}
