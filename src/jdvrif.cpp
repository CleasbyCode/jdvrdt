//	JPG Data Vehicle for Reddit, Imgur, Flickr & other compatible social media / image hosting sites. (jdvrif v1.2). 
//	Created by Nicholas Cleasby (@CleasbyCode) 10/04/2023

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

typedef unsigned char BYTE;
typedef unsigned short SBYTE;

struct jdvStruct {
	std::vector<BYTE> ImageVec, FileVec, ProfileVec, EmbdImageVec, EncryptedVec, DecryptedVec;
	std::string IMAGE_NAME, FILE_NAME, MODE;
	bool imageMastodon = false;
	size_t IMAGE_SIZE{}, FILE_SIZE{};
	SBYTE imgVal{}, subVal{};
};

// Update values, such as block size, file sizes and other values, and write them into the relevant vector/array index locations. Overwrites previous values.
class ValueUpdater {
public:
	void Value(std::vector<BYTE>& vect, BYTE(&ary)[], int valueInsertIndex, const size_t VALUE, SBYTE bits, bool isArray) {
		if (isArray) {
			while (bits) {
				ary[valueInsertIndex++] = (VALUE >> (bits -= 8)) & 0xff;
			}
		}
		else {
			while (bits) {
				vect[valueInsertIndex++] = (VALUE >> (bits -= 8)) & 0xff;
			}
		}
	}
} *update;

class VectorFill {
public:
	void Vector(std::vector<BYTE>& vect, std::ifstream& rFile, const size_t FSIZE, const std::string MODE) {
		int largeFile = 52428800; // 50MB or greater considered a large file for jdvrif.
		std::string 
			encryptMsg = "Encrypting and embedding",
			decryptMsg = "Decrypting and extracting",
			waitMsg = MODE == "-i" ? encryptMsg : decryptMsg;
			
		if (FSIZE >= largeFile) {
			std::cout << "\nPlease Wait...\n"<< waitMsg <<" larger files will take more time.\n";
		}
		vect.resize(FSIZE / sizeof(BYTE));
		rFile.read((char*)vect.data(), FSIZE);
	}
} *fill;

// Open user image & data file or Embedded image file. Display error & exit program if any file fails to open.
void openFiles(char* [], jdvStruct& jdv);

// Function finds and removes all the inserted ICC profile blocks.
void removeProfileHeaders(jdvStruct& jdv);

// Encrypt or decrypt user's data file and its filename.
void encryptDecrypt(jdvStruct& jdv);

// Function splits user's data file up into 65KB (or smaller) ICC profile blocks.
void insertProfileBlocks(jdvStruct& jdv);

// Write out to file the embedded image file or the extracted data file.
void writeOutFile(jdvStruct& jdv);

// Display program infomation
void displayInfo();

int main(int argc, char** argv) {

	jdvStruct jdv;

	if (argc == 2 && std::string(argv[1]) == "--info") {
		argc = 0;
		displayInfo();
	}
	else if (argc >= 4 && argc < 10 && std::string(argv[1]) == "-i") { // Insert file mode.
		jdv.MODE = argv[1], jdv.subVal = argc - 1, jdv.IMAGE_NAME = argv[2];
		argc -= 2;
		while (argc != 1) {  // We can insert upto six files at a time (outputs one image for each file).
			jdv.imgVal = argc, jdv.FILE_NAME = argv[3];
			openFiles(argv++, jdv);
			argc--;
		}
		argc = 1;
	}
	else if (argc >= 3 && argc < 9 && std::string(argv[1]) == "-x") { // Extract file mode.
		jdv.MODE = argv[1];
		while (argc >= 3) {  // We can extract files from upto six embedded images at a time.
			jdv.IMAGE_NAME = argv[2];
			openFiles(argv++, jdv);
			argc--;
		}
	}
	else {
		std::cout << "\nUsage:\t\bjdvrif -i <jpg-image>  <file(s)>\n\t\bjdvrif -x <jpg-image(s)>\n\t\bjdvrif --info\n\n";
		argc = 0;
	}
	if (argc != 0) {
		if (argc == 2) {
			std::cout << "\nComplete! Please check your extracted file(s).\n\n";
		}
		else {
			std::cout << "\nComplete!\n\nYou can now share your \"file-embedded\" JPG image(s) on compatible platforms.\n\n";
		}
	}
	return 0;
}

