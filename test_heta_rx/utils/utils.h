

// *******************************************************************************************
// Function: 
//		long writeBinaryFileToArray (char frame_name[], unsigned char *frame_data);
// 
// Description:
//		This routine open a binary file in read mode, and copy the file content to an array.
// 
// Parameters:
// 		frame_name	- The binay file name
//		frame_data	- The array name
//
// Return:
//		The size of binary file or the length of array (in byte).
// *******************************************************************************************
long writeBinaryFileToArray (char frame_name[], unsigned char *frame_data);


// *******************************************************************************************
// Function: 
//		void writeArrayToBinaryFile (char frame_name[], unsigned char *frame_data, long frame_data_size);
// 
// Description:
//		This routine open a binary file in write mode, and copy the array content to this file.
// 
// Parameters:
// 		frame_name	- The binay file name
//		frame_data	- The array name
//		frame_data_size - The length of array (in byte)
//
// Return:
//		None
// *******************************************************************************************
void writeArrayToBinaryFile (char frame_name[], unsigned char *frame_data, long frame_data_size);


// *******************************************************************************************
// Function: 
//		char* int2str (int value, char str[], char base);
// 
// Description:
//		This routine convert an integer value to string.
// 
// Parameters:
// 		value		- The input integer value
//		str			- The output string
//		base		- 10: decimal expression
//					- 16: hexadecimal expression
//
// Return:
//		The first address of output string.
// *******************************************************************************************
char* int2str (int value, char str[], char base);
