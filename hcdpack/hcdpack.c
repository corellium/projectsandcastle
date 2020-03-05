#define _XOPEN_SOURCE   500

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MAX_IMAGES      128

static uint8_t *buf;
static size_t size;

static struct {
    size_t offs, size;
    char *name;
} images[MAX_IMAGES];
static unsigned nimages;

static void process_cfgd(uint8_t *buf, size_t size, char *name)
{
    size_t strsz;

    if(size < 11)
        return;
    strsz = buf[10];
    if(strsz > size - 11)
        return;

    memcpy(name, &buf[11], strsz);
    name[strsz] = 0;
}

static size_t process_image(uint8_t *buf, size_t size, char *name)
{
    size_t offs, itsz;
    uint16_t op;

    for(offs=0; offs<=size-3; ) {
        op = *(uint16_t *)&buf[offs];
        itsz = buf[offs+2];
        if(itsz > size - offs - 3)
            itsz = size - offs - 3;
        if(op != 0xFC4E && op != 0xFC4C)
            break;
        if(offs > 0 && offs <= size - 15 && buf[offs] == 0x4C && buf[offs+1] == 0xFC && !memcmp(&buf[offs+7], "BRCMcfgS", 8))
            break;
        if(offs <= size - 15 && buf[offs] == 0x4C && buf[offs+1] == 0xFC && !memcmp(&buf[offs+7], "BRCMcfgD", 8))
            process_cfgd(&buf[offs+15], itsz - 12, name);
        offs += itsz + 3;
        if(op == 0xFC4E)
            break;
    }

    return offs;
}

static void collect_images(int print)
{
    size_t offs, imsz;
    char name[256];

    for(offs=0; offs<=size-15; offs++) {
        if(buf[offs] != 0x4C || buf[offs+1] != 0xFC)
            continue;
        if(memcmp(&buf[offs+7], "BRCMcfgS", 8))
            continue;
        name[0] = 0;
        imsz = process_image(buf + offs, size - offs, name);
        if(print)
            printf("Found sig at %ld (%ld): %s\n", offs, imsz, name);
        if(nimages < MAX_IMAGES) {
            images[nimages].offs = offs;
            images[nimages].size = imsz;
            images[nimages].name = strdup(name);
            nimages ++;
        }
        offs += imsz - 1;
    }
}

static void get_tag(const char *str, char id, char *out, size_t outlen)
{
    const char *n;
    size_t l;

    out[0] = 0;
    while(*str) {
        if(str[0] == '_') {
            str ++;
            continue;
        }
        if(str[1] != '-')
            return;
        n = strchr(str + 2, '_');
        if(!n)
            n = str + strlen(str);
        if(str[0] == id) {
            str += 2;
            l = n - str;
            if(l + 1 > outlen)
                l = outlen - 1;
            memcpy(out, str, l);
            out[l] = 0;
            return;
        }
        str = n;
    }
}

int main(int argc, char *argv[])
{
    FILE *f;
    char chip[8], rev[4], mod[32], vend[4], *vendstr;
    char chipstr[64], modstr[64];
    unsigned i;

    if(argc != 2 && argc != 6) {
        fprintf(stderr, "usage: autohcd <hcd-pack>\n");
        fprintf(stderr, "       autohcd <hcd-pack> <otp-chip> <module> <otp-nvram> <outfile>\n");
        return 1;
    }

    f = fopen(argv[1], "rb");
    if(!f) {
        fprintf(stderr, "error: failed opening '%s'.\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(size);
    fread(buf, 1, size, f);
    fclose(f);

    collect_images(argc == 2);
    if(argc < 6)
        return 0;

    get_tag(argv[2], 'C', chip, sizeof(chip));
    get_tag(argv[2], 's', rev, sizeof(rev));
    strcpy(mod, argv[3]);
    mod[0] = toupper(mod[0]);
    get_tag(argv[4], 'V', vend, sizeof(vend));
    switch(tolower(vend[0])) {
    case 'm': vendstr = "MUR"; break;
    case 'u': vendstr = "USI"; break;
    case 't': vendstr = "TDK"; break;
    default: vendstr = "UNK";
    }
    sprintf(chipstr, "BCM%s%s ", chip, rev);
    sprintf(modstr, " %s %s", mod, vendstr);

    for(i=0; i<nimages; i++) {
        if(strncmp(images[i].name, chipstr, strlen(chipstr)))
            continue;
        if(strstr(images[i].name, modstr))
            break;
    }
    if(i >= nimages) {
        fprintf(stderr, "Image for %s/%s not found.\n", chipstr, modstr);
        return 1;
    }

    f = fopen(argv[5], "wb");
    if(!f) {
        fprintf(stderr, "error: failed opening '%s' for write.\n", argv[5]);
        return 1;
    }
    fwrite(buf + images[i].offs, images[i].size, 1, f);
    fclose(f);

    printf("BCM%s%s.hcd", chip, rev);

    return 0;
}
