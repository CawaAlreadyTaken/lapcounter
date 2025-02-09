#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <linux/can.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "../src/lapcounter.h"

typedef struct __is_packed {
    float latitude;
    float longitude;
} Primary_GPS_COORDS;

size_t serialize_Primary_GPS_COORDS(uint8_t* buffer, float latitude, float longitude) {
    Primary_GPS_COORDS primary_gps_coords = { latitude, longitude };
    memcpy(buffer, &primary_gps_coords, sizeof(Primary_GPS_COORDS));
    return sizeof(Primary_GPS_COORDS);
}

size_t deserialize_Primary_GPS_COORDS(uint8_t* buffer, Primary_GPS_COORDS* primary_gps_coords) {
    memcpy(primary_gps_coords, buffer, sizeof(Primary_GPS_COORDS));
    return sizeof(Primary_GPS_COORDS);
}


void main(int argc, char **argv) {

    double time_spent = 0.0;
    int bytes_read;     // to remove warning on fscanf
    double x, y;        // to read from file
    char mode;          // p: pixel, r: real

    // modified from here
    int s;
    int j;
    int nBytesRead;
    struct sockaddr_can addr1;
    struct sockaddr_can addr2;
    struct can_frame frame;
    struct ifreq ifr1;
    struct ifreq ifr2;

    const char *ifname1 = "vcan0";
    const char *ifname2 = "can0";

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
        perror("Error while opening socket for reading virtual can");
        return;
    }

    if ((j = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
        perror("Error while opening socket for writing can");
        return;
    }

    strcpy(ifr1.ifr_name, ifname1);
    strcpy(ifr2.ifr_name, ifname2);
    ioctl(s, SIOCGIFINDEX, &ifr1);
    ioctl(j, SIOCGIFINDEX, &ifr2);

    addr1.can_family  = AF_CAN;
    addr1.can_ifindex = ifr1.ifr_ifindex;
    addr2.can_family  = AF_CAN;
    addr2.can_ifindex = ifr2.ifr_ifindex;

    printf("%s at index %d\n", ifname1, ifr1.ifr_ifindex);
    printf("%s at index %d\n", ifname2, ifr2.ifr_ifindex);

    if (bind(s, (struct sockaddr *)&addr1, sizeof(addr1)) == -1) {
            perror("Error in socket bind");
            return;
    }

    frame.can_id  = 0x6A0;  //change this
    frame.can_dlc = 4; // 1 for lap count, 4 for timestamp
    frame.data[0] = 0x12;
    frame.data[1] = 0x34;
    frame.data[2] = 0x56;
    frame.data[3] = 0x78;


    nBytesRead = write(s, &frame, sizeof(struct can_frame));  //write on canbus that lapcounter started
    if (nBytesRead == -1) {
        fprintf(stderr, "Tried to write on %s but failed. Is it on?\n", ifname1);
        return;
    }

    //printf("Wrote %d bytes to canbus.\n", nbytes);
    // to here

    lc_point_t point;   // to pass to evaluation
    lc_counter_t * lp = lc_init(NULL);  // initialization with default settings

    //assert(argc >= 3);  // assert if command has filename and mode
    //FILE *file = fopen(argv[1], "r");
    //mode = argv[2][0];  // modes should be r/p

    clock_t begin = clock();

    while (1) {
        struct can_frame frameRead;
        nBytesRead = read(s, &frameRead, sizeof(struct can_frame));
        uint8_t* buffer_primary_gps_coords = (uint8_t*)malloc(sizeof(Primary_GPS_COORDS));
        buffer_primary_gps_coords[0] = (uint8_t)frameRead.data[0];
        buffer_primary_gps_coords[1] = (uint8_t)frameRead.data[1];
        buffer_primary_gps_coords[2] = (uint8_t)frameRead.data[2];
        buffer_primary_gps_coords[3] = (uint8_t)frameRead.data[3];
        buffer_primary_gps_coords[4] = (uint8_t)frameRead.data[4];
        buffer_primary_gps_coords[5] = (uint8_t)frameRead.data[5];
        buffer_primary_gps_coords[6] = (uint8_t)frameRead.data[6];
        buffer_primary_gps_coords[7] = (uint8_t)frameRead.data[7];

        Primary_GPS_COORDS* primary_gps_coords_d = (Primary_GPS_COORDS*)malloc(sizeof(Primary_GPS_COORDS));
        deserialize_Primary_GPS_COORDS(buffer_primary_gps_coords, primary_gps_coords_d);
        printf("%f %f\n", primary_gps_coords_d->latitude, primary_gps_coords_d->longitude);
        point.x = (double)primary_gps_coords_d->latitude;
        point.y = (double)primary_gps_coords_d->longitude;
        //printf("%lf, %lf\n", point.x, point.y);
        //printf("%lf, %lf\n", point.x, point.y);
        if (lc_eval_point(lp, &point)) {
            struct can_frame frameWrite;
            frameWrite.can_id  = 0x6A0;  //change this
            frameWrite.can_dlc = 1; // 1 for lap count
            uint8_t lapCount = lp->laps_count;  //check if this overwrites data on frame
            frameWrite.data[0] = lapCount;
            int nbytes = write(s, &frameWrite, sizeof(struct can_frame));
            printf("BIP!\n");

        }
    }
    printf("\nLAP COUNT: %d\n", lp->laps_count);

    lc_reset(lp); // reset object (removes everything but thresholds)
    lc_destroy(lp);

    clock_t end = clock();
    time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
    printf("The elapsed time is %f seconds\n", time_spent);
}
