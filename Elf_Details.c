#include "Elf_Details.h"
#include "Harklehash.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define SUPER_STR_ME(str) #str
// #define EXTRA_STR_ME(str) SUPER_STR_ME(str)
// #define STR_ME(str) EXTRA_STR_ME(str)

// Purpose: Open and parse an ELF file.  Allocate, configure and return Elf_Details pointer.
// Input:	Filename, relative or absolute, to an ELF file
// Output:	A dynamically allocated Elf_Details struct that contains information about elvenFilename
// Note:	
//			It is caller's responsibility to free the return value from this function (and all char* within)
//			This function does all the prep work.  The actual parsing work is done by parse_elf()
struct Elf_Details* read_elf(char* elvenFilename)
{
	/* LOCAL VARIABLES */
	struct Elf_Details* retVal = NULL;	// Struct to be allocated, initialized and returned
	FILE* elfFile = NULL;				// File pointer of elvenFilename
	size_t elfSize = 0;					// Size of the file in bytes
	char* elfGuts = NULL;				// Holds contents of binary file
	char* tmpPtr = NULL;				// Holds return value from strstr()/strncpy()
	int tmpRetVal = 0;					// Holds return value from parse_elf()

	/* INPUT VALIDATION */
	if (!elvenFilename)
	{
		PERROR(errno);
		return retVal;
	}

	/* READ ELF FILE */
	// OPEN FILE
	elfFile = fopen(elvenFilename, "rb");

	if (elfFile)
	{
		// GET FILE SIZE
		elfSize = file_len(elfFile);
		PERROR(errno);  // DEBUGGING

		// ALLOCATE BUFFER
		elfGuts = (char*)gimme_mem(elfSize + 1, sizeof(char));
		PERROR(errno);  // DEBUGGING

		if (elfGuts)
		{
			// READ FILE
			if (fread(elfGuts, sizeof(char), elfSize, elfFile) != elfSize)
			{
				PERROR(errno);  // DEBUGGING
				take_mem_back((void**)&elfGuts, strlen(elfGuts), sizeof(char));
				// memset(elfGuts, 0, elfSize);
				// free(elfGuts);
				// elfGuts = NULL;
				fclose(elfFile);
				elfFile = NULL;
				return retVal;
			}
			else
			{
#ifdef DEBUGLEROAD
				print_it(elfGuts, elfSize);  // DEBUGGING
#endif // DEBUGLEROAD
			}			
		}
	}
	else
	{
		PERROR(errno);  // DEBUGGING
		return retVal;
	}

	/* ALLOCATE STRUCT MEMORY */
	retVal = (struct Elf_Details*)gimme_mem(1, sizeof(struct Elf_Details));
	if (!retVal)
	{
		PERROR(errno);
		take_mem_back((void**)&elfGuts, elfSize, sizeof(char));
		// memset(elfGuts, 0, elfSize);
		// free(elfGuts);
		// elfGuts = NULL;
		fclose(elfFile);
		elfFile = NULL;
		return retVal;
	}

	/* CLOSE ELF FILE */
	if (elfFile)
	{
		fclose(elfFile);
		elfFile = NULL;
	}

	/* PARSE ELF GUTS INTO STRUCT */
	// Allocate Filename
	retVal->fileName = gimme_mem(strlen(elvenFilename) + 1, sizeof(char));
	// Copy Filename
	if (retVal->fileName)
	{
		tmpPtr = strncpy(retVal->fileName, elvenFilename, strlen(elvenFilename));
		if(tmpPtr != retVal->fileName)
		{
			PERROR(errno);
			fprintf(stderr, "ERROR: strncpy of filename into Elf_Details struct failed!\n");
		}
		else
		{
#ifdef DEBUGLEROAD
			puts(retVal->fileName);  // DEBUGGING
#endif // DEBUGLEROAD
		}
	}
	// Initialize Remaining Struct Members
	tmpRetVal = parse_elf(retVal, elfGuts);

	/* FINAL CLEAN UP */
	if (elfGuts)
	{
		take_mem_back((void**)&elfGuts, elfSize + 1, sizeof(char));
		// memset(elfGuts, 0, elfSize);
		// free(elfGuts);
		// elfGuts = NULL;
	}

