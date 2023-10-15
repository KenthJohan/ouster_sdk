#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ouster_clib.h>

#include "convert.h"
#include "tigr.h"

typedef enum {
	SOCK_INDEX_LIDAR,
	SOCK_INDEX_IMU,
	SOCK_INDEX_COUNT
} sock_index_t;

typedef enum {
	FIELD_RANGE,
	FIELD_COUNT
} field_t;

typedef enum {
	SNAPSHOT_MODE_UNKNOWN,
	SNAPSHOT_MODE_RAW,
	SNAPSHOT_MODE_DESTAGGER,
	MONITOR_MODE_COUNT
} monitor_mode_t;

mode_t get_mode(char const *str)
{
	if (strcmp(str, "raw") == 0) {
		return SNAPSHOT_MODE_RAW;
	}
	if (strcmp(str, "destagger") == 0) {
		return SNAPSHOT_MODE_DESTAGGER;
	}
	return SNAPSHOT_MODE_UNKNOWN;
}

void print_help(int argc, char *argv[])
{
	printf("Hello welcome to %s!!\n", argv[0]);
	printf("This tool takes snapshots from LiDAR sensor and saves it as PNG image.\n");
	printf("To use this tool you will have to provide the meta file that correspond to the LiDAR sensor configuration\n");
	printf("Arguments:\n");
	printf("\targ1: The meta file that correspond to the LiDAR sensor configuration\n");
	printf("\targ2: View mode\n");
	printf("\t\topt1: raw\n");
	printf("\t\topt2: destagger\n");
	printf("Examples:\n");
	printf("\t$ %s <%s> <%s>\n", argv[0], "meta.json", "mode");
	printf("\t$ %s <%s> %s\n", argv[0], "meta.json", "raw");
	printf("\t$ %s <%s> %s\n", argv[0], "meta.json", "destagger");
}



typedef struct
{
	monitor_mode_t mode;
	char const *metafile;
	ouster_meta_t meta;
	pthread_mutex_t lock;
	Tigr *bmp;
} app_t;

void *rec(void *ptr)
{
	app_t *app = ptr;
	ouster_meta_t *meta = &app->meta;

	ouster_field_t fields[FIELD_COUNT] = {
	    [FIELD_RANGE] = {.quantity = OUSTER_QUANTITY_RANGE, .depth = 4}};

	ouster_field_init(fields, FIELD_COUNT, meta);

	int socks[2];
	socks[SOCK_INDEX_LIDAR] = ouster_sock_create_udp_lidar(7502);
	socks[SOCK_INDEX_IMU] = ouster_sock_create_udp_imu(7503);

	ouster_lidar_t lidar = {0};

	int w = app->meta.midw;
	int h = app->meta.pixels_per_column;

	while (1) {
		int timeout_sec = 1;
		int timeout_usec = 0;
		uint64_t a = ouster_net_select(socks, SOCK_INDEX_COUNT, timeout_sec, timeout_usec);

		if (a == 0) {
			ouster_log("Timeout\n");
		}

		if (a & (1 << SOCK_INDEX_LIDAR)) {
			char buf[OUSTER_NET_UDP_MAX_SIZE];
			int64_t n = ouster_net_read(socks[SOCK_INDEX_LIDAR], buf, sizeof(buf));
			if(n == meta->lidar_packet_size)
			{
				ouster_lidar_get_fields(&lidar, meta, buf, fields, FIELD_COUNT);
				if (lidar.last_mid == meta->mid1) {
					if (app->mode == SNAPSHOT_MODE_RAW) {}
					if (app->mode == SNAPSHOT_MODE_DESTAGGER) {
						ouster_field_destagger(fields, FIELD_COUNT, meta);
					}
					convert_u32_to_bmp(fields[FIELD_RANGE].data, app->bmp, w, h);
					ouster_field_zero(fields, FIELD_COUNT);
					printf("frame=%i, mid_loss=%i\n", lidar.frame_id, lidar.mid_loss);
				}
			}
			else
			{
				printf("Bytes received (%ji) does not match lidar_packet_size (%ji)\n", (intmax_t)n, (intmax_t)app->meta.lidar_packet_size);
			}
		}

		if (a & (1 << SOCK_INDEX_IMU)) {
			// char buf[OUSTER_NET_UDP_MAX_SIZE];
			// int64_t n = ouster_net_read(socks[SOCK_INDEX_IMU], buf, sizeof(buf));
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	printf("===================================================================\n");
	ouster_fs_pwd();

	if (argc != 3) {
		print_help(argc, argv);
		return 0;
	}

	app_t app = {
	    .lock = PTHREAD_MUTEX_INITIALIZER};
	app.metafile = argv[1];
	app.mode = get_mode(argv[2]);

	if (app.mode == SNAPSHOT_MODE_UNKNOWN) {
		printf("The mode <%s> does not exist.\n", argv[2]);
		print_help(argc, argv);
		return 0;
	}

	{
		char *content = ouster_fs_readfile(app.metafile);
		if (content == NULL) {
			return 0;
		}
		ouster_meta_parse(content, &app.meta);
		free(content);
		printf("Column window: %i %i\n", app.meta.mid0, app.meta.mid1);
	}



	int w = app.meta.midw;
	int h = app.meta.pixels_per_column;

	app.bmp = tigrBitmap(w, h);

	{
		pthread_t thread1;
		int rc = pthread_create(&thread1, NULL, rec, (void *)&app);
		ouster_assert(rc == 0, "");
	}

	int flags = 0;
	Tigr *screen = NULL;

again:
	if(screen)
	{
		tigrFree(screen);
	}
	screen = tigrWindow(w, h, "ouster_view", flags|TIGR_AUTO);
	while (!tigrClosed(screen)) {
		int c = tigrReadChar(screen);
		//printf("c: %i\n", c);
		switch (c)
		{
		case '1':
			flags &= ~(TIGR_2X | TIGR_3X | TIGR_4X);
			goto again;
		case '2':
			flags &= ~(TIGR_2X | TIGR_3X | TIGR_4X);
			flags |= TIGR_2X;
			goto again;
		case '3':
			flags &= ~(TIGR_2X | TIGR_3X | TIGR_4X);
			flags |= TIGR_3X;
			goto again;
		case '4':
			flags &= ~(TIGR_2X | TIGR_3X | TIGR_4X);
			flags |= TIGR_4X;
			goto again;
		
		default:
			break;
		}
		tigrClear(screen, tigrRGB(0x80, 0x90, 0xa0));
		pthread_mutex_lock(&app.lock);
		tigrBlit(screen, app.bmp, 0, 0, 0, 0, w, h);
		pthread_mutex_unlock(&app.lock);
		tigrUpdate(screen);
	}
	tigrFree(screen);

	return 0;
}
