/*
 * Copyright (C) 2016 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int blank(int fd, int offset);
int read_mode();
int write_mode();

/* Read plain address from misc partiton and set the Wifi and BT mac addresses accordingly */

#define READ_MODE 0 /* read from misc partition and write to /system */
#define WRITE_MODE 1 /* read from WCNSS_qcom_wlan_nv and write into misc partition */
#define SET_MODE 2 /* set a specific mac address */

int main(int argc, char **argv) {
    char *mac_address = NULL;

    int mode = READ_MODE;
    int c;
    while ((c = getopt(argc, argv, "rws:")) != -1)
    switch (c) {
      case 'r':
        mode = READ_MODE;
        break;
      case 'w':
        mode = WRITE_MODE;
        break;
      case 's':
        mode = SET_MODE;
        mac_address = optarg;
        break;
      case '?':
        if (optopt == 's')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
    }

    if (mode == READ_MODE) {
        return read_mode();
    } else if (mode == WRITE_MODE) {
        return write_mode();
    }
}
  
int read_mode() {
    int fd1, fd2;
    int i;

    fd1 = open("/dev/block/bootdevice/by-name/misc", O_RDONLY);
    fd2 = open("/data/misc/wifi/config", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    char macbyte;
    char macbuf[3];
    if (!blank(fd1, 0x3000)) {

        write(fd2, "cur_etheraddr=", 14);
        printf("getmac: found mac in misc partition\n"); 

        for (i = 0; i < 6; i++) {
            lseek(fd1, 0x3000 + i, SEEK_SET);
            lseek(fd2, 0, SEEK_END);
            read(fd1, &macbyte, 1);
            sprintf(macbuf, "%02x", macbyte);
            write(fd2, &macbuf, 2);
            if(i != 5) write(fd2, ":", 1);
            printf("got octet %02x\n", macbyte);
        }

        write(fd2, "\n", 1);
        close(fd2);


        const char* src  = "/dev/block/bootdevice/by-name/system";
        const char* trgt = "/system";
        const char* type = "tmpfs";
        unsigned long mntflags = MS_REMOUNT;
        const char* opts = "";

        int result = 0;
        result = mount(src, trgt, type, mntflags, opts);
        if (result == 0) {
            printf("mount rw created at %s...\n", trgt);
        } else {
            printf("Error : Failed to remount rw %s\n"
              "Reason: %s [%d]\n",
             src, strerror(errno), errno);
        }

        /* seems like driver is actually using WCNSS_qcom_wlan_nv.bin, seems like we can leave WCNSS_qcom_wlan_nv_boot.bin alone */
        fd2 = open("/system/etc/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        for(i = 0; i < 6; i++) {
            lseek(fd1, 0x3000 + i, SEEK_SET);
            lseek(fd2, 0x0a + i, SEEK_SET);
            read(fd1, &macbyte, 1);
            write(fd2, &macbyte, 1);
        }

        /* need to close so we can remount /system */
        close(fd2);

        /* put /system back as read only */
        mntflags = MS_RDONLY | MS_REMOUNT | MS_RELATIME;
        opts = "data=ordered";
        result = mount(src, trgt, type, mntflags, opts);
        if (result == 0) {
            printf("mount ro created at %s...\n", trgt);
        } else {
            printf("Error : Failed to remount ro %s\n"
              "Reason: %s [%d]\n",
             src, strerror(errno), errno);
        }
    } else {
        printf("getmac: unfortunately misc partition is blank\n"); 
    }

    if (!blank(fd1, 0x4000)) {
        fd2 = open("/data/misc/bluetooth/bdaddr", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        for(i = 0; i < 6; i++) {
            lseek(fd1, 0x4000 + i, SEEK_SET);
            lseek(fd2, 0, SEEK_END);
            read(fd1, &macbyte, 1);
            sprintf(macbuf, "%02x", macbyte);
            write(fd2, &macbuf, 2);
            if(i != 5) write(fd2, ":", 1);
        }
    }

    close(fd2);
    close(fd1);

    return 0;
}

int write_mode() {
    int fd1, fd2;
    char macbyte;
    int i;

    fd1 = open("/system/etc/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin", S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    fd2 = open("/dev/block/bootdevice/by-name/misc", O_WRONLY);

    if (!blank(fd1, 0x0a)) {
        for (i = 0; i < 6; i++) {
            lseek(fd1, 0x0a + i, SEEK_SET);
            lseek(fd2, 0x3000 + i, SEEK_SET);
            read(fd1, &macbyte, 1);
            write(fd2, &macbyte, 1);
            printf("got octet %02x\n", macbyte);
        }

        close(fd2);
        close(fd1);
    } else {
        printf("getmac: unfortunately you are not on stock\n");
        return 1;
    }

/*
    close(fd2);
    close(fd1);
*/
    return 0;
}

int blank(int fd, int offset)
{
    char macbyte;
    int i, count = 0;

    for (i = 0; i < 6; i++) {
        lseek(fd, offset + i, SEEK_SET);
        read(fd, &macbyte, 1);

        if (!macbyte)
            count++;
        else
            count = 0;

        if (count > 2)
            return 1;
    }

    return 0;
}