	return retVal;
}


// Purpse:	Parse an ELF file contents into an Elf_Details struct
// Input:
//			elven_struct - Struct to store elven details
//			elven_contents - ELF file contents
// Output:	ERROR_* as specified in Elf_Details.h
int parse_elf(struct Elf_Details* elven_struct, char* elven_contents)
{
	/* LOCAL VARIABLES */
	int retVal = ERROR_SUCCESS;	// parse_elf() return value
	char* tmpPtr = NULL;		// Holds return values from string functions
	int tmpInt = 0;				// Holds various temporary return values
	int dataOffset = 0;			// Used to offset into elven_contents
	char* tmpBuff = NULL;		// Temporary buffer used to assist in slicing up elven_contents
	struct HarkleDict* elfHdrClassDict = NULL;
	struct HarkleDict* elfHdrEndianDict = NULL;
	struct HarkleDict* tmpNode = NULL;	// Holds return values from lookup_* functions

	/* INPUT VALIDATION */
	if (!elven_struct || !elven_contents)
	{
		retVal = ERROR_NULL_PTR;
		return retVal;
	}
	else if (strlen(elven_contents) == 0)
	{
		retVal = ERROR_ORC_FILE;
		return retVal;
	}

	/* PARSE ELF FILE CONTENTS */
	// 1. Find the beginning of the ELF Header
	tmpPtr = strstr(elven_contents, ELF_H_MAGIC_NUM);
	// printf("elven_contents:\t%p\nMagic Num:\t%p\n", elven_contents, tmpPtr);  // DEBUGGING
	if (tmpPtr != elven_contents)
	{
		PERROR(errno);
		fprintf(stderr, "This is not an ELF formatted file.\nStart:\t%p\n%s:\t%p\n", elven_contents, ELF_H_MAGIC_NUM, tmpPtr);
		retVal = ERROR_ORC_FILE;
		return retVal;
	}

	/* PREPARE DYNAMICALLY ALLOCATED VARIABLES */
	// Peformed here to avoid memory leak if elven_contents turns out to be an ORC File
	tmpBuff = gimme_mem(strlen(elven_contents) + 1, sizeof(char));
	elfHdrClassDict = init_elf_header_class_dict();
	elfHdrEndianDict = init_elf_header_endian_dict();

	// 2. Begin initializing the struct
	// 2.1. Filename should already be initialized in calling function
	// 2.2. ELF Class
	dataOffset = 4;
	// fprintf(stdout, "ELF Class:\t%d\n", (*(elven_contents + dataOffset)));  // DEBUGGING
	// Unecessary?
	// strncpy(tmpBuff, elven_contents + dataOffset, 1);  // Copy one byte from the data offset to tmpBuff
	// fprintf(stdout, "tmpBuff now holds:\t%c (%d)\n", *tmpBuff, *tmpBuff);  // DEBUGGING
	// tmpInt = atoi(tmpBuff);  // THIS DOESN'T WORK!
	// tmpInt = (int)tmpBuff[0];  // Better way to do this?
	tmpInt = (int)(*(elven_contents + dataOffset));
	// fprintf(stdout, "tmpInt now holds:\t%d\n", tmpInt);  // DEBUGGING
	tmpNode = lookup_value(elfHdrClassDict, tmpInt);
	if (tmpNode)  // Found it
	{
		// fprintf(stdout, "tmpNode->name:\t%s\n", tmpNode->name);  // DEBUGGING
		elven_struct->elfClass = gimme_mem(strlen(tmpNode->name) + 1, sizeof(char));
		if (elven_struct->elfClass)
		{
			if (strncpy(elven_struct->elfClass, tmpNode->name, strlen(tmpNode->name)) != elven_struct->elfClass)
			{
				fprintf(stderr, "ELF Class string '%s' not copied into ELF Struct!\n", tmpNode->name);
			}
			else
			{
#ifdef DEBUGLEROAD
				fprintf(stdout, "Successfully copied '%s' into ELF Struct!\n", elven_struct->elfClass);
#endif // DEBUGLEROAD
			}
		} 
	}
	else
	{
		fprintf(stderr, "ELF Class %d not found in HarkleDict!\n", tmpInt);
	}
	// 2.3. Endianess
	dataOffset += 1;
	tmpInt = (int)(*(elven_contents + dataOffset));
	// fprintf(stdout, "tmpInt now holds:\t%d\n", tmpInt);  // DEBUGGING
	tmpNode = lookup_value(elfHdrEndianDict, tmpInt);
	if (tmpNode)  // Found it
	{
		// fprintf(stdout, "tmpNode->name:\t%s\n", tmpNode->name);  // DEBUGGING
		elven_struct->endianess = gimme_mem(strlen(tmpNode->name) + 1, sizeof(char));
		if (elven_struct->endianess)
		{
			if (strncpy(elven_struct->endianess, tmpNode->name, strlen(tmpNode->name)) != elven_struct->endianess)
			{
				fprintf(stderr, "ELF Endianess string '%s' not copied into ELF Struct!\n", tmpNode->name);
			}
			else
			{
#ifdef DEBUGLEROAD
				fprintf(stdout, "Successfully copied '%s' into ELF Struct!\n", elven_struct->endianess);
#endif // DEBUGLEROAD
			}
		} 
	}
	else
	{
		fprintf(stderr, "ELF Endianess %d not found in HarkleDict!\n", tmpInt);
	}
	// 2.4. Version
	// IMPLEMENT NOW!
	// 2.5. Target OS
	// IMPLEMENT NOW!
	// 2.6. ABI Version
	// IMPLEMENT NOW!
	// 2.7. Type
	// Implement this later

	/* CLEAN UP */
	// Zeroize/Free/NULLify tempBuff
	if (tmpBuff)
	{
		take_mem_back((void**)&tmpBuff, strlen(elven_contents) + 1, sizeof(char));
	}
	// Zeroize/Free/NULLify elfHdrClassDict
	if (elfHdrClassDict)
	{
		tmpInt = destroy_a_list(&elfHdrClassDict);
	}
	// Zeroize/Free/NULLify elfHdrEndianDict
	if (elfHdrEndianDict)
	{
		tmpInt = destroy_a_list(&elfHdrEndianDict);
	}

	return retVal;
}