void openFiles(char* argv[], jdvStruct& jdv) {

	const std::string READ_ERR_MSG = "\nRead Error: Unable to open/read file: ";

	std::ifstream
		readImage(jdv.IMAGE_NAME, std::ios::binary),
		readFile(jdv.FILE_NAME, std::ios::binary);

	if (jdv.MODE == "-i" && (!readImage || !readFile) || jdv.MODE == "-x" && !readImage) {

		// Open file failure, display relevant error message and exit program.
		const std::string ERR_MSG = !readImage ? READ_ERR_MSG + "\"" + jdv.IMAGE_NAME + "\"\n\n" : READ_ERR_MSG + "\"" + jdv.FILE_NAME + "\"\n\n";

		std::cerr << ERR_MSG;
		std::exit(EXIT_FAILURE);
	}

	// Get size of files.
	readImage.seekg(0, readImage.end),
	readFile.seekg(0, readFile.end);

	// Update file size variables.
	jdv.IMAGE_SIZE = readImage.tellg();
	jdv.FILE_SIZE = readFile.tellg();

	// Reset read position of files. 
	readImage.seekg(0, readImage.beg),
	readFile.seekg(0, readFile.beg);

	if (jdv.MODE == "-x") { // Extract mode. 

		fill->Vector(jdv.EmbdImageVec, readImage, jdv.IMAGE_SIZE, jdv.MODE);

		// Signature index location within vector "EmbdImageVec", if image is saved/downloaded from Mastodon. 
		// Mastodon retains the ICC profiles & the embedded data, but inserts a longer JPG header, which we need to delete before extracting file(s).
		const SBYTE JDV_SIG_INDEX_MASTODON = 43;	

		// Signature index location within vector "EmbdImageVec". 
		const SBYTE JDV_SIG_INDEX = 25;	

		if (jdv.EmbdImageVec[JDV_SIG_INDEX] == 'J' && jdv.EmbdImageVec[JDV_SIG_INDEX + 4] == 'i') {
			
			removeProfileHeaders(jdv);
		}
		else if (jdv.EmbdImageVec[JDV_SIG_INDEX_MASTODON] == 'J' && jdv.EmbdImageVec[JDV_SIG_INDEX_MASTODON + 4] == 'i') {

			const SBYTE HEADER_SIZE = 18;

			// Erase the 18 bytes of the JPG header that was inserted by Mastodon.
			jdv.EmbdImageVec.erase(jdv.EmbdImageVec.begin() + 2, jdv.EmbdImageVec.begin() + 2 + HEADER_SIZE);
			
			jdv.imageMastodon = true;
			
			removeProfileHeaders(jdv);
		}	
		else {
			std::cerr << "\nImage Error: Image file \"" << jdv.IMAGE_NAME << "\" does not appear to be a jdvrif file-embedded image.\n\n";
			std::exit(EXIT_FAILURE);
		}
	}
	else { // Insert mode. 
		
		// Get total size of Profile headers (approx.). Each header is 18 bytes long and is inserted at every 65K of the data file
		const size_t TOTAL_ICC_PROFILE_SIZE = jdv.FILE_SIZE / 65535 * 18; 
		
		// 200MB file size limit for Flickr.  20MB limit for Reddit & Imgur.
		const size_t MAX_FILE_SIZE = 209715200; 
		
		if (jdv.IMAGE_SIZE + jdv.FILE_SIZE + TOTAL_ICC_PROFILE_SIZE > MAX_FILE_SIZE) {
			
			// File size check failure, display error message and exit program.
			std::cerr << "\nFile Size Error:\n\n  Your data file size [" << jdv.FILE_SIZE+jdv.IMAGE_SIZE + TOTAL_ICC_PROFILE_SIZE <<
				" Bytes] must not exceed 200MB (209715200 Bytes).\n\n" <<
				"  The data file size includes:\n\n    Image file size [" << jdv.IMAGE_SIZE << " Bytes],\n    Total size of ICC Profile Headers ["
				<< TOTAL_ICC_PROFILE_SIZE << " Bytes] (18 bytes for every 65K of the data file).\n\n";
			
			std::exit(EXIT_FAILURE);
		}

		// The first 152 bytes of this vector contains the main (basic) ICC profile.
		jdv.ProfileVec.reserve(jdv.FILE_SIZE);
		jdv.ProfileVec = {
			0xFF, 0xD8, 0xFF, 0xE2, 0xFF, 0xFF, 0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46,
			0x49, 0x4C, 0x45, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x84, 0x20, 0x4A, 0x44, 0x56,
			0x52, 0x69, 0x46, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x61, 0x63, 0x73, 0x70, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};

		// Read-in and store JPG image file into vector "ImageVec".
		fill->Vector(jdv.ImageVec, readImage, jdv.IMAGE_SIZE, jdv.MODE);

		// Read-in and store user's data file into vector "FileVec".
		fill->Vector(jdv.FileVec, readFile, jdv.FILE_SIZE, jdv.MODE);

		// This vector will be used to store the users encrypted data file.
		jdv.EncryptedVec.reserve(jdv.FILE_SIZE);

		const std::string
			JPG_SIG = "\xFF\xD8\xFF",	// JPG image signature. 
			JPG_CHECK{ jdv.ImageVec.begin(), jdv.ImageVec.begin() + JPG_SIG.length() };	// Get image header from vector. 

		// Before removing header, make sure we are dealing with a valid JPG image file.
		if (JPG_CHECK != JPG_SIG) {
			// File requirements check failure, display relevant error message and exit program.
			std::cerr << "\nImage Error: File does not appear to be a valid JPG image.\n\n";
			std::exit(EXIT_FAILURE);
		}

		// Signature for Define Quantization Table(s) 
		const auto DQT_SIG = { 0xFF, 0xDB };

		// Find location in vector "ImageVec" of first DQT index location of the image file.
		const size_t DQT_POS = search(jdv.ImageVec.begin(), jdv.ImageVec.end(), DQT_SIG.begin(), DQT_SIG.end()) - jdv.ImageVec.begin();

		// Erase the first n bytes of the JPG header before this DQT position. We later replace the erased header with the contents of vector "ProfileVec".
		jdv.ImageVec.erase(jdv.ImageVec.begin(), jdv.ImageVec.begin() + DQT_POS);

		// Encrypt the user's data file and its filename.
		encryptDecrypt(jdv);
	}
}

