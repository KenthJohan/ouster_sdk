#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <ouster_clib/sock.h>
#include <ouster_clib/net.h>
#include <ouster_clib/log.h>
#include <ouster_clib/types.h>
#include <ouster_clib/lidar.h>
#include <ouster_clib/mat.h>
#include <ouster_clib/os_file.h>
#include <ouster_clib/meta.h>



#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>


typedef enum 
{
    SOCK_INDEX_LIDAR,
    SOCK_INDEX_IMU,
    SOCK_INDEX_COUNT
} sock_index_t;


int pixsize_to_cv_type(int size)
{
    switch (size)
    {
    case 1: return CV_8U;
    case 2: return CV_16U;
    case 4: return CV_32S;
    }
    return 0;
}

cv::Mat ouster_get_cvmat(ouster_field_t * field)
{
    cv::Mat m(field->mat.dim[2], field->mat.dim[1], pixsize_to_cv_type(field->mat.dim[0]), field->mat.data);
    return m;
}



int main(int argc, char* argv[])
{
    {
        char cwd[1024] = {0};
        getcwd(cwd, sizeof(cwd));
        printf("Current working dir: %s\n", cwd);
    }

    char const * metastr = ouster_os_file_read("../meta1.json");
    ouster_meta_t meta = {0};
    ouster_meta_parse(metastr, &meta);
    printf("Column window: %i %i\n", meta.column_window[0], meta.column_window[1]);

    int socks[2];
    socks[SOCK_INDEX_LIDAR] = ouster_sock_create_udp_lidar("7502");
    socks[SOCK_INDEX_IMU] = ouster_sock_create_udp_imu("7503");

    #define FIELD_COUNT 4
    ouster_field_t fields[FIELD_COUNT] = {
        {.quantity = OUSTER_QUANTITY_RANGE},
        {.quantity = OUSTER_QUANTITY_REFLECTIVITY},
        {.quantity = OUSTER_QUANTITY_SIGNAL},
        {.quantity = OUSTER_QUANTITY_NEAR_IR}
    };

    ouster_field_init(fields, FIELD_COUNT, &meta);



    cv::namedWindow("0", cv::WINDOW_FREERATIO);
    cv::namedWindow("1", cv::WINDOW_FREERATIO);
    cv::namedWindow("2", cv::WINDOW_FREERATIO);
    cv::namedWindow("3", cv::WINDOW_FREERATIO);
    cv::resizeWindow("0", fields[0].mat.dim[1]*20, fields[0].mat.dim[2]*2);
    cv::resizeWindow("1", fields[1].mat.dim[1]*20, fields[1].mat.dim[2]*2);
    cv::resizeWindow("2", fields[2].mat.dim[1]*20, fields[2].mat.dim[2]*2);
    cv::resizeWindow("3", fields[3].mat.dim[1]*20, fields[3].mat.dim[2]*2);

    ouster_lidar_t lidar = {0};

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
            //ouster_log("%-10s %5ji %5ji:  \n", "SOCK_LIDAR", (intmax_t)n, meta.lidar_packet_size);
            if(n != meta.lidar_packet_size)
            {
                ouster_log("%-10s %5ji of %5ji:  \n", "SOCK_LIDAR", (intmax_t)n, meta.lidar_packet_size);
            }
            ouster_lidar_get_fields(&lidar, &meta, buf, fields, FIELD_COUNT);
            if(lidar.last_mid == meta.column_window[1])
            {
                ouster_mat4_apply_mask_u32(&fields[0].mat, fields[0].mask);
                cv::Mat mat_f0 = ouster_get_cvmat(fields + 0);
                cv::Mat mat_f1 = ouster_get_cvmat(fields + 1);
                cv::Mat mat_f2 = ouster_get_cvmat(fields + 2);
                cv::Mat mat_f3 = ouster_get_cvmat(fields + 3);
                cv::Mat mat_f0_show;
                cv::Mat mat_f1_show;
                cv::Mat mat_f2_show;
                cv::Mat mat_f3_show;
                cv::normalize(mat_f0, mat_f0_show, 0, 255, cv::NORM_MINMAX, CV_8UC1);
                cv::normalize(mat_f1, mat_f1_show, 0, 255, cv::NORM_MINMAX, CV_8UC1);
                cv::normalize(mat_f2, mat_f2_show, 0, 255, cv::NORM_MINMAX, CV_8UC1);
                cv::normalize(mat_f3, mat_f3_show, 0, 255, cv::NORM_MINMAX, CV_8UC1);
                cv::imshow("0", mat_f0_show);
                cv::imshow("1", mat_f1_show);
                cv::imshow("2", mat_f2_show);
                cv::imshow("3", mat_f3_show);

                //printf("mat = %i of %i\n", mat.num_valid_pixels, mat.dim[1] * mat.dim[2]);
                ouster_mat4_zero(&fields[0].mat);

                //int key = cv::waitKey(1);
                int key = cv::pollKey();
                if(key == 27){break;}
            }
        }


        if(a & (1 << SOCK_INDEX_IMU))
        {
            char buf[1024*256];
            net_read(socks[SOCK_INDEX_IMU], buf, sizeof(buf));
            //ouster_log("%-10s %5ji:  \n", "SOCK_IMU", (intmax_t)n);
        }
    }


    return 0;
}