// Purpose:	Print human-readable details about an ELF file
// Input:
//			elven_file - A Elf_Details struct that contains data about an ELF file
//			sectionsToPrint - Bitwise AND the "PRINT_*" macros into this variable
//				to control what the function actually prints.
//			stream - A stream to send the information to (e.g., stdout, A file)
// Output:	None
// Note:	This function will print the relevant data from elven_file into stream
//				based on the flags found in sectionsToPrint
void print_elf_details(struct Elf_Details* elven_file, unsigned int sectionsToPrint, FILE* stream)
{
	/* LOCAL VARIABLES */
	const char notConfigured[] = { "¡NOT CONFIGURED!"};

	/* INPUT VALIDATION */
	if (!stream)
	{
		fprintf(stderr, "ERROR: FILE* stream was NULL!\n");
		return;
	}
	else if (!elven_file)
	{
		fprintf(stream, "ERROR: struct Elf_Details* elven_file was NULL!\n");
		return;
	}
	else if ((sectionsToPrint >> 6) > 0)
	{
		fprintf(stream, "ERROR: Invalid flags found in sectionsToPrint!\n");
		return;
	}

	fprintf(stream, "\n\n");

	/* ELF HEADER */
	if (sectionsToPrint & PRINT_ELF_HEADER || sectionsToPrint & PRINT_EVERYTHING)
	{
		// Header
		print_fancy_header(stream, "ELF HEADER", HEADER_DELIM);
		// Filename
		if (elven_file->fileName)
		{
			fprintf(stream, "Filename:\t%s\n", elven_file->fileName);
		}
		else
		{
			fprintf(stream, "Filename:\t%s\n", notConfigured);	
		}
		// Class
		if (elven_file->elfClass)
		{
			fprintf(stream, "Class:\t\t%s\n", elven_file->elfClass);
		}
		else
		{
			fprintf(stream, "Class:\t\t%s\n", notConfigured);	
		}
		// Endianess
		if (elven_file->endianess)
		{
			fprintf(stream, "Endianess:\t%s\n", elven_file->endianess);
		}
		else
		{
			fprintf(stream, "Endianess:\t%s\n", notConfigured);	
		}
		// ELF Version
		fprintf(stream, "ELF Version:\t%d\n", elven_file->version);
		// Target OS ABI
		if (elven_file->targetOS)
		{
			fprintf(stream, "Target OS ABI:\t%s\n", elven_file->targetOS);
		}
		else
		{
			fprintf(stream, "Target OS ABI:\t%s\n", notConfigured);	
		}
		// Version of the ABI
		fprintf(stream, "ABI Version:\t%d\n", elven_file->ABIversion);
		// Type of ELF File
		// IMPLEMENT THIS LATER
		// Section delineation
		fprintf(stream, "\n\n");
	}

	/* PROGRAM HEADER */
	if (sectionsToPrint & PRINT_ELF_PRGRM_HEADER || sectionsToPrint & PRINT_EVERYTHING)
	{
		// Header
		print_fancy_header(stream, "PROGRAM HEADER", HEADER_DELIM);
		// Implement later
		fprintf(stream, "\n\n");
	}

	/* SECTION HEADER */
	if (sectionsToPrint & PRINT_ELF_SECTN_HEADER || sectionsToPrint & PRINT_EVERYTHING)
	{
		// Header
		print_fancy_header(stream, "SECTION HEADER", HEADER_DELIM);
		// Implement later
		fprintf(stream, "\n\n");
	}

	/* PROGRAM DATA */
	if (sectionsToPrint & PRINT_ELF_PRGRM_DATA || sectionsToPrint & PRINT_EVERYTHING)
	{
		// Header
		print_fancy_header(stream, "PROGRAM DATA", HEADER_DELIM);
		// Implement later
		fprintf(stream, "\n\n");
	}

	/* SECTION DATA */
	if (sectionsToPrint & PRINT_ELF_SECTN_DATA || sectionsToPrint & PRINT_EVERYTHING)
	{
		// Header
		print_fancy_header(stream, "SECTION DATA", HEADER_DELIM);
		// Implement later
		fprintf(stream, "\n\n");
	}

	return;
}


