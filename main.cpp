// Copyright 2006 Steve Bjorg
//
// This file is part of FLDBUTIL.
// 
// FLDBUTIL is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// FLDBUTIL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with FLDBUTIL; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VER_MAJOR 0
#define VER_MINOR 2

#define BUFFER_SIZE	(1024*1024)

typedef unsigned int DWORD;
typedef char FLDB_FILE_NAME[24];

#if BYTE_ORDER == 4321
inline DWORD ConvertLittleEndianToHost(DWORD value) {
	return ((value & 0xFF000000) >> 24) | ((value & 0xFF0000) >> 8) | ((value & 0xFF00) << 8) | ((value & 0xFF) << 24);
}
#else
inline DWORD ConvertLittleEndianToHost(DWORD value) {
	return value;
}
#endif

template <class VALUE_TYPE>
inline VALUE_TYPE min(VALUE_TYPE first, VALUE_TYPE second) {
	return (first < second) ? first : second;
}

inline void SetFullPath(char* fullpath, const char* basepath, const char* filename) {
	strcpy(fullpath, basepath);
	int len = strlen(fullpath);
	if((len != 0) && (fullpath[len-1] != '/')) {
		strcat(fullpath, "/");
	}
	strcat(fullpath, filename);
}

struct FLDBHeader {

	//--- Fields ---
	DWORD	headerLen;		// header length, always 0x220
	DWORD	_unknown_2;		// always 0x01
	DWORD	timestamp;		// compile timestamp, as epoch	
	DWORD	Count;			// number of files in archive
	DWORD	_unknown_4;	 	// probably data version, so far always 0x24 (36)
	char	Magic[4];		// the 'FLDB'
	DWORD	_unknown_5;	 	// always 0x00
	DWORD	_unknown6;		// so far always 0x00
	char	comment[512];	// file comment, multiline, zero-terminated
	
	//--- Methods ---
	bool Read(FILE* f) {
		memset(this, 0, sizeof(FLDBHeader));
		if(fread(this, sizeof(FLDBHeader), 1, f) != 1) {
			return false;
		}
		if(memcmp(Magic, "FLDB", sizeof(Magic)) != 0) {
			return false;
		}
		Count = ConvertLittleEndianToHost(Count);
		return true;
	}
	
	void Print(FILE* f) {
		fprintf(f, "Count: %d\n", Count);
	}
};

struct FLDBFileEntry {

	//--- Fields ---
	DWORD	Offset;
	DWORD	Size;
	FLDB_FILE_NAME	Name;
	DWORD	_unknown_6;
	
	//--- Methods ---
	bool Read(FILE* f) {
		memset(this, 0, sizeof(FLDBFileEntry));
		if(fread((void*)this, sizeof(FLDBFileEntry), 1, f) != 1) {
			return false;
		}
		Offset = ConvertLittleEndianToHost(Offset);
		Size = ConvertLittleEndianToHost(Size);
		_unknown_6 = ConvertLittleEndianToHost(_unknown_6);
		return true;
	}
	
	void Print(FILE* f) {
//		fprintf(f, "%s: %d (0x%X) [0x%X = %d]\n", Name, Size, Offset, _unknown_6, _unknown_6);
		time_t t = (time_t)_unknown_6;
		fprintf(f, "%s: %d (0x%X) %s\n", Name, Size, Offset, ctime(&t));
	}
	
	bool Extract(FILE* f, const char* path) {
		static unsigned char buffer[BUFFER_SIZE];
	
		// move to entry position
		fseek(f, Offset, SEEK_SET);
		
		// create destination file
		FILE* g = fopen(path, "wb");
		if(g == nullptr) {
			return false;
		}
		
		// copy source to destination
		for(DWORD work_size = Size, read_size; work_size != 0; work_size -= read_size) {
			read_size = min(work_size, (DWORD)BUFFER_SIZE);
			if(fread(buffer, read_size, 1, f) != 1) {
				fclose(g);
				g = nullptr;
				return false;
			}
			if(fwrite(buffer, read_size, 1, g) != 1) {
				fclose(g);
				g = nullptr;
				return false;
			}
		}
		
		// close file & free memory
		fclose(g);
		g = nullptr;
		return true;
	}
};

void PrintBanner() {
	fprintf(stderr, "FLDBUTIL %d.%d by Cormac\n", VER_MAJOR, VER_MINOR);
}

