// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Corellium LLC
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "mtfw.h"

struct hxt_metrics {
    int left, right;
    int top, bottom;
};

#define HXT_IOC_MAGIC           'h'
#define HXT_IOC_SET_CS          _IOW(HXT_IOC_MAGIC, 1, uint32_t)
#define HXT_IOC_RESET           _IO(HXT_IOC_MAGIC, 2)
#define HXT_IOC_READY           _IO(HXT_IOC_MAGIC, 3)
#define HXT_IOC_SETUP_IRQ       _IO(HXT_IOC_MAGIC, 4)
#define HXT_IOC_WAIT_IRQ        _IOW(HXT_IOC_MAGIC, 5, uint32_t)
#define HXT_IOC_METRICS         _IOW(HXT_IOC_MAGIC, 6, struct hxt_metrics)

#define MAX_DATA_CHUNK          16384

#define MT_CMD_LAST             0xE1
#define MT_DEV_INFO             0xE2
#define MT_REP_INFO             0xE3
#define MT_CTRL_WRITE_SHORT     0xE4
#define MT_CTRL_WRITE_LONG      0xE5
#define MT_CTRL_READ_SHORT      0xE6
#define MT_CTRL_READ_LONG       0xE7
#define MT_READ_FRAME_LEN       0xEA
#define MT_READ_LEN             0xEB
#define MT_SPI_Z2_WAKE_CMD      0xEE

static mtfw_item_t *mt_firmware;

static int bootload(int fd)
{
    int i, sz;
    unsigned char buf[16], rdbuf[MAX_DATA_CHUNK];
    mtfw_item_t *iter;

    if(ioctl(fd, HXT_IOC_RESET)) {
        perror("failed resetting controller");
        return 1;
    }

    memset(buf, 0, sizeof(buf));

    if(write(fd, buf, 4) != 4) {
        perror("failed writing command");
        return 1;
    }
    if(read(fd, buf, 4) != 4) {
        perror("failed reading result");
        return 1;
    }
    usleep(1000);

    if(ioctl(fd, HXT_IOC_SETUP_IRQ)) {
        perror("failed enabling IRQ");
        return 1;
    }

    if(ioctl(fd, HXT_IOC_RESET)) {
        perror("failed resetting controller");
        return 1;
    }

    if(ioctl(fd, HXT_IOC_WAIT_IRQ, 500))
        perror("failed waiting for boot IRQ");

    memset(buf, 0, sizeof(buf));

    if(ioctl(fd, HXT_IOC_SET_CS, 1)) {
        perror("failed asserting CS#");
        return 1;
    }
    usleep(1000);

    if(write(fd, buf, 4) != 4) {
        perror("failed writing command");
        return 1;
    }
    if(read(fd, buf, 4) != 4) {
        perror("failed reading result");
        return 1;
    }

    if(ioctl(fd, HXT_IOC_SET_CS, 0)) {
        perror("failed deasserting CS#");
        return 1;
    }

    for(iter=mt_firmware; iter; iter=iter->next) {
        switch(iter->type) {
        case MTFW_WRITE:
        case MTFW_WRITE_ACK:
            if(ioctl(fd, HXT_IOC_SET_CS, 1)) {
                perror("failed asserting CS#");
                return 1;
            }
            usleep(1000);
            for(i=0; i<iter->size; i+=MAX_DATA_CHUNK) {
                sz = iter->size - i;
                if(sz > MAX_DATA_CHUNK)
                    sz = MAX_DATA_CHUNK;

                if(write(fd, iter->data + i, sz) != sz) {
                    perror("failed writing data block");
                    return 1;
                }
                if(read(fd, rdbuf, sz) != sz) {
                    perror("failed reading result");
                    return 1;
                }
            }
            if(ioctl(fd, HXT_IOC_SET_CS, 0)) {
                perror("failed deasserting CS#");
                return 1;
            }

            if(iter->type == MTFW_WRITE_ACK) {
                buf[0] = 0x1A;
                buf[1] = 0xA1;
                if(ioctl(fd, HXT_IOC_SET_CS, 1)) {
                    perror("failed asserting CS#");
                    return 1;
                }
                usleep(1000);
                if(write(fd, buf, 2) != 2) {
                    perror("failed writing data block");
                    return 1;
                }
                if(read(fd, buf, 2) != 2) {
                    perror("failed reading result");
                    return 1;
                }
                if(ioctl(fd, HXT_IOC_SET_CS, 0)) {
                    perror("failed deasserting CS#");
                    return 1;
                }
                usleep(1000);
            }

            break;
        }
    }

    usleep(50000);
    return 0;
}