// Purpose:	Prints an uppercase title surrounded by delimiters
// Input:
//			stream - Stream to print the header to
//			title - Title to print
//			delimiter - Single character to create the box
// Output:	None
// Note:	Automatically sizes the box
void print_fancy_header(FILE* stream, char* title, unsigned char delimiter)
{
	int headerWidth = 0;  // Used to dynamically determine the width of the header
	int i = 0;  // Iterating variable

	if (!stream || !title || !delimiter)
	{
		fprintf(stderr, "ERROR: NULL/nul parameter received!\n");
		return;
	}

	// ### title ###
	headerWidth = 3 + 1 + strlen(title) + 1 + 3;

	// First line
	for (i = 0; i < headerWidth; i++)
	{
		fputc(delimiter, stream);
	}
	fputc(0xA, stream);

	// Second line
	fprintf(stream, "%c%c%c %s %c%c%c\n", delimiter, delimiter, delimiter, title, delimiter, delimiter, delimiter);

	// Third line
	for (i = 0; i < headerWidth; i++)
	{
		fputc(delimiter, stream);
	}
	fputc(0xA, stream);

	return;
}


// Purpose:	Determine the exact length of a file
// Input:	Open FILE pointer
// Output:	Exact length of file in bytes
size_t file_len(FILE* openFile)
{
	size_t retVal = 0;
	char oneLetter = 'H';

	if (openFile)
	{
		while (1)
		{
			oneLetter = fgetc(openFile);
			PERROR(errno);  // DEBUGGING

			if (oneLetter != EOF)
			{
				retVal++;
			}
			else
			{
				break;
			}
		}

		rewind(openFile);
	}
	else
	{
		retVal = -1;
	}

	return retVal;
}


