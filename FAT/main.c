#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <windows.h>
#include "fatparse.h"

#define MAX_COMMAND_SIZE 12
#define FAT_TABLE_SECTOR (*(uint16_t *) g_Boot_Sector.reserved_Sector)
#define SECTOR_PER_FAT (*(uint32_t *)g_Boot_Sector.sector_Per_Fat)
#define NUM_FAT (*(uint8_t *)g_Boot_Sector.num_FAT)
#define CLUSTER_SIZE (*(uint16_t *)g_Boot_Sector.byte_Per_Sector * *(uint8_t *) g_Boot_Sector.sector_Per_Cluster)
#define SECTOR_PER_CLUSTER (*(uint8_t *)g_Boot_Sector.sector_Per_Cluster)
#define SECTOR_SIZE (*(uint16_t *)g_Boot_Sector.byte_Per_Sector)

boot_Sector_FAT32 g_Boot_Sector;
DIR *g_currentDir = NULL;
BYTE *g_Buffer = NULL;
DWORD bytesRead;
HANDLE g_Device = NULL;
uint32_t g_SubDirectoryNum = 0;

void showCurrentState();
char showCurCluster();
uint32_t fat_Look_Up(uint32_t cluster);
void moveToCluster(uint32_t cluster);
uint32_t str_To_Uint32(char *str);
void handleCommand(uint32_t dirIndex);

int main(int argc, char **argv)
{
    char command[MAX_COMMAND_SIZE];
    g_Buffer = (BYTE *) malloc(512);
    g_Device = CreateFile("\\\\.\\J:",    // Drive to open
                        GENERIC_READ,           // Access mode
                        FILE_SHARE_READ | FILE_SHARE_WRITE,        // Share Mode
                        NULL,                   // Security Descriptor
                        OPEN_EXISTING,          // How to create
                        0,                      // File attributes
                        NULL);                  // Handle to template

    if(g_Device == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile: %lu\n", GetLastError());
        getchar();
        return 1;
    }
    SetFilePointer (g_Device, 0, NULL, FILE_BEGIN);
    ReadFile(g_Device, g_Buffer, 512, &bytesRead, NULL);
    memmove(&g_Boot_Sector, g_Buffer, 512);
    g_Buffer = (BYTE *) realloc(g_Buffer, CLUSTER_SIZE);

    g_currentDir = (DIR *) calloc(1, sizeof(DIR));
    g_currentDir->cluster = *(uint32_t *) g_Boot_Sector.root_Dir_Cluster;
    g_currentDir->name = (char *) malloc(1);
    g_currentDir->name[0] = 0;
    command[0] = 0;

    while (command[0] != '?')
    {
        showCurrentState();
        fgets(command, MAX_COMMAND_SIZE, stdin);
        if (command[0] != '?')
        {
            handleCommand(str_To_Uint32(command));
        }
        system("cls");
        g_SubDirectoryNum = 0;
    }
    // printf("OEM: %s\n", g_Boot_Sector.OEM_ID);
    // printf("Bytes per sector: %hu\n", *(uint16_t *)g_Boot_Sector.byte_Per_Sector);
    // printf("Sectors per cluster: %hu\n", *(uint8_t *)g_Boot_Sector.sector_Per_Cluster * 1);
    // printf("Reserved sectors: %hu\n", *(uint16_t *)g_Boot_Sector.reserved_Sector);
    // getchar();
    CloseHandle(g_Device);
    return 0;
}

void showCurrentState()
{
    printf("Path: ");
    showPath(g_currentDir);
    putchar(10);
    printf("\nFlags description: Read Only, Hidden, System, Volume label, Directory, Archive.\n\n");
    printf("Flags        Size       Creation date      Last modified date      Name\n");
    moveToCluster(g_currentDir->cluster);
    ReadFile(g_Device, g_Buffer, CLUSTER_SIZE, &bytesRead, NULL);
    while (!showCurCluster())
    {
        uint32_t lookUpResult = fat_Look_Up(g_currentDir->cluster);
        if (lookUpResult == 0)
        {
            break;
        }
        else if (lookUpResult == 1)
        {
            printf("Bad cluster detected!\n");
            break;
        }
        moveToCluster(lookUpResult);
        ReadFile(g_Device, g_Buffer, CLUSTER_SIZE, &bytesRead, NULL);
    }
}

