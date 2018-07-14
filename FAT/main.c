#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
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
char **g_LFN_Array = NULL;
uint8_t g_LFN_Length = 0;

void showCurrentState();
char showCurCluster();
uint32_t fat_Look_Up(uint32_t cluster);
void moveToCluster(uint32_t cluster);
uint32_t str_To_Uint32(char *str);
void handleCommand(uint32_t dirIndex);
void showEntryInfo(char *entry);
void showEntryName(char *entry);
void free_LFN_Array();
void UCS_2_To_UTF_8(uint16_t character);

int main(int argc, char **argv)
{
    SetConsoleOutputCP(65001);      //UTF-8
    char command[MAX_COMMAND_SIZE];
    command[0] = '\\';
    command[1] = '\\';
    command[2] = '.';
    command[3] = '\\';
    command[5] = ':';
    command[6] = 0;
    printf("Enter a drive letter (of which has been formated with FAT32): ");
    scanf("%c%*[^\n]", command + 4);
    getchar();
    g_Device = CreateFile(command,    // Drive to open
                        GENERIC_READ,           // Access mode
                        FILE_SHARE_READ | FILE_SHARE_WRITE,        // Share Mode
                        NULL,                   // Security Descriptor
                        OPEN_EXISTING,          // How to create
                        0,                      // File attributes
                        NULL);                  // Handle to template

    if(g_Device == INVALID_HANDLE_VALUE)
    {
        printf("Unable to open drive! Error code: %lu\n", GetLastError());
        getchar();
        return 1;
    }
    g_Buffer = (BYTE *) malloc(512);
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
        else if (g_Buffer[i + 11] == 0x0F)
        {
            if ((g_Buffer[i] >> 6) & 1)
            {
                g_LFN_Array = (char **) calloc( (g_Buffer[i] & 0x1F), sizeof(char *));
                g_LFN_Length = (g_Buffer[i] & 0x1F);
            }
        }
        else if (((g_Buffer[i + 11] >> 2) & 1) || ((g_Buffer[i + 11] >> 3) & 1))
        {
            free_LFN_Array();
            continue;
        }
        showEntryInfo(g_Buffer + i);
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
        Sleep(1000);
        return;
    }
    DIR *newSub = (DIR *) malloc(sizeof(DIR));
    size_t lenFolderName;
    for (lenFolderName = 0; g_Buffer[dirIndex + lenFolderName] != 32 && lenFolderName < 8; lenFolderName++){};
    newSub->name = (char *) calloc(lenFolderName + 1, 1);
    memmove(newSub->name, g_Buffer + dirIndex, lenFolderName);
    newSub->cluster = ( (*(uint16_t *)(g_Buffer + dirIndex + 20)) << 16) | (*(uint16_t *)(g_Buffer + dirIndex + 26));
    newSub->next = g_currentDir;
    g_currentDir = newSub;
    return;
}
void showEntryInfo(char *entry)
{
    const static char s_Flags[6] = "RHSVDA";
    if (entry[11] == 0x0F && g_LFN_Array != NULL)
    {
        uint8_t index = (entry[0] & 0x1F) - 1;
        g_LFN_Array[index] = (char *) malloc(26);
        memmove(g_LFN_Array[index], entry + 1, 10);
        memmove(g_LFN_Array[index] + 10, entry + 14, 12);
        memmove(g_LFN_Array[index] + 22, entry + 28, 4);
        return;
    }
    for (int j = 0; j < 6; j++)
    {
        if ((entry[11] >> j) & 1)
        {
            putchar(s_Flags[j]);
        }
        else
        {
            putchar('-');
        }
    }
    printf("     %-10u", *(uint32_t *) (entry + 28));
    printf("   ");
    printDate(*(uint16_t *)(entry + 16));
    printf("       ");
    printDate(*(uint16_t *)(entry + 24));
    printf("        ");
    showEntryName( (g_LFN_Array == NULL) ? (entry) : (NULL));
    if ((entry[11] >> 4) & 1)
    {
        printf("           -Index: %u", g_SubDirectoryNum - 1);
    }
    putchar(10);
}

void showEntryName(char *entry)
{
    if (entry == NULL)
    {
        for (uint8_t i = 0; i < g_LFN_Length; i++)
        {
            for (uint8_t j = 0; j < 26; j += 2)
            {
                if (g_LFN_Array[i][j] == 0)
                {
                    break;
                }
                UCS_2_To_UTF_8( *(uint16_t *) (g_LFN_Array[i] + j) );
            }
        }
        free_LFN_Array();
        return;
    }

    for (int j = 0; j < 8; j++)
    {
        if (entry[j] != 32)
        {
            putchar( (((entry[12] >> 3) & 1) ? (tolower(entry[j])) : entry[j]));
        }
    }
    if (!(((entry[11] >> 4) & 1) || (entry[8] + entry[9] + entry[10] == 96)))
    {
        putchar('.');
    }
    for (int j = 8; j < 11; j++)
    {
        if (entry[j] != 32)
        {
            putchar( (((entry[12] >> 4) & 1) ? (tolower(entry[j])) : entry[j]));
        }
    }
}

void free_LFN_Array()
{
    for (uint8_t i = 0; i < g_LFN_Length; i++)
    {
        free(g_LFN_Array[i]);
    }
    free(g_LFN_Array);
    g_LFN_Array = NULL;
    g_LFN_Length = 0;
}

void UCS_2_To_UTF_8(uint16_t character)
{
    if (character == 0)
    {
        return;
    }
    int8_t magnitude = 16;
    char *str = NULL;
    while (magnitude--)
    {
        if (character >> magnitude)
        {
            break;
        }
    }
    magnitude++;
    if (magnitude < 8)
    {
        str = (char *) calloc(2, 1);
        str[0] = character;
    }
    else if (magnitude < 12)
    {
        str = (char *) calloc(3, 1);
        str[0] = (0xC0 | (character >> 6));
        str[1] = (0x80 | (character & 0x3F));
    }
    else
    {
        str = (char *) calloc(4, 1);
        str[0] = (0xE0 | (character >> 12));
        str[1] = (0x80 | ((character >> 6) & 0x3F));
        str[2] = (0x80 | (character & 0x3F));
    }
    printf("%s", str);
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