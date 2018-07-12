#ifndef FAT_PARSE_H
#define FAT_PARSE_H

#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#define RANGE(from, to) [to - from + 1]

typedef struct
{
    char jump_Code          RANGE(0, 2);
    char OEM_ID             RANGE(3, 10);
    char byte_Per_Sector    RANGE(11, 12);
    char sector_Per_Cluster RANGE(13, 13);
    char reserved_Sector    RANGE(14, 15);
    char num_FAT            RANGE(16, 16);
    char num_Dir_Entry      RANGE(17, 18);
    char unused1            RANGE(19, 20);
    char media_Descriptor   RANGE(21, 21);
    char unused2            RANGE(22, 23);
    char sector_Per_Track   RANGE(24, 25);
    char num_Head           RANGE(26, 27);
    char num_Hidden_Sector  RANGE(28, 31);
    char total_Sector_Count RANGE(32, 35);
    char sector_Per_Fat     RANGE(36, 39);
    char flags              RANGE(40, 41);
    char FAT_Version        RANGE(42, 43);
    char root_Dir_Cluster   RANGE(44, 47);
    char unused3            RANGE(48, 511);
} boot_Sector_FAT32;

struct directory
{
    uint32_t cluster;
    char *name;
    struct directory *next;
};

typedef struct directory DIR;

void showPath(DIR *activeDir);
void printDate(uint16_t data_Fat_Format);

/* uint32_t FAT32_Look_Up(uint32_t active_Cluster, uint16_t sector_Size); */

#endif