void removeProfileHeaders(jdvStruct& jdv) {
	
	std::cout << "\nOK, jdvrif \"file-embedded\" image found!\n";
	
	const SBYTE
		PROFILE_HEADER_LENGTH = 18,	// Byte length value of the embedded profile header (see "ProfileBlockVec"). 
		NAME_LENGTH_INDEX = 32,		// Index location for length value of filename for user's data file.
		NAME_LENGTH = jdv.EmbdImageVec[NAME_LENGTH_INDEX], // Get embedded value of filename length from "EmbdImageVec", stored within the main profile.
		NAME_INDEX = 33,		// Start index location of filename for user's data file.
		PROFILE_COUNT_INDEX = 72,	// Value index location for the total number of inserted ICC profile headers (see "ProfileBlockVec").
		FILE_SIZE_INDEX = 88,		// Start index location for the file size value of the user's data file.
		FILE_INDEX = 152; 		// Start index location of user's data file within vector "EmbdImageVec".

	// From the relevant index location, get size value of user's data file from "EmbdImageVec", stored within the mail profile.
	const size_t FILE_SIZE = jdv.EmbdImageVec[FILE_SIZE_INDEX] << 24 | jdv.EmbdImageVec[FILE_SIZE_INDEX + 1] << 16 |
		jdv.EmbdImageVec[FILE_SIZE_INDEX + 2] << 8 | jdv.EmbdImageVec[FILE_SIZE_INDEX + 3];
		
	// Signature string for the embedded ICC profile headers we need to find & remove from the user's data file.
	const std::string PROFILE_SIG = "ICC_PROFILE";	

	// From vector "EmbdImageVec", get the value of the total number of embedded profile headers, stored within the mail profile.
	SBYTE profileCount = jdv.EmbdImageVec[PROFILE_COUNT_INDEX] << 8 | jdv.EmbdImageVec[PROFILE_COUNT_INDEX + 1];

	// Get the encrypted filename from vector "EmbdImageVec", stored within the main profile.
	jdv.FILE_NAME = { jdv.EmbdImageVec.begin() + NAME_INDEX, jdv.EmbdImageVec.begin() + NAME_INDEX + jdv.EmbdImageVec[NAME_LENGTH_INDEX] };

	// Erase the 152 byte main profile from vector "EmbdImageVec", so that the start of "EmbdImageVec" is now the beginning of the user's encrypted data file.
	jdv.EmbdImageVec.erase(jdv.EmbdImageVec.begin(), jdv.EmbdImageVec.begin() + FILE_INDEX);

	size_t headerIndex = 0; // Variable will store the index location within "EmbdImageVec" of each ICC profile header we find within the vector.

	// Within "EmbdImageVec" find and erase all occurrences of the 18 byte ICC profile header, (see "ProfileBlockVec").
	// If imageMastodon is true and profileCount is just 1, then skip this while-loop as Mastodon has already stripped for us the last (single) 18 byte ICC Profile header.
	if (!jdv.imageMastodon && profileCount > 1) {
		while (profileCount--) {
			headerIndex = search(jdv.EmbdImageVec.begin() + headerIndex, jdv.EmbdImageVec.end(), PROFILE_SIG.begin(), PROFILE_SIG.end()) - jdv.EmbdImageVec.begin() - 4;
			jdv.EmbdImageVec.erase(jdv.EmbdImageVec.begin() + headerIndex, jdv.EmbdImageVec.begin() + headerIndex + PROFILE_HEADER_LENGTH);
		}
	}
	// Remove the JPG image from the user's data file. 
	// Erase all bytes starting from the end of "FILE_SIZE" value. Vector "EmbdImageVec" now contains just the user's encrypted data file.
	jdv.EmbdImageVec.erase(jdv.EmbdImageVec.begin() + FILE_SIZE, jdv.EmbdImageVec.end());

	// This vector will be used to store the decrypted user's data file. 
	jdv.DecryptedVec.reserve(FILE_SIZE);

	// The encrypted data file is now stored in the vector "FileVec".
	jdv.EmbdImageVec.swap(jdv.FileVec);

	// Decrypt the contents of "FileVec".
	encryptDecrypt(jdv);
}

