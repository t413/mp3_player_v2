
#ifndef USERINTERFACE_H_
#define USERINTERFACE_H_

#define MATCH(a,b)		(strcmp(a,b) == 0)
#define IS_SD_CARD_HERE (!(IOPIN0 & (1<<16)))

typedef struct {
	unsigned int id;
	char name[20];
}Artist;
#define Sizeof_all_artists 5
Artist all_artists[Sizeof_all_artists];

void sd_card_detect(void *pvParameters);
void uartUI(void *pvParameters);

#endif /* USERINTERFACE_H_ */


/*
else if(MATCH(command, "write")) {
	char *filename = strtok(0, "");
	rprintf("File to write: |%s|\n", filename);

	FIL file;
	if(FR_OK == f_open(&file, filename, (FA_WRITE | FA_CREATE_ALWAYS)))
	{
		rprintf("File successfully opened\n");
		char buff[512] = "File written using FATFS From Chen\n";

		unsigned int bytesWritten = 0;
		if(FR_OK != f_write(&file, buff, sizeof(buff), &bytesWritten))
		{
			rprintf("Failed to write file\n");
		}

		f_close(&file);
		rprintf("File is closed\n");
	}
	else
	{
		rprintf("Failed to open the file\n");
	}
}

FRESULT scan_files (char* path)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
	FIL file; //to open file and read mp3 id3 data
	char tag[40]; //to read mp3 id3 data to.
    int i;
    char *fn;
	#if _USE_LFN
    static char lfn[_MAX_LFN * (_DF1S ? 2 : 1) + 1];
    fno.lfname = lfn;
    fno.lfsize = sizeof(lfn);
	#endif

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
			if ((fno.fattrib & AM_HID) || (fno.fname[0] == '.')) continue;
			#if _USE_LFN
				fn = *fno.lfname ? fno.lfname : fno.fname;
			#else
				fn = fno.fname;
			#endif
            if (fno.fattrib & AM_DIR) {
                sprintf(&path[i], "/%s", fn);
                res = scan_files(path);
                if (res != FR_OK) break;
                path[i] = 0;
            } else {
				char* got_ext = strrchr(fn,'.');
				if ((got_ext != NULL) && (0 == strncmp(got_ext, ".mp3", 4)) ){
					sprintf(&path[i], "/%s", fn);  //get the whole file-path
					if (FR_OK == f_open(&file, path, (FA_READ | FA_OPEN_EXISTING)))
					read_ID3_info(TITLE_ID3,tag,sizeof(tag),&file);
					rprintf("found: %s",tag);
					read_ID3_info(ARTIST_ID3,tag,sizeof(tag),&file);
					rprintf(" by %s\n",tag);
					path[i] = 0;  //restore path to what it was before.
				}
            }
        }
    }

    return res;
}



*/