// Purpose:	Print a buffer, regardless of nul characters
// Input:	
//			buff - non-nul terminated char array
//			size - number of characters in buff
// Output:	Number of characters printed
size_t print_it(char* buff, size_t size)
{
	size_t retVal = 0;

	puts("\n");
	if (buff)
	{
		for (; retVal < size; retVal++)
		{
			putchar((*(buff + retVal)));
		}
	}
	puts("\n");

	return retVal;
}


// Purpose:	Wrap calloc
// Input:
//			numElem - function allocates memory for an array of numElem elements
//			sizeElem - size of each numElem
// Output:	Pointer to dynamically allocated array
// Note:	
//			Cast the return value to the type you want
//			It is the responsibility of the calling function to free the mem returned
void* gimme_mem(size_t numElem, size_t sizeElem)
{
	void* retVal = NULL;
	int numRetries = 0;

	for (; numRetries <= MAX_RETRIES; numRetries++)
	{
		retVal = (void*)calloc(numElem, sizeElem);
		if (retVal)
		{
			break;
		}
	}

	if (!retVal)
	{
		PERROR(errno);
	}

	return retVal;
}


// Purpose:	Automate zeroizing, free'ing, and NULL'ing of dynamically allocated memory
// Input:	
//			buff - Pointer to a buffer pointer
//			numElem - The number of things in *buff
//			sizeElem - The size of each thing in *buff
// Output:	ERROR_* as specified in Elf_Details.h
// Note:	Modifies the pointer to *buf by making it NULL
int take_mem_back(void** buff, size_t numElem, size_t sizeElem)
{
	/* LOCAL VARIABLES */
	int retVal = ERROR_SUCCESS;

	/* INPUT VALIDATION */
	if (!buff)
	{
		retVal = ERROR_NULL_PTR;
	}
	else if (!(*buff))
	{
		retVal = ERROR_NULL_PTR;
	}
	else if (numElem < 1 || sizeElem < 1)
	{
		retVal = ERROR_BAD_ARG;
	}
	/* FREE MEMORY */
	else
	{
		// Zeroize the memory
		memset(*buff, ZEROIZE_CHAR, numElem * sizeElem);
		PERROR(errno);

		// Free the memory
		free(*buff);
		PERROR(errno);

		// NULL the pointer variable
		*buff = NULL;
	}

	return retVal;
}


// Purpose:	Assist clean up efforts by zeroizing/free'ing an Elf_Details struct
// Input:	Pointer to an Elf_Details struct pointer
// Output:	ERROR_* as specified in Elf_Details.h
// Note:	This function will modify the original variable in the calling function
int kill_elf(struct Elf_Details** old_struct)
{
	int retVal = ERROR_SUCCESS;

	if (old_struct)
	{
		if (*old_struct)
		{
			/* ZEROIZE AND FREE (as appropriate) STRUCT MEMBERS */
			// char* fileName;		// Absolute or relative path
			if ((*old_struct)->fileName)
			{
				retVal += take_mem_back((void**)&((*old_struct)->fileName), strlen((*old_struct)->fileName), sizeof(char));
				if (retVal)
				{
					PERROR(errno);
					fprintf(stderr, "take_mem_back() returned %d on struct->filename free!\n", retVal);
					retVal = ERROR_SUCCESS;
				}
			}
			// char* elfClass;		// 32 or 64 bit
			if ((*old_struct)->elfClass)
			{
				retVal += take_mem_back((void**)&((*old_struct)->elfClass), strlen((*old_struct)->elfClass), sizeof(char));
				if (retVal)
				{
					PERROR(errno);
					fprintf(stderr, "take_mem_back() returned %d on struct->elfClass free!\n", retVal);
					retVal = ERROR_SUCCESS;
				}
			}
			// char* endianess;	// Little or Big
			if ((*old_struct)->endianess)
			{
				retVal += take_mem_back((void**)&((*old_struct)->endianess), strlen((*old_struct)->endianess), sizeof(char));
				if (retVal)
				{
					PERROR(errno);
					fprintf(stderr, "take_mem_back() returned %d on struct->endianess free!\n", retVal);
					retVal = ERROR_SUCCESS;
				}
			}
			// int version;		// ELF version
			(*old_struct)->version = 0;
			// char* targetOS;		// Target OS ABI
			if ((*old_struct)->targetOS)
			{
				retVal += take_mem_back((void**)&((*old_struct)->targetOS), strlen((*old_struct)->targetOS), sizeof(char));
				if (retVal)
				{
					PERROR(errno);
					fprintf(stderr, "take_mem_back() returned %d on struct->targetOS free!\n", retVal);
					retVal = ERROR_SUCCESS;
				}
			}
			// int ABIversion;		// Version of the ABI
			(*old_struct)->ABIversion = 0;
			// int type;			// The type of ELF file
			(*old_struct)->type = 0;

			/* FREE THE STRUCT ITSELF */
			retVal += take_mem_back((void**)old_struct, 1, sizeof(struct Elf_Details));
			if (retVal)
			{
				PERROR(errno);
				fprintf(stderr, "take_mem_back() returned %d on struct free!\n", retVal);
				retVal = ERROR_SUCCESS;
			}
		}
		else
		{
			retVal = ERROR_NULL_PTR;
		}
	}
	else
	{
		retVal = ERROR_NULL_PTR;
	}

	return retVal;
}


