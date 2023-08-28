#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ouster_client2/client.h"
#include "ouster_client2/net.h"
#include "ouster_client2/log.h"
#include "ouster_client2/types.h"
#include "ouster_client2/lidar_context.h"




#define OUSTER_PACKET_HEADER_SIZE 32
#define OUSTER_COLUMN_HEADER_SIZE 12
#define OUSTER_CHANNEL_DATA_SIZE 12
#define OUSTER_COLUMS_PER_PACKET 16
#define OUSTER_PIXELS_PER_COLUMN 16
#define OUSTER_COLUMN_HEADER_SIZE 12
#define OUSTER_COL_SIZE ((OUSTER_PIXELS_PER_COLUMN*OUSTER_CHANNEL_DATA_SIZE)+OUSTER_COLUMN_HEADER_SIZE)





typedef enum 
{
    SOCK_INDEX_LIDAR,
    SOCK_INDEX_IMU,
    SOCK_INDEX_COUNT
} sock_index_t;








int main(int argc, char* argv[])
{

    int socks[2];
    socks[SOCK_INDEX_LIDAR] = ouster_client_create_lidar_udp_socket("7502");
    socks[SOCK_INDEX_IMU] = ouster_client_create_imu_udp_socket("7503");

    ouster_mat_t mat = {0};
    mat.esize = 4;
    mat.stride = mat.esize * 16;
    mat.data = malloc(mat.stride * 1024);

    lidar_context_t lidctx =
    {
        .packet_header_size = OUSTER_PACKET_HEADER_SIZE,
        .columns_per_packet = OUSTER_COLUMS_PER_PACKET,
        .col_size = OUSTER_COL_SIZE,
        .column_header_size = OUSTER_COLUMN_HEADER_SIZE,
        .channel_data_size = OUSTER_CHANNEL_DATA_SIZE,
        .pixels_per_column = OUSTER_PIXELS_PER_COLUMN,
    };

    //for(int i = 0; i < 100; ++i)
    while(1)
    {
        int timeout_seconds = 1;
        uint64_t a = net_select(socks, SOCK_INDEX_COUNT, timeout_seconds);

        if(a == 0)
        {
            ouster_log("Timeout\n");
        }


        if(a & (1 << SOCK_INDEX_LIDAR))
        {
            char buf[1024*10];
            int64_t n = net_read(socks[SOCK_INDEX_LIDAR], buf, sizeof(buf));
            //ouster_log("%-10s %5ji:  \n", "SOCK_LIDAR", (intmax_t)n);
            lidar_context_get_range(&lidctx, buf, &mat);
            if(lidctx.mid_last == lidctx.mid_max)
            {
                printf("Complete scan! num_valid_pixels=%i\n", mat.num_valid_pixels);
                memset(&mat, 0, sizeof(ouster_mat_t));
            }
        }


        if(a & (1 << SOCK_INDEX_IMU))
        {
            char buf[1024*256];
            int64_t n = net_read(socks[SOCK_INDEX_IMU], buf, sizeof(buf));
            //ouster_log("%-10s %5ji:  \n", "SOCK_IMU", (intmax_t)n);
        }
    }


    return 0;
}
