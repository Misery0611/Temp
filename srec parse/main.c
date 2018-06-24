/*  __Misery__
    SREC Motorola Testing
    Support a 78-byte limit on total record length and 64-byte limit on data length.
*/
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TRUE 1
#define FALSE 0

/* Enum of analysis result */
typedef enum
{
    UNDEFINED = -1,
    OK = 0,
    UNACCEPTABLE_CHARACTER,
    LINE_NOT_STARTING_WITH_S,
    INVALID_RECORD_TYPE,
    INVALID_LINE_LENGTH,
    COUNT_INCORRECT,
    /*DATA_LIMIT_EXCEEDED,*/
    CHECKSUM_INCORRECT,
    S0_ADDRESS_NONZERO,
    LINE_COUNT_MISMATCH,
} parseID;

struct range
{
    uint8_t min, max;
};

parseID analyzeRecord(char *record);
void handleError(parseID result);
uint8_t hexPairToDec(char first, char second);

int main(void)
{
    char SrecFileName[50], curSrec[80];
    FILE *SrecDoc = NULL;
    uint32_t lineIndex = 0;
    parseID curParseResult;
    
    /* Gets the file name and open it */
    printf("Enter the SREC file name (extension must be included): ");
    fgets(SrecFileName, 50, stdin);
    SrecFileName[strlen(SrecFileName) - 1] = 0;
    SrecDoc = (FILE *) fopen(SrecFileName, "r");

    /* Check if manage to open file or not */
    if (SrecDoc == NULL)
    {
        printf("Unable to open Srec File!\n");
        getchar();
        return 0;
    }

    /* Handle S-records one by one */
    while (TRUE)
    {
        /* Increase the line index
           "Clear" old Srec data, gets new one
           Reset parse result */
        curParseResult = UNDEFINED;
        curSrec[0] = 0;
        lineIndex++;
        fgets(curSrec, 80, SrecDoc);

        /* Check if it is the EOF or not */
        if (curSrec[strlen(curSrec) - 1] != 10)
        {
            fscanf(SrecDoc, "%*[^\n]");
            if (fgetc(SrecDoc) == EOF)
            {
                if (!feof(SrecDoc))
                {
                    printf("Error while reading file!\n");
                }
                break;
            }
        }

        curParseResult = analyzeRecord(curSrec);

        /* Show the parse result */
        printf("Line %lu: ", lineIndex);
        handleError(curParseResult);
    }

    //Close the file when done
    fclose(SrecDoc);
    SrecDoc = NULL;
    printf("Finished!\n");
    getchar();
    return 0;
}

parseID analyzeRecord(char *record)
{
    static range recordLengthLimit[10] = {{10, 70}, {12, 74}, {14, 76}, {16, 78}, {0, 0}, {10, 10}, {0, 0}, {14, 14}, {12, 12}, {10, 10}};
    static uint16_t lineCount = 0;
    size_t recordLen = strlen(record);
    uint32_t temp;
    /* Increase the S123 count one unit */
    if (record[0] == 'S' && record[1] > '0' && record[1] < '4')
    {
        lineCount++;
    }
    /* Check for Record length limit */
    if (recordLen > 79 || (recordLen == 79 && record[78] != 10))
    {
        return INVALID_LINE_LENGTH;
    }
    /* Remove the LF at the end of the record if exist */
    if (record[recordLen - 1] == 10)
    {
        record[--recordLen] = 0;
    }
    /* Check whether the record starts with S or not */
    if (record[0] != 'S')
    {
        return LINE_NOT_STARTING_WITH_S;
    }
    /* Detect if there is invalid character */
    for (size_t i = 1; record[i]; i++)
    {
        if (record[i] < 48 || record[i] > 90 || (record[i] > 57 && record[i] < 65))
        {
            return UNACCEPTABLE_CHARACTER;
        }
    }
    /* Check if type is supported or not? */
    if (record[1] > 57 || record[1] == '6' || record[1] == '4')
    {
        return INVALID_RECORD_TYPE;
    }
    /* Invalid line length? */
    if (recordLen % 2 || recordLen < recordLengthLimit[record[1] - 48].min || recordLen > recordLengthLimit[record[1] - 48].max)
    {
        return INVALID_LINE_LENGTH;
    }
    /* Is count field correct? */
    if (2 * hexPairToDec(record[2], record[3]) != recordLen - 4)
    {
        return COUNT_INCORRECT;
    }
    /* Check sum is correct, isn't it? */
    temp = 0;
    for (size_t i = 2; i < recordLen - 2; i += 2)
    {
        temp += hexPairToDec(record[i], record[i + 1]);
    }
    if (hexPairToDec(record[recordLen - 2], record[recordLen - 1]) != (0xFF - (temp & 0xFF)))
    {
        return CHECKSUM_INCORRECT;
    }
    /* S0 records' address field must be filled with zero */
    if (record[1] == '0' && (record[4] + record[5] + record[6] + record[7] != 192))
    {
        return S0_ADDRESS_NONZERO;
    }
    /* S5 records' value is right? */
    if (record[1] == '5')
    {
        temp = (hexPairToDec(record[4], record[5]) << 8);
        temp |= hexPairToDec(record[6], record[7]);
        if (temp != lineCount)
        {
            return LINE_COUNT_MISMATCH;
        }
    }
    return OK;
}

void handleError(parseID result)
{
    switch (result)
    {
        case OK:
            printf("OK!\n");
            break;
        case UNACCEPTABLE_CHARACTER:
            printf("Record contains invalid character!\n");
            break;
        case LINE_NOT_STARTING_WITH_S:
            printf("Record is not starting with S!\n");
            break;
        case INVALID_RECORD_TYPE:
            printf("Unknown Record Type! (Supporting Types: S0, S1, S2, S3, S5, S7, S8, S9)\n");
            break;
        case INVALID_LINE_LENGTH:
            printf("Line has an invalid length!\n");
            break;
        case COUNT_INCORRECT:
            printf("Value of the count field mismatches the length of remaining data.\n");
            break;
        /*case DATA_LIMIT_EXCEEDED:
            printf("The data field's length exceeded the limit!\n");
            break;*/
        case CHECKSUM_INCORRECT:
            printf("Checksum value is incorrect!\n");
            break;
        case S0_ADDRESS_NONZERO:
            printf("The address field of S0 Record is not filled with zero!\n");
            break;
        case LINE_COUNT_MISMATCH:
            printf("Transmitted lines counts conflicted! (S5 type)\n");
            break;
        default:
            printf("Undefined Error!\n");
    }
}

uint8_t hexPairToDec(char first, char second)
{
    (first > 57) ? (first -= 55) : (first -= 48);
    (second > 57) ? (second -= 55) : (second -= 48);
    return ((first << 4) | second);
}