// Purpose:	Build a HarkleDict of Elf Header Class definitions
// Input:	None
// Output:	Pointer to the head node of a linked list of HarkleDicts
// Note:	Caller is responsible for utilizing destroy_a_list() to free this linked list
struct HarkleDict* init_elf_header_class_dict(void)
{
	/* LOCAL VARIABLES */
	struct HarkleDict* retVal = NULL;
	char* arrayOfNames[] = { "32-bit format", "64-bit format" };
	// fprintf(stdout, "ELF_H_CLASS_32:\t%s\n", STR_ME(ELF_H_CLASS_32));  // DEBUGGING
	size_t numNames = sizeof(arrayOfNames)/sizeof(*arrayOfNames);
	int arrayOfValues[] = { ELF_H_CLASS_32, ELF_H_CLASS_64 };
	size_t numValues = sizeof(arrayOfValues)/sizeof(*arrayOfValues);
	int i = 0;

	/* INPUT VALIDATION */
	// Verify the parallel arrays are the same length
	assert(numNames == numValues);

	for (i = 0; i < numNames; i++)
	{
		retVal = add_entry(retVal, (*(arrayOfNames + i)), (*(arrayOfValues + i)));
		if (!retVal)
		{
			fprintf(stderr, "Harkledict add_entry() returned NULL for:\n\tName:\t%s\n\tValue:\t%d\n", \
				(*(arrayOfNames + i)), (*(arrayOfValues + i)));
			break;
		}
	}

	return retVal;
}


// Purpose:	Build a HarkleDict of Elf Header Data definitions
// Input:	None
// Output:	Pointer to the head node of a linked list of HarkleDicts
// Note:	Caller is responsible for utilizing destroy_a_list() to free this linked list
struct HarkleDict* init_elf_header_endian_dict(void)
{
	/* LOCAL VARIABLES */
	struct HarkleDict* retVal = NULL;
	char* arrayOfNames[] = { "Little Endian", "Big Endian" };
	size_t numNames = sizeof(arrayOfNames)/sizeof(*arrayOfNames);
	int arrayOfValues[] = { ELF_H_DATA_LITTLE, ELF_H_DATA_BIG };
	size_t numValues = sizeof(arrayOfValues)/sizeof(*arrayOfValues);
	int i = 0;

	/* INPUT VALIDATION */
	// Verify the parallel arrays are the same length
	assert(numNames == numValues);

	for (i = 0; i < numNames; i++)
	{
		retVal = add_entry(retVal, (*(arrayOfNames + i)), (*(arrayOfValues + i)));
		if (!retVal)
		{
			fprintf(stderr, "Harkledict add_entry() returned NULL for:\n\tName:\t%s\n\tValue:\t%d\n", \
				(*(arrayOfNames + i)), (*(arrayOfValues + i)));
			break;
		}
	}

	return retVal;
}