void PrintUsage() {
	fprintf(stderr, "USAGE: FLDBUTIL <cmd> <source> [ <dest> ]\n");
	fprintf(stderr, "Commands\n");
	fprintf(stderr, "\tx\textract files from .DB file\n");
	fprintf(stderr, "\tv\tshow verbose progress\n");
	fprintf(stderr, "\th\tprint HTML table of contents\n");
}

void PrintHTMLRow(FILE* f, const char* text) {
	fprintf(f, "<td rowspan=\"1\" style=\"border: 1px solid rgb(0, 0, 0); width: 25%%;\">%s</td>", text);
}

void PrintHTMLRow(FILE* f, int count) {
	char buffer[32];
	char* c = &buffer[31];
	*c-- = 0;
	if(count == 0) {
		*c-- = '0';
	} else {
		int digits = 0;
		while(count > 0) {
			if((digits != 0) && (digits % 3 == 0)) {
				*c-- = ',';
			}
			*c-- = '0' + count % 10;
			count /= 10;
			++digits;
		}
	}
	PrintHTMLRow(f, c + 1);
}

void PrintHTML(FILE* f, FLDBFileEntry* entries, DWORD count) {
	fprintf(f, "<table cellpadding=\"1\" cellspacing=\"1\" style=\"border: 1px solid rgb(0, 0, 0); border-collapse: collapse; width: 100%;\"><tbody>");
	fprintf(f, "<tr>");
	PrintHTMLRow(f, "<strong>Filename</strong>");
	PrintHTMLRow(f, "<strong>Date</strong>");
	PrintHTMLRow(f, "<strong>Size</strong>");
	PrintHTMLRow(f, "<strong>Description</strong>");
	fprintf(f, "</tr>");
	for(DWORD i = 0; i < count; ++i) {
		fprintf(f, "<tr>");
		PrintHTMLRow(f, entries[i].Name);
		PrintHTMLRow(f, "");
		PrintHTMLRow(f, entries[i].Size);
		PrintHTMLRow(f, "");
		fprintf(f, "</tr>");
	}
	fprintf(f, "</tbody></table>\n");
}

int main (int argc, char * const argv[]) {
	int exit_code = -1;
	bool verbose = false;
	bool extract = false;
	bool html = false;
	const char* db_filename = nullptr;
	const char* output_path = nullptr;
	
	// check arguments
	PrintBanner();
	if(argc < 3) {
		PrintUsage();
		return 0;
	}
	
	// loop over command argument
	for(const char* cmd = argv[1]; *cmd != 0; ++cmd) {
		switch(*cmd) {
		case 'v':
			verbose = true;
			break;
		case 'x':
			extract = true;
			break;
		case 'h':
			html = true;
			break;
		default:
			return -1;
		}
	}
	db_filename = argv[2];
	output_path = (argc >= 4) ? argv[3] : ".";

	// open database
	FILE* f = fopen(db_filename, "rb");
	if(f == nullptr) {
		fprintf(stderr, "ERROR: unable to open '%s'\n", db_filename);
		return -1;
	}
	
	// read header
	FLDBHeader header;
	if(!header.Read(f)) {
			printf("ERROR: '%s' is not an FLDB file\n", db_filename);
			fclose(f);
			f = nullptr;
			return -1;
	}
	if(verbose) {
		printf("Successfully opened '%s'\n", db_filename);
		printf("--- Header ----\n");
		header.Print(stdout);
		printf("--- Entries (Name:Size (Offset) [Unknown]) ---\n");
	}
		
	// read file entries
	FLDBFileEntry* entries = new FLDBFileEntry[header.Count];
	for(DWORD i = 0; i < header.Count; ++i) {
		if(!entries[i].Read(f)) {
			printf("ERROR: file entry %d is corrupt\n", i);
			goto done_mem_release;
		}
		if(verbose) {
			entries[i].Print(stdout);
		}
	}
	if(verbose) {
		printf("---------------\n");
	}

	// extract entries
	if(extract) {
		for(DWORD i = 0; i < header.Count; ++i) {

			// compute destination path
			char fullpath[256];
			SetFullPath(fullpath, output_path, entries[i].Name);

			// extracting
			printf("Extracting '%s'...", fullpath);
			entries[i].Extract(f, fullpath);
			printf("done (%d bytes)\n", entries[i].Size);
		}
	}
	
	// print html table
	if(html) {
		PrintHTML(stdout, entries, header.Count);
	}

	exit_code = 0;
done_mem_release:
	delete[] entries;
	entries = nullptr;

done_close:
	// close file
	fclose(f);
	f = nullptr;

	return exit_code;
}