void encryptDecrypt(jdvStruct& jdv) {

	if (jdv.MODE == "-i") { // Insert mode.
		
		// Before we encrypt user's data filename, check for and remove "./" or ".\" characters at the start of the filename. 
		size_t lastSlashPos = jdv.FILE_NAME.find_last_of("\\/");

		if (lastSlashPos <= jdv.FILE_NAME.length()) {
			std::string_view new_name(jdv.FILE_NAME.c_str() + (lastSlashPos + 1), jdv.FILE_NAME.length() - (lastSlashPos + 1));
			jdv.FILE_NAME = new_name;
		}
	}

	const std::string XOR_KEY = "\xFF\xD8\xFF\xE2\xFF\xFF";	// String used to xor encrypt/decrypt the filename of user's data file.

	size_t indexPos = 0;   	// When encrypting/decrypting the filename, this variable stores the index read position of the filename,
				// When encrypting/decrypting the user's data file, this variable is used as the index position of where to 
				// insert each byte of the data file into the relevant "encrypted" or "decrypted" vectors.

	const SBYTE MAX_LENGTH_FILENAME = 23;

	const size_t
		NAME_LENGTH = jdv.FILE_NAME.length(),	// Filename length of user's data file.
		XOR_KEY_LENGTH = XOR_KEY.length(),
		FILE_SIZE = jdv.FileVec.size();		// File size of user's data file.

	// Make sure character length of filename does not exceed set maximum.
	if (NAME_LENGTH > MAX_LENGTH_FILENAME) {
		std::cerr << "\nFile Error: Filename length of your data file (" + std::to_string(NAME_LENGTH) + " characters) is too long.\n"
			"\nFor compatibility requirements, your filename must be under 24 characters.\nPlease try again with a shorter filename.\n\n";
		std::exit(EXIT_FAILURE);
	}

	std::string
		inName = jdv.FILE_NAME,
		outName;

	SBYTE
		xorKeyStartPos = 0,
		xorKeyPos = xorKeyStartPos,	// Character position variable for XOR_KEY string.
		nameKeyStartPos = 0,
		nameKeyPos = nameKeyStartPos,	// Character position variable for filename string (outName / inName).
		bits = 8;			// Value used in "updateValue" function.

	// xor encrypt or decrypt filename and data file.
	while (FILE_SIZE > indexPos) {

		if (indexPos >= NAME_LENGTH) {
			nameKeyPos = nameKeyPos > NAME_LENGTH ? nameKeyStartPos : nameKeyPos;	 // Reset filename character position to the start if it's reached last character.
		}
		else {
			xorKeyPos = xorKeyPos > XOR_KEY_LENGTH ? xorKeyStartPos : xorKeyPos;	// Reset XOR_KEY position to the start if it's reached last character.
			outName += inName[indexPos] ^ XOR_KEY[xorKeyPos++];	// XOR each character of filename against characters of XOR_KEY string. Store output characters in "outName".
										// Depending on Mode, filename is either encrypted or decrypted.
		}

		if (jdv.MODE == "-i") {
			// Encrypt data file. XOR each byte of the data file within "jdv.FileVec" against each character of the encrypted filename, "outName". 
			// Store encrypted output in vector "jdv.EncryptedVec".
			jdv.EncryptedVec.emplace_back(jdv.FileVec[indexPos++] ^ outName[nameKeyPos++]);
		}
		else {
			// Decrypt data file: XOR each byte of the data file within vector "jdv.FileVec" against each character of the encrypted filename, "inName". 
			// Store decrypted output in vector "jdv.DecryptedVec".
			jdv.DecryptedVec.emplace_back(jdv.FileVec[indexPos++] ^ inName[nameKeyPos++]);
		}
	}
	
	if (jdv.MODE == "-i") { // Insert mode.

		const SBYTE
			PROFILE_NAME_LENGTH_INDEX = 32, // Location index inside the main profile "ProfileVec" to store the filename length value of the user's data file.
			PROFILE_NAME_INDEX = 33,	// Location index inside the main profile "ProfileVec" to store the filename of the user's data file.
			PROFILE_VEC_SIZE = 152;		// Byte size of main profile within "ProfileVec". User's encrypted data file is stored at the end of the main profile.

		// Update the character length value of the filename for user's data file. Write value into the main profile of vector "ProfileVec".
		jdv.ProfileVec[PROFILE_NAME_LENGTH_INDEX] = static_cast<int>(NAME_LENGTH);

		// Make space for the filename by removing equivalent length of characters from main profile within vector "ProfileVec".
		jdv.ProfileVec.erase(jdv.ProfileVec.begin() + PROFILE_NAME_INDEX, jdv.ProfileVec.begin() + outName.length() + PROFILE_NAME_INDEX);
	
		// Insert the encrypted filename into the main profile of vector "ProfileVec".
		jdv.ProfileVec.insert(jdv.ProfileVec.begin() + PROFILE_NAME_INDEX, outName.begin(), outName.end());
	
		// Insert contents of vector "EncryptedVec" into vector "ProfileVec", combining then main profile with the user's encrypted data file.	
		jdv.ProfileVec.insert(jdv.ProfileVec.begin() + PROFILE_VEC_SIZE, jdv.EncryptedVec.begin(), jdv.EncryptedVec.end());
		
		// Call function to insert ICC profile headers into the users data file, so as to split it into 65KB (or smaller) profile blocks.
		insertProfileBlocks(jdv);
	}
	else { // Extract Mode.
		
		// Update string variable with the decrypted filename.
		jdv.FILE_NAME = outName;

		// Write the extracted (decrypted) data file out to disk.
		writeOutFile(jdv);
	}
}

