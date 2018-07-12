#include "fatparse.h"

void showPath(DIR *activeDir)
{
    if (activeDir->next)
    {
        showPath(activeDir->next);
    }
    printf("%s/", activeDir->name);
}

void printDate(uint16_t data_Fat_Format)
{
    printf("%hu - %hu - %hu", (data_Fat_Format & 0x001F), ((data_Fat_Format >> 5) & 0x000F), ((data_Fat_Format >> 9) & 0x007F) + 1980);
}