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
    CHECKSUM_INCORRECT,
    S0_ADDRESS_NONZERO,
    LINE_COUNT_MISMATCH,
} parseID;

/* Struct store 2 value refer limits of an object */
struct range
{
    uint8_t min, max;
};

FILE *reportFile = NULL;    /* Pointer to the output file */

/***************************************************************************************************
 * PROTOTYPE
 **************************************************************************************************/
/**
 * @brief [Function to analyze a record]
 * 
 * @param record [Pointer to the record string]
 * @return [The an ID, which indicates analysis result]
 */
parseID analyzeRecord(char *record);

/**
 * @brief [Show the result in text]
 * 
 * @param result [The error code]
 * @return [nothing]
 */
void exportResult(parseID result);

/**
 * @brief [Convert a pair of Hexa character to one byte value]
 * 
 * @param first [First Hexa character]
 * @param second [Second Hexa character]
 * 
 * @return [The value in decimal]
 */
uint8_t hexPairToDec(char first, char second);

/***************************************************************************************************
 * CODE
 **************************************************************************************************/
int main(void)
{
    char SrecFileName[50], curSrec[80];    /* Path of the SREC file and current record data */
    FILE *SrecDoc = NULL;    /* Pointer to the SREC file data*/
    uint32_t lineIndex = 0;    /* Variable denote the index of current line */
    parseID curParseResult;    /* Current record's parse result */

    /* Gets the file name and open it */
    printf("Enter the SREC file name (extension must be included): ");
    fgets(SrecFileName, 50, stdin);
    /* Remove the LF at the end of the input data */
    SrecFileName[strlen(SrecFileName) - 1] = 0;
    SrecDoc = (FILE *) fopen(SrecFileName, "r");
    /* Open (create if not exist) the report file */
    reportFile = fopen("report.txt", "a+");

    /* Check if manage to open file or not */
    if (SrecDoc == NULL)
    {
        printf("Unable to open Srec File!\n");
        getchar();
        return 0;
    }
    /* Ensure opening report file is successful */
    if (reportFile == NULL)
    {
        printf("Faild to load report file!\n");
        getchar();
        return 0;
    }
    /* Clarify report file */
    fprintf(reportFile, "%s\n", SrecFileName);
    printf("Processing!\n");
    /* Handle S-records one by one */
    while (TRUE)
    {
        curParseResult = UNDEFINED;    /* Reset the parse result */
        curSrec[0] = 0;    /* "Clear" old data */
        lineIndex++;    /* Increase the index by one unit */
        fgets(curSrec, 80, SrecDoc);    /* Get new data*/

        /* Check if it is the EOF or not */
        if (curSrec[strlen(curSrec) - 1] != 10)
        {
            /* Ignore everything left of the line */
            fscanf(SrecDoc, "%*[^\n]");
            if (fgetc(SrecDoc) == EOF)
            {
                /* If not end of file then it's an reading error */
                if (!feof(SrecDoc))
                {
                    fprintf(reportFile, "Error while reading file!\n");
                }
                break;
            }
        }

        /* Analyze new record*/
        curParseResult = analyzeRecord(curSrec);

        /* Show the parse result */
        fprintf(reportFile, "Line %lu: ", lineIndex);
        exportResult(curParseResult);
    }

    /* Show ending sign */
    printf("Finished!\n");
    fprintf(reportFile, "Done!\n");
    /* Close the file when done */
    fclose(SrecDoc);
    SrecDoc = NULL;
    fclose(reportFile);
    reportFile = NULL;
    /* Wait for a respond */
    getchar();
    return 0;
}

parseID analyzeRecord(char *record)
{
    static const range s_RecordLengthLimit[10] = {{10, 74}, {12, 74}, {14, 76}, {16, 78}, {0, 0}, {10, 10}, {0, 0}, {14, 14}, {12, 12}, {10, 10}};
    static uint16_t s_LineCount = 0;
    size_t recordLen = strlen(record);
    uint32_t temp;
    /* Increase the S123 count one unit */
    if (record[0] == 'S' && record[1] > '0' && record[1] < '4')
    {
        s_LineCount++;
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
        if (record[i] < 48 || record[i] > 70 || (record[i] > 57 && record[i] < 65))
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
    if (recordLen % 2 || recordLen < s_RecordLengthLimit[record[1] - 48].min || recordLen > s_RecordLengthLimit[record[1] - 48].max)
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
    /* Calculate the sum */
    for (size_t i = 2; i < recordLen - 2; i += 2)
    {
        /* Add each pair value to temp */
        temp += hexPairToDec(record[i], record[i + 1]);
    }
    /* Compare the checksum data */
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
        if (temp != s_LineCount)
        {
            return LINE_COUNT_MISMATCH;
        }
    }
    return OK;
}

void exportResult(parseID result)
{
    switch (result)
    {
        case OK:
            fprintf(reportFile, "OK!\n");
            break;
        case UNACCEPTABLE_CHARACTER:
            fprintf(reportFile, "Record contains invalid character!\n");
            break;
        case LINE_NOT_STARTING_WITH_S:
            fprintf(reportFile, "Record is not starting with S!\n");
            break;
        case INVALID_RECORD_TYPE:
            fprintf(reportFile, "Unknown Record Type! (Supporting Types: S0, S1, S2, S3, S5, S7, S8, S9)\n");
            break;
        case INVALID_LINE_LENGTH:
            fprintf(reportFile, "Line has an invalid length!\n");
            break;
        case COUNT_INCORRECT:
            fprintf(reportFile, "Value of the count field mismatches the length of remaining data.\n");
            break;
        case CHECKSUM_INCORRECT:
            fprintf(reportFile, "Checksum value is incorrect!\n");
            break;
        case S0_ADDRESS_NONZERO:
            fprintf(reportFile, "The address field of S0 Record is not filled with zero!\n");
            break;
        case LINE_COUNT_MISMATCH:
            fprintf(reportFile, "Transmitted lines counts conflicted! (S5 type)\n");
            break;
        default:
            fprintf(reportFile, "Undefined Error!\n");
    }
}

uint8_t hexPairToDec(char first, char second)
{
    (first > 57) ? (first -= 55) : (first -= 48);
    (second > 57) ? (second -= 55) : (second -= 48);
    return ((first << 4) | second);
}