static unsigned get16le(unsigned char *buf)
{
    return ((unsigned)buf[1] << 8) | buf[0];
}

static void put16le(unsigned char *buf, unsigned val)
{
    buf[0] = val;
    buf[1] = val >> 8;
}

static int send_z2(int fd, unsigned char *cmd, unsigned char *rsp)
{
    unsigned char buf[16] = { 0 };
    unsigned i, csum = 0;

    if(!cmd)
        cmd = buf;
    if(!rsp)
        rsp = buf;

    for(i=0; i<14; i++)
        csum += cmd[i];
    put16le(cmd + 14, csum);

    if(ioctl(fd, HXT_IOC_SET_CS, 1)) {
        perror("failed asserting CS#");
        return 1;
    }
    usleep(1000);

    if(write(fd, cmd, 16) != 16) {
        perror("failed writing command");
        return 1;
    }
    if(read(fd, rsp, 16) != 16) {
        perror("failed reading result");
        return 1;
    }

    if(ioctl(fd, HXT_IOC_SET_CS, 0)) {
        perror("failed deasserting CS#");
        return 1;
    }
    usleep(1000);

    return 0;
}

static int send_wake(int fd)
{
    unsigned char cmd[16] = { MT_SPI_Z2_WAKE_CMD };;
    return send_z2(fd, cmd, NULL);
}

static int read_report(int fd, unsigned char rpt, unsigned char *buf, unsigned *plen)
{
    unsigned char cmd[16] = { MT_REP_INFO, rpt };
    unsigned char rsp[16], *pbuf;
    unsigned len, rlen;
    if(send_z2(fd, cmd, NULL))
        return 1;
    if(send_z2(fd, cmd, rsp))
        return 1;
    if(rsp[2])
        return 2;
    len = get16le(rsp + 3);
    rlen = len < *plen ? len : *plen;
    *plen = len;
    if(rlen <= 11) {
        cmd[0] = MT_CTRL_READ_SHORT;
        if(send_z2(fd, cmd, NULL))
            return 1;
        cmd[0] = MT_CMD_LAST;
        cmd[1] = 0;
        if(send_z2(fd, cmd, rsp))
            return 1;
        memcpy(buf, rsp + 3, rlen);
        return 0;
    }
    cmd[0] = MT_CTRL_READ_LONG;
    put16le(cmd + 3, rlen);
    if(send_z2(fd, cmd, NULL))
        return 1;
    pbuf = malloc(rlen + 5);
    if(!pbuf)
        return 1;
    memset(pbuf, 0xA5, rlen + 5);
    if(ioctl(fd, HXT_IOC_SET_CS, 1)) {
        free(pbuf);
        perror("failed asserting CS#");
        return 1;
    }
    usleep(1000);
    if(write(fd, pbuf, rlen + 5) != rlen + 5) {
        free(pbuf);
        perror("failed writing command");
        return 1;
    }
    if(read(fd, pbuf, rlen + 5) != rlen + 5) {
        free(pbuf);
        perror("failed reading result");
        return 1;
    }
    if(ioctl(fd, HXT_IOC_SET_CS, 0)) {
        free(pbuf);
        perror("failed deasserting CS#");
        return 1;
    }
    usleep(1000);
    memcpy(buf, pbuf + 3, rlen);
    free(pbuf);
    return 0;
}

