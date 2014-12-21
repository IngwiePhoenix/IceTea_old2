#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>

using namespace std;

void help(char* me) {
	cout << "file2c by IngwiePhoenix | Convert a file into a C source file, that is CPP aware." << endl;
	cout << endl;
	cout << "Usage: " << me << " inputFile varName > output" << endl;
}

int main(int argc, char* argv[]) {
	if(argc != 3) { help(argv[0]); exit(1); }
	else {
    	char* fn = argv[1];
    	char* varName = argv[2];
    	FILE* f = fopen(fn, "rb");
    	if(!f) { cerr << "Error while opening file." << endl; exit(1); }
    	
    	// Get filesize...
    	int filesize;
 		struct stat stat_buf;
    	int rc = stat(fn, &stat_buf);
    	filesize = stat_buf.st_size;    	
    	
    	printf("char %s[] = {\n    ",varName);
    	unsigned long n = 0;
    	while(!feof(f)) {
        	unsigned char c;
        	if(fread(&c, 1, 1, f) == 0) break;
        	++n;
        	if((int)n == filesize) {
        		printf("0x%.2X", (int)c);
        	} else {
        		printf("0x%.2X, ", (int)c);
        	}
        	if(n % 20 == 0) printf("\n    ");
    	}
    	fclose(f);
    	printf("\n};\n");
    	printf("int %s_length=%i;\n",varName,filesize);
    }
    return 0;
}