char showCurCluster()
{
    const static char s_Flags[6] = "RHSVDA";
    static char s_LFN_Check = 0;
    int i;
    for (i = 0; i < CLUSTER_SIZE; i += 32)
    {
        if (g_Buffer[i] == 0)
        {
            break;
        }
        g_SubDirectoryNum++;
        if (g_Buffer[i] == 0xE5 || (g_Buffer[i] == '.' && (g_Buffer[i + 1] == '.' || g_Buffer[i + 1] == ' ')))
        {
            continue;
        }
        else if (((g_Buffer[i + 11] >> 2) & 1) || ((g_Buffer[i + 11] >> 3) & 1))
        {
            continue;
        }
        else if (g_Buffer[i + 11] == 0x0F)
        {
            continue;
        }                                        
        for (int j = 0; j < 6; j++)
        {
            if ((g_Buffer[i + 11] >> j) & 1)
            {
                putchar(s_Flags[j]);
            }
            else
            {
                putchar('-');
            }
        }
        printf("     %-10u", *(uint32_t *) (g_Buffer + i + 28));
        printf("   ");
        printDate(*(uint16_t *)(g_Buffer + i + 16));
        printf("        ");
        printDate(*(uint16_t *)(g_Buffer + i + 24));
        printf("         ");
        for (int j = 0; j < 8; j++)
        {
            if (g_Buffer[i + j] != 32)
            {
                putchar(g_Buffer[i + j]);
            }
        }
        if (!((g_Buffer[i + 11] >> 4) & 1))
        {
            putchar('.');
        }
        for (int j = 8; j < 11; j++)
        {
            if (g_Buffer[i + j] != 32)
            {
                putchar(g_Buffer[i + j]);
            }
        }
        if ((g_Buffer[i + 11] >> 4) & 1)
        {
            printf("           -Index: %u", g_SubDirectoryNum - 1);
        }
        putchar(10);
    }
    return (i < CLUSTER_SIZE);
}

uint32_t fat_Look_Up(uint32_t activeCluster)
{
    uint32_t result = 0;
    activeCluster *= 4;
    SetFilePointer (g_Device, (FAT_TABLE_SECTOR + (activeCluster /SECTOR_SIZE)) * SECTOR_SIZE, NULL, FILE_BEGIN);
    ReadFile(g_Device, g_Buffer, SECTOR_SIZE, &bytesRead, NULL);
    result = *(uint32_t *) (g_Buffer + activeCluster % SECTOR_SIZE);
    result &= 0x0FFFFFFF;
    if (result >= 0x0FFFFFF8)
    {
        return 0;
    }
    else if (result == 0x0FFFFFF7)
    {
        return 1;
    }
    else
    {
        return result;
    }
}

void moveToCluster(uint32_t cluster)
{
    if (cluster < 2)
    {
        return;
    }
    SetFilePointer (g_Device, ((cluster - 2) * SECTOR_PER_CLUSTER + FAT_TABLE_SECTOR + NUM_FAT * SECTOR_PER_FAT) * SECTOR_SIZE, NULL, FILE_BEGIN);
}

uint32_t str_To_Uint32(char *str)
{
    uint32_t result = 0;
    for (int i = 0; str[i]; i++)
    {
        if (str[i] == 10)
        {
            break;
        }
        if (str[i] < '0' || str[i] > '9')
        {
            return 0;
        }
        result = (result * 10 + str[i] - 48);
    }
    return result;
}

void handleCommand(uint32_t dirIndex)
{
    if (dirIndex == 0)
    {
        if (g_currentDir->next == NULL)
        {
            return;
        }
        DIR *temp = g_currentDir;
        g_currentDir = g_currentDir->next;
        free(temp);
        return;
    }
    if (dirIndex >= g_SubDirectoryNum)
    {
        printf("Invalid index!\n");
        return;
    }
    dirIndex *= 32;
    moveToCluster(g_currentDir->cluster + dirIndex / CLUSTER_SIZE);
    ReadFile(g_Device, g_Buffer, CLUSTER_SIZE, &bytesRead, NULL);
    if (!((g_Buffer[dirIndex + 11] >> 4) & 1) || (g_Buffer[dirIndex] == '.' && g_Buffer[dirIndex + 1] == '.'))
    {
        printf("Invalid index!\n");
        return;
    }
    DIR *newSub = (DIR *) malloc(sizeof(DIR));
    size_t lenFolderName;
    for (lenFolderName = 0; g_Buffer[dirIndex + lenFolderName] != 32 && lenFolderName < 8; lenFolderName++){};
    newSub->name = (char *) calloc(lenFolderName + 1, 1);
    memmove(newSub->name, g_Buffer + dirIndex, lenFolderName);
    newSub->cluster = ((*(uint16_t *) (g_Buffer + dirIndex + 20) )<< 16) | (*(uint16_t *) (g_Buffer + dirIndex + 26));
    newSub->next = g_currentDir;
    g_currentDir = newSub;
    return;
}
// typedef struct
// {
//     char jump_Code          RANGE(0, 2);
//     char OEM_ID             RANGE(3, 10);
//     char byte_Per_Sector    RANGE(11, 12);
//     char sector_Per_Cluster RANGE(13, 13);
//     char reserved_Sector    RANGE(14, 15);
//     char num_FAT            RANGE(16, 16);
//     char num_Dir_Entry      RANGE(17, 18);
//     char unused1            RANGE(19, 20);
//     char media_Descriptor   RANGE(21, 21);
//     char unused2            RANGE(22, 23);
//     char sector_Per_Track   RANGE(24, 25);
//     char num_Head           RANGE(26, 27);
//     char num_Hidden_Sector  RANGE(28, 31);
//     char total_Sector_Count RANGE(32, 35);
//     char sector_Per_Fat     RANGE(36, 39);
//     char flags              RANGE(40, 41);
//     char FAT_Version        RANGE(42, 43);
//     char root_Dir_Cluster   RANGE(44, 47);
//     char unused3            RANGE(48, 511);
// } boot_Sector_FAT32;