static int write_report(int fd, unsigned char rpt, const unsigned char *buf, unsigned len)
{
    unsigned char cmd[16] = { MT_CTRL_WRITE_SHORT, rpt, len };
    memcpy(cmd + 3, buf, len);
    if(send_z2(fd, cmd, NULL))
        return 1;
    cmd[0] = MT_CMD_LAST;
    cmd[1] = cmd[2] = 0;
    return send_z2(fd, cmd, NULL);
}

#if 0
static int dump_report(int fd, unsigned char rpt, unsigned maxl)
{
    unsigned char *pbuf = malloc(maxl);
    unsigned len = maxl, i;
    int res;
    if(!pbuf)
        return 1;
    res = read_report(fd, rpt, pbuf, &len);
    if(res == 0) {
        fprintf(stderr, "rpt %02x:", rpt);
        for(i=0; i<len && i<maxl; i++)
            fprintf(stderr, " %02x", pbuf[i]);
        fprintf(stderr, " [%d]\n", len);
    } else if(res == 2)
        fprintf(stderr, "rpt %02x: unsupported\n", rpt);
    free(pbuf);
    return res;
}
#endif

int main(int argc, char *argv[])
{
    int fd;
    unsigned char d9[16];
    struct hxt_metrics hxtm;
    unsigned len;
    unsigned retries = 3;
    FILE *flist;
    char llist[256], *sep;
    struct stat statbuf;

    if(argc != 3 && argc != 4) {
        fprintf(stderr, "usage: hx-touchd <personality> <fwimage> <syscfg>\n"
                        "       <personality> = C1F5D,2\n"
                        "       <fwimage> = D10.mtprops\n"
                        "       <syscfg> = /dev/block/nvme0n3\n"
                        "   or: hx-touchd <fwlist> <syscfg>\n"
                        "       <fwlist> = file with <personality> <fwimage> pairs\n");
        return 1;
    }

    if(argc == 4) {
        mt_firmware = mtfw_load_firmware(argv[1], argv[2], argv[3]);
    } else {
        flist = fopen(argv[1], "r");
        if(!flist) {
            fprintf(stderr, "failed opening firmware list\n");
            return 1;
        }
        while(fgets(llist, sizeof(llist), flist)) {
            sep = strpbrk(llist, "#\n\r");
            if(sep)
                *sep = 0;
            sep = strpbrk(llist, " \t");
            if(!sep)
                continue;
            *sep = 0;
            sep ++;
            while(*sep == ' ' || *sep == '\t')
                sep ++;

            if(stat(sep, &statbuf))
                continue;
            mt_firmware = mtfw_load_firmware(llist, sep, argv[2]);
            if(mt_firmware)
                break;
        }
        fclose(flist);
    }
    if(!mt_firmware) {
        fprintf(stderr, "failed loading firmware\n");
        return 1;
    }

retry:
    fd = open("/dev/hx-touch", O_RDWR);
    if(fd < 0) {
        perror("failed opening /dev/hx-touch");
        return 1;
    }

    if(bootload(fd))
        return 1;

    send_wake(fd);

    len = 16;
    if(read_report(fd, 0xD9, d9, &len)) {
        if(!retries) {
            fprintf(stderr, "touch controller did not come up correctly\n");
            return 1;
        }
        retries --;
        close(fd);
        sleep(1);
        goto retry;
    }

    hxtm.left = (short)get16le(d9 + 8);
    hxtm.right = (short)get16le(d9 + 12);
    hxtm.top = (short)get16le(d9 + 14);
    hxtm.bottom = (short)get16le(d9 + 10);
    ioctl(fd, HXT_IOC_METRICS, &hxtm);

    write_report(fd, 0xAF, (unsigned char *)"\x00", 1);

    ioctl(fd, HXT_IOC_READY);

    while(1)
        sleep(60);

    close(fd);

    return 0;
}
