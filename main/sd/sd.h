#pragma once 


 void sdcard_init(void) ; 


 #define MAX_FILES       100
#define MAX_FILENAME    50

char (*get_filtered_files(void))[MAX_FILENAME] ; 
int scan_files_by_extension(const char *suffix) ; 