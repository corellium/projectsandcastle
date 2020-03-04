#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define DTREECMD "fdt\n"
#define BOOTCMD "bootl\n"

int main(int argc, char *argv[])
{
    libusb_device **devs;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    libusb_device_handle *device = NULL;
    unsigned int i = 0, j = 0;
    unsigned vid = 0x05ac, pid = 0x4141;
    int status = 0, tsize = 0, chunk, done;
    struct stat st;
    size_t size, dsize;
    FILE *fp;
    char *kbuff = NULL;
    char *dtree[1024*64] = {0};

    if (argc < 3) {
        printf("syntax :./loadbin linux.bin dtree.bin\n");
        exit(1);
    }

    status = libusb_init(NULL);
    if (status < 0)
        return status;

    status = libusb_get_device_list(NULL, &devs);
    if (status < 0)
        return status;

    fp = fopen(argv[1], "r");
    if(!fp) {
        printf("Failed to open file\n");
        status = 1;
        goto err;
    }

    if(stat(argv[1], &st) != 0) {
        printf("Failed to get file size\n");
        fclose(fp);
        status = 1;
        goto err;
    }
    size = st.st_size;

    kbuff = malloc(size);
    if(!kbuff) {
        printf("failed to allocate buffer\n");
        fclose(fp);
        status = 1;
        goto err;
    }

    if(fread(kbuff, 1, size, fp) != size) {
        printf("error reading file %s\n", argv[1]);
        fclose(fp);
        status = 1;
        goto err;
    }

    fclose(fp);
    fp = fopen(argv[2], "r");
    if(!fp) {
        printf("Failed to open dtree\n");
        status = 1;
        goto err;
    }

    if(stat(argv[2], &st) != 0) {
        printf("Failed to get file size\n");
        fclose(fp);
        status = 1;
        goto err;
    }

    dsize = st.st_size;
    if(fread(dtree, 1, dsize, fp) <= 0) {
        printf("error reading dtree %s\n", argv[2]);
        fclose(fp);
        status = 1;
        goto err;
    }
    fclose(fp);

    for (i=0; (dev=devs[i]) != NULL; i++) {
        status = libusb_get_device_descriptor(dev, &desc);
        if ((desc.idVendor == vid)
            && (desc.idProduct == pid)) {
            break;
        }
    }

    if (dev == NULL) {
        printf("Couldn't find device\n");
        status = 1;
        goto err;
    }

    status = libusb_open(dev, &device);
    libusb_free_device_list(devs, 1);
    if (status < 0) {
        printf("libusb_open() failed: %s\n", libusb_error_name(status));
        goto err;
    }

    libusb_set_auto_detach_kernel_driver(device, 1);
    status = libusb_claim_interface(device, 0);
    if (status != LIBUSB_SUCCESS) {
        libusb_close(device);
        printf("libusb_claim_interface failed: %s\n", libusb_error_name(status));
        goto err;
    }

    status = libusb_control_transfer(device, 0x21, 2, 0, 0, 0, 0, 1000);
    if (status < 0) {
        fprintf(stderr, "libcontrol failed %d\n", status);
        goto err;
    }

    status = libusb_control_transfer(device, 0x21, 1, 0, 0, 0, 0, 1000);
    if (status < 0) {
        fprintf(stderr, "libcontrol failed %d\n", status);
        goto err;
    }

    status = libusb_bulk_transfer(device, 2, (unsigned char *)dtree, dsize, &tsize, 5000);
    if (status < 0) {
        fprintf(stderr, "Sending dtree failed with status %d\n", status);
        goto err;
    }

    status = libusb_control_transfer(device, 0x21, 3, 0, 0, (unsigned char *)DTREECMD, strlen(DTREECMD)+1, 1000);
    if (status < 0) {
        fprintf(stderr, "failed sending dtree cmd %d\n", status);
        goto err;
    }

    done = 0;
    status = libusb_control_transfer(device, 0x21, 2, 0, 0, 0, 0, 0);
    if (status < 0) {
        fprintf(stderr, "libcontrol failed %d\n", status);
        goto err;
    }

    status = libusb_control_transfer(device, 0x21, 1, 0, 0, 0, 0, 0);
    if (status < 0) {
        fprintf(stderr, "libcontrol failed %d\n", status);
        goto err;
    }

    while(done < size) {
        chunk = size - done;
        if(chunk > 1048576)
            chunk = 1048576;
        status = libusb_bulk_transfer(device, 2, (unsigned char *)kbuff + done, chunk, &tsize, 5000);
        if(status == LIBUSB_ERROR_PIPE) {
            printf("libusb_bulk_transfer failed with error %d\n", status);
            libusb_clear_halt(device, 2);
            j ++;
            if(j >= 5)
                break;
        } else {
            done += tsize;
            if(!tsize)
                break;
        }
    }

    if(done != size) {
        printf("Failed to transfer image to device\n");
        status = 1;
        goto err;
    }

    status = libusb_control_transfer(device, 0x21, 4, 0, 0, 0, 0, 0);
    if (status < 0) {
        fprintf(stderr, "libcontrol failed %d\n", status);
        goto err;
    }
    libusb_control_transfer(device, 0x21, 3, 0, 0, (unsigned char *)BOOTCMD, strlen(BOOTCMD)+1, 0);

    printf("Success!\n");

err:
    if(kbuff)
        free(kbuff);

    if(device) {
        libusb_release_interface(device, 0);
        libusb_close(device);
    }
    libusb_exit(NULL);
    return status;
}
