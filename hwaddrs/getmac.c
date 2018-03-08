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
#include <stdlib.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int blank(int fd, int offset);
int read_from_misc_partition(int);
int write_to_misc_partition(int);
int write_mac_address_to_misc_partition(int, char *);

/* Read plain address from misc partiton and set the Wifi and BT mac addresses accordingly */

#define BOOT_MODE 0 /* read from misc partition and write to /system */
#define READ_MODE 1 /* display values from misc parttion without writing to /system */
#define WRITE_MODE 2 /* read from WCNSS_qcom_wlan_nv and write into misc partition */
#define READ_SYSTEM_MODE 3 /* read from system/WCNSS_qcom_wlan_nv and don't write into misc partition */
#define SET_WIFI_MODE 4 /* set a specific mac address */
#define SET_BT_MODE 5 /* set a specific mac address */

int main(int argc, char **argv) {
    char *mac_address = NULL;

    int mode = BOOT_MODE;
    int c;
    while ((c = getopt(argc, argv, "hbcrws:t:")) != -1)
    switch (c) {
      case 'b':
        mode = BOOT_MODE;
        break;
      case 'c':
        mode = READ_SYSTEM_MODE;
        break;
      case 'r':
        mode = READ_MODE;
        break;
      case 'w':
        mode = WRITE_MODE;
        break;
      case 's':
        mode = SET_WIFI_MODE;
        mac_address = optarg;
        break;
      case 't':
        mode = SET_BT_MODE;
        mac_address = optarg;
        break;
      case 'h':
        printf("-b reads from misc partition and writes to /system\n");
        printf("-c reads from /system files to show current address without writing to misc partition\n");
        printf("-r reads from misc partition without writing to to /system\n");
        printf("-w reads from /system files and writes to misc partition (should run on stock rom only)\n");
        printf("-s <mac address in aa:bb:cc:dd:ee form> sets wifi mac address\n");
        printf("-t <mac address in aa:bb:cc:dd:ee form> sets bluetooth mac address\n");
        exit(0);
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

    if (mode == BOOT_MODE) {
        return read_from_misc_partition(0);
    } else if (mode == READ_MODE) {
        return read_from_misc_partition(1);
    } else if (mode == WRITE_MODE) {
        return write_to_misc_partition(0);
    } else if (mode == READ_SYSTEM_MODE) {
        return write_to_misc_partition(1);
    } else if (mode == SET_WIFI_MODE) {
        return write_mac_address_to_misc_partition(1, mac_address);
    } else if (mode == SET_BT_MODE) {
        return write_mac_address_to_misc_partition(0, mac_address);
    }
}
  
int read_from_misc_partition(int preview) {
    int fd1, fd2;
    int i;

    fd1 = open("/dev/block/bootdevice/by-name/misc", O_RDONLY);
    if (preview == 0) {
        fd2 = open("/data/misc/wifi/config", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    }

    char macbyte = 0;
    char macbuf[3];
    if (!blank(fd1, 0x3000)) {

        if (preview == 0) {
            write(fd2, "cur_etheraddr=", 14);
        }

        printf("current wifi mac=");
        for (i = 0; i < 6; i++) {
            lseek(fd1, 0x3000 + i, SEEK_SET);
            if (preview == 0) {
                lseek(fd2, 0, SEEK_END);
            }
            read(fd1, &macbyte, 1);
            sprintf(macbuf, "%02x", macbyte);
            if (preview == 0) {
                write(fd2, &macbuf, 2);
                if (i != 5) write(fd2, ":", 1);
            }
            printf("%02x", macbyte);
            if (i != 5) printf(":");
        }
        printf("\n");
        if (preview == 0) {
            write(fd2, "\n", 1);
            close(fd2);
        }


        if (preview == 0) {
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
            for (i = 0; i < 6; i++) {
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

        }
    } else {
        printf("getmac: unfortunately misc partition for wifi mac is blank\n"); 
    }

    int fd3;
    if (!blank(fd1, 0x4000)) {
        if (preview == 0) {
            fd2 = open("/data/misc/bluetooth/bdaddr", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            fd3 = open("/data/property/persist.service.bdroid.bdaddr", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        }
        printf("current bt mac=");
        for (i = 0; i < 6; i++) {
            lseek(fd1, 0x4000 + i, SEEK_SET);
            if (preview == 0) {
                lseek(fd2, 0, SEEK_END);
                lseek(fd3, 0, SEEK_END);
            }
            read(fd1, &macbyte, 1);
            sprintf(macbuf, "%02x", macbyte);
            if (preview == 0) {
                write(fd2, &macbuf, 2);
                if(i != 5) write(fd2, ":", 1);
                write(fd3, &macbuf, 2);
                if(i != 5) write(fd3, ":", 1);
            }
            printf("%02x", macbyte);
            if (i != 5) printf(":");
        }
        printf("\n");
    } else {
        printf("getmac: unfortunately misc partition for bt mac is blank\n"); 
    }

    if (preview == 0) {
        close(fd2);
        close(fd3);
    }
    close(fd1);

    return 0;
}

int write_to_misc_partition(int preview) {
    int fd1, fd2;
    char macbyte = 0;
    int i;

    fd1 = open("/system/etc/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin", S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (preview == 0) {
        fd2 = open("/dev/block/bootdevice/by-name/misc", O_WRONLY);
    }

    if (!blank(fd1, 0x0a)) {
        printf("current wifi mac=");
        for (i = 0; i < 6; i++) {
            lseek(fd1, 0x0a + i, SEEK_SET);
            if (preview == 0) {
                lseek(fd2, 0x3000 + i, SEEK_SET);
            }
            read(fd1, &macbyte, 1);
            if (preview == 0) {
                write(fd2, &macbyte, 1);
            }
            printf("%02x", macbyte);
            if (i != 5) printf(":");
        }
        printf("\n");

        if (preview == 0) {
            close(fd2);
        }
        close(fd1);
    } else {
        printf("getmac: unfortunately you are not on stock\n");
        return 1;
    }

    /* on stock the bluetooth address is stored /data/misc/bluedroid/bt_config.conf
L:\Slice1\Hardware\LG.K20.Plus\wlan>more bt_config.conf.LGMP26011a4d4d4
[Info]
FileSource = Empty
TimeCreated = 2017-01-01 00:27:42

[Adapter]
Address = 88:36:5f:09:ca:25
     */

    return 0;
}

int write_mac_address_to_misc_partition(int is_wifi, char *macaddr) {
   char macbyte = 0;
   int i;
   int offset = 0x3000;
   if (is_wifi == 0) {
       offset = 0x4000;
   }

   unsigned int num;
   printf("set mac to = %s\n", (macaddr));

   int fd2;
   fd2 = open("/dev/block/bootdevice/by-name/misc", O_WRONLY);

   for (i = 0; i < 6; i++) {
       char *mac_ind = (char *)malloc(3);
       char mac_conv[5];
       strncpy(mac_ind, macaddr+i*3, 2);
       mac_ind[2] = '\0';
       sprintf(mac_conv, "0x%s", mac_ind);
       macbyte = strtol(mac_conv, NULL, 10);
/*
       printf("mac_ind is %s\n", (mac_ind));
       printf("mac_conv is %s\n", (mac_conv));
*/
       sscanf(mac_ind, "%x", &num);
       macbyte = num;
/*
       printf("num=%d, hex=%02x\n", (num, macbyte));
*/

       lseek(fd2, offset + i, SEEK_SET);
       write(fd2, &macbyte, 1);

       free(mac_ind);
   }
   close(fd2);

   return 0;
}

int blank(int fd, int offset) {
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