void insertProfileBlocks(jdvStruct& jdv) {

	const SBYTE PROFILE_HEADER_SIZE = 18;

	BYTE ProfileHeaderBlock[PROFILE_HEADER_SIZE] = { 0xFF, 0xE2, 0xFF, 0xFF, 0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00, 0x01, 0x01 };

	const size_t
		VECTOR_SIZE = jdv.ProfileVec.size(),	// Get updated size for vector "ProfileVec" after adding user's data file.
		BLOCK_SIZE = 65535;			// ICC profile default block size (0xFFFF).

	size_t tallySize = 2;	// Keep count of how much data we have traversed while inserting "ProfileBlockVec" headers at every "BLOCK_SIZE" within "ProfileVec". 

	SBYTE
		bits = 16,			// Variable used with the "updateValue" function.	
		profileCount = 0,		// Keep count of how many ICC profile blocks ("ProfileBlockVec") we insert into the user's data file.
		profileMainBlockSizeIndex = 4,  // "ProfileVec" start index location for the 2 byte block size field.
		profileBlockSizeIndex = 2,	// "ProfileBlockVec" start index location for the 2 byte block size field.
		profileCountIndex = 72,		// Start index location in main profile, where we store the value of the total number of inserted ICC profile headers.
		profileDataSizeIndex = 88;	// Start index location in main profile, where we store the file size value of the user's data file.

	// Where we see +4 (-4) or +2, these values are the number of bytes at the start of vector "ProfileVec" (4 bytes: 0xFF, 0xD8, 0xFF, 0xE2) 
	// and "ProfileBlockVec" (2 bytes: 0xFF, 0xE2), just before the default "BLOCK_SIZE" bytes: 0xFF, 0xFF, where the block count starts from. 
	// We need to count or subtract these bytes where relevant.

	if (BLOCK_SIZE + 4 >= VECTOR_SIZE) {

		// Seems we are dealing with a small data file. All data content fits within the main 65KB profile block. 
		// Finish up, skip the "While loop" and write the "embedded" image out to file, exit program.

		// Update profile block size of vector "ProfileVec", as it is smaller than the set default value (0xFFFF).
		update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileMainBlockSizeIndex, VECTOR_SIZE - 4, bits, false);

		// Even though no more profile header blocks are required, so as to prevent the app GIMP from displaying an error message regarding an invalid ICC profile, 
		// we will insert a single "ProfileBlockVec" with a minimum size of 16 bytes at the end of the data file, just before start of JPG image.

		update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileBlockSizeIndex, (PROFILE_HEADER_SIZE - 2), bits, true);

		jdv.ProfileVec.insert(jdv.ProfileVec.begin() + VECTOR_SIZE, &ProfileHeaderBlock[0], &ProfileHeaderBlock[PROFILE_HEADER_SIZE]); // Insert last "ProfileBlockVec" header.

		profileCount = 1; // One will always be the minimum number of ICC profile headers. 

		// Insert final profileCount value into vector "Profile". // 1
		update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileCountIndex, profileCount, bits, false);

		bits = 32;

		// Insert file size value of user's data file into vector "ProfileVec".
		update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileDataSizeIndex, jdv.FILE_SIZE, bits, false);
	}

	// Insert "ProfileBlockVec" vector header contents into data file at every "BLOCK_SIZE", or whatever size remains, that is below "BLOCK_SIZE", until end of file.
	if (VECTOR_SIZE > BLOCK_SIZE + 4) {

		bool isMoreData = true;

		while (isMoreData) {

			tallySize += BLOCK_SIZE + 2;

			if (BLOCK_SIZE + 2 >= jdv.ProfileVec.size() - tallySize + 2) {

				// Data file size remaining is less than or equal to, the "BLOCK_SIZE" default value +2, 
				// We are near the end of the file. Update profile block size & insert the final profile block header and some other values, then exit the loop.
				update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileBlockSizeIndex, (jdv.ProfileVec.size() + PROFILE_HEADER_SIZE) - (tallySize + 2), bits, true);

				profileCount++;

				// Update final profileCount value into vector "ProfileVec".
				update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileCountIndex, profileCount, bits, false);

				bits = 32;

				// Insert file size value of user's data file into vector "ProfileVec".
				update->Value(jdv.ProfileVec, ProfileHeaderBlock, profileDataSizeIndex, jdv.FILE_SIZE, bits, false);

				// Insert final "ProfileBlockVec" header contents.
				jdv.ProfileVec.insert(jdv.ProfileVec.begin() + tallySize, &ProfileHeaderBlock[0], &ProfileHeaderBlock[PROFILE_HEADER_SIZE]);

				isMoreData = false; // No more data, exit loop.
			}

			else {  // Keep going, we have not yet reached end of file (last block size).
				
				profileCount++;
				
				// Insert another ICC profile header ("ProfileBlockVec").
				jdv.ProfileVec.insert(jdv.ProfileVec.begin() + tallySize, &ProfileHeaderBlock[0], &ProfileHeaderBlock[PROFILE_HEADER_SIZE]);
			}
		}
	}

	// Insert contents of vector "ProfileVec" into vector "ImageVec", combining user's data file and ICC profile header blocks, with the JPG image.	
	jdv.ImageVec.insert(jdv.ImageVec.begin(), jdv.ProfileVec.begin(), jdv.ProfileVec.end());

	std::string diffVal = std::to_string(jdv.subVal - jdv.imgVal);	// If we embed multiple data files (max 6), each outputted image will be differentiated 
									// by a number in the name, e.g. jdv_img1.jpg, jdv_img2.jpg, jdv_img3.jpg.
	jdv.FILE_NAME = "jdv_img" + diffVal + ".jpg";

	writeOutFile(jdv);
}

void writeOutFile(jdvStruct& jdv) {

	std::ofstream writeFile(jdv.FILE_NAME, std::ios::binary);

	if (!writeFile) {
		std::cerr << "\nWrite Error: Unable to write to file.\n\n";
		std::exit(EXIT_FAILURE);
	}

	if (jdv.MODE == "-i") {
		// Write out to disk image file embedded with the encrypted data file.
		writeFile.write((char*)&jdv.ImageVec[0], jdv.ImageVec.size());
		std::cout << "\nCreated output file: \"" + jdv.FILE_NAME + " " << jdv.ImageVec.size() << " " << "Bytes\"\n";
		std::string msgSizeWarning = 
			"\n**Warning**\n\nDue to the file size of your \"file-embedded\" JPG image,\nyou will only be able to share " + jdv.FILE_NAME + " on the following platforms: \n\n"
			"Flickr, ImgPile, ImgBB, ImageShack, PostImage, Reddit & Imgur";
		const size_t
			msgLen = msgSizeWarning.length(),
			imgSize = jdv.ImageVec.size(),
			mastodonSize = 8388608,		// 8MB
			imgurRedditSize = 20971520,	// 20MB
			postImageSize = 25165824,	// 24MB
			imageShackSize = 26214400,	// 25MB
			imgbbSize = 33554432,		// 32MB
			imgPileSize = 104857600;	// 100MB
		
		msgSizeWarning = (imgSize > imgurRedditSize && imgSize <= postImageSize ? msgSizeWarning.substr(0, msgLen - 16) 
			: (imgSize > postImageSize && imgSize <= imageShackSize ? msgSizeWarning.substr(0, msgLen - 27) 
			: (imgSize > imageShackSize && imgSize <= imgbbSize ? msgSizeWarning.substr(0, msgLen - 39) 
			: (imgSize > imgbbSize && imgSize <= imgPileSize ? msgSizeWarning.substr(0, msgLen - 46) 
			: (imgSize > imgPileSize ? msgSizeWarning.substr(0, msgLen - 55) : msgSizeWarning)))));

		if (imgSize > mastodonSize) {
			std::cerr << msgSizeWarning << ".\n";
		}
		
		jdv.EncryptedVec.clear();
	}
	else {
		// Write out to disk the extracted (decrypted) data file.
		writeFile.write((char*)&jdv.DecryptedVec[0], jdv.DecryptedVec.size());
		std::cout << "\nExtracted file: \"" + jdv.FILE_NAME + " " << jdv.DecryptedVec.size() << " " << "Bytes\"\n";
		jdv.DecryptedVec.clear();
	}
}

void displayInfo() {

	std::cout << R"(
JPG Data Vehicle for Reddit, Imgur & Flickr (jdvrif v1.2). Created by Nicholas Cleasby (@CleasbyCode) 10/04/2023.

jdvrif enables you to embed & extract arbitrary data of up to *200MB within a single JPG image.

You can upload and share your data-embedded JPG image file on compatible social media & image hosting sites.

*Size limit per JPG image is platform dependant:-

  Flickr (200MB), ImgPile (100MB), ImgBB (32MB), ImageShack (25MB),
  PostImage (24MB), Reddit & *Imgur (20MB), Mastodon (8MB).

*Imgur issue: Data is still retained when the file-embedded JPG image is over 5MB, but Imgur reduces the dimension size of the image.
 
jdvrif data-embedded images will not work with Twitter. For Twitter, please use pdvzip (PNG only).

This program works on Linux and Windows.

The file data is inserted and preserved within multiple 65KB ICC Profile blocks in the image file.
 
To maximise the amount of data you can embed in your image file. I recommend compressing your 
data file(s) to zip/rar formats, etc.

Using jdvrif, You can insert up to six files at a time (outputs one image per file).

You can also extract files from up to six images at a time.

)";
}
