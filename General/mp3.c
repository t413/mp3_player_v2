/*
 * mp3.c
 *
 *  Created on: Oct 26, 2010
 *      Author: timo
 */

#include "../osHandles.h"
#include "../fat/ff.h"				// FAT File System Library
#include "../fat/diskio.h" 			// Disk IO Initialize SPI Mutex
#include "../drivers/uart/uart0.h"  // uart0GetChar()
#include "../System/rprintf.h"      // rprintf
#include "./read_id3.h"
#include <string.h>
#include <malloc.h>                 // malloc, etc.
#include "mp3.h"

Artist * findArtist(char * artist_name);
FRESULT scan_files (char* path);


#define MP3_DATAREQ_IS_HIGH (IOPIN0 & (1<<15))
#define min(a,b) ((a>b)?(b):(a))


void mp3_task(void *pvParameters) {
	OSHANDLES *osHandles = (OSHANDLES*)pvParameters; // Cast the void pointer to pointer of OSHANDLES
		
	player_status.has_song = 0;
	player_status.playing = 0;

	vTaskDelay(100);
	scan_root();  //scan/populate database of artist & their albums.
	//artist_list->tracks = bubblesort(artist_list->tracks);

	for(;;)
	{
		Track * playlist;
		player_status.playlist_pos = 0;
		unsigned char cntl = RESUME;
		if (xQueueReceive(osHandles->queue.playback_playlist, &playlist,999999999)) {
			player_status.playlist_pos = 0;
			
			while (1) { //playlist loop
				Track * this_song = get_track_number(player_status.playlist_pos, playlist);
				
				if (this_song == NULL) {rprintf("get_track_number returned null."); break;} //past end of the playlist.
				
				rprintf("item(%i) track(%s) path(%s)",player_status.playlist_pos,this_song->name,this_song->filename);
				
				/* ---- check file's extention. ---- */
				char* got_ext = strrchr(this_song->filename,'.'); //get position of last '.'
				if ((got_ext == NULL) || (0 != strncmp(got_ext, ".MP3", 4)) ){
					rprintf("file not .mp3\n");
					break; // filename didn't have a .mp3 extention.
				}

				/* ---- open the file. ---- */
				FIL file;
				FRESULT rstat = f_open(&file, this_song->filename, (FA_READ | FA_OPEN_EXISTING));
				if (rstat != FR_OK) {
					rprintf("open error #%i (0x%02X).\n",rstat,rstat);
					f_close(&file);
					break;
				}
				/* ---- file opened successfully ---- */
				player_status.has_song = 1;
				player_status.playing = 1;
				//player_status.legnth = Finfo.fsize;
				
				player_status.position = 0;
				while (1){
					/* ---- check about the control queue ---- */
					if (xQueueReceive(osHandles->queue.mp3_control, &cntl,0)){
						if ((cntl == PAUSE) || (cntl == PLAY_PAUSE)) {
							player_status.playing = 0;
							if (xQueueReceive(osHandles->queue.mp3_control, &cntl,1000*60*5)) {}
							else {cntl = STOP; break;} //stop if past timeout (5 mins)
						}
						if ((cntl == NEXT_T)||(cntl == PREV_T)||(cntl == STOP)) break;  //breaks the playing loop..
					}
					
					if (cntl == SEEK_F_8X) { f_lseek(&file, file.fptr + 4096*8); player_status.position += 8; }
					if (cntl == SEEK_R_8X) { f_lseek(&file, file.fptr - 4096*8); player_status.position -= 8; }

					player_status.position += 1;
					/* ---- read a chunk of data to the buffer ---- */
					player_status.playing = 1;
					char buff[4096];
					unsigned int bytesRead = 0;
					unsigned int start_ms = xTaskGetTickCount();
					f_read(&file, buff, sizeof(buff), &bytesRead);
					player_status.read_speed = bytesRead/(xTaskGetTickCount()-start_ms);

					/* ---- send chunked data to the mp3 decoder ---- */
					int i = 0;
					while( i < bytesRead) {
						if (MP3_DATAREQ_IS_HIGH) {
							if (xSemaphoreTake( osHandles->lock.SPI, 100)){
								IOSET1 = (1<<29);  //MP3-decoder CS => active high
								rxTxByteSSPSPI(buff[i++]);
								IOCLR1 = (1<<29);  //MP3-decoder CS
								xSemaphoreGive( osHandles->lock.SPI );
							}
						}
					}
					if(sizeof(buff) > bytesRead){ // Last Chunk in file
						break; // Break outer loop, song ended
					}
				}
				f_close(&file);
				player_status.has_song = 0;
				player_status.playing = 0;
				
				//done playing the current playlist position, increment and continue.
				if (cntl == STOP) break;  //breaks the playlist loop..
				else if (cntl == PREV_T) { //previous / skip to beginning of track.
					if ((player_status.position < 50) && (player_status.playlist_pos > 0)){
						player_status.playlist_pos--;
					}
				}
				else if (cntl == NEXT_T) {player_status.playlist_pos++;}
				else {player_status.playlist_pos++;cntl = RESUME;}
				
			} //playlist loop
		} //xQueueReceive
	} //endless for loop. don't break; this.
}





void scan_root(void){
	if (artist_list != NULL) {rprintf("already scanned.\n");return;}
	char long_pathname[40] = "0:/MUSIC";
	rprintf("scanning 0:/MUSIC\n");
	scan_files(long_pathname);

	num_of_artists = 0;
	Artist * list_cpy = artist_list;
	while(list_cpy != NULL) {
		num_of_artists++;
		list_cpy = list_cpy->next;
	}
}	

FRESULT scan_files (char* path)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int i;
    char *fn;
	
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {
        	vTaskDelay(0);
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
			if ((fno.fattrib & AM_HID) || (fno.fname[0] == '.')) continue;
			fn = fno.fname;
            if (fno.fattrib & AM_DIR) {
				path[i]='/';  //sprintf(&path[i], "/%s", fn);  
				strcpy(&path[i+1],fn);  //get the whole file-path
                res = scan_files(path);
                if (res != FR_OK) break;
                path[i] = 0;
            } else {
				char* got_ext = strrchr(fn,'.');
				if ((got_ext != NULL) && (0 == strncmp(got_ext, ".MP3", 4)) ){
					FIL file; //to open file and read mp3 id3 data
					char tag[20]; //to read mp3 id3 data to.
					path[i]='/';  //sprintf(&path[i], "/%s", fn);  
					strcpy(&path[i+1],fn);  //get the whole file-path
					if (FR_OK == f_open(&file, path, (FA_READ | FA_OPEN_EXISTING)))
						
					/* ---- find/create artist node ---- */
					read_ID3_info(ARTIST_ID3,tag,sizeof(tag),&file);  //read artist-name
					//rprintf("(by %s) ",tag);
					Artist * this_artist = findArtist(tag); //search for this artist in the database.
					if (this_artist == NULL) {  //didn't find artist (hasn't been found before)
						this_artist = (Artist *)malloc(sizeof(Artist));
						strcpy(this_artist->name, tag);
						this_artist->tracks = NULL;
						this_artist->next = artist_list;
						artist_list = this_artist;
					}
					/* ---- create track node ---- */
					read_ID3_info(TITLE_ID3,tag,sizeof(tag),&file);  //read track-name
					Track * this_track = (Track *)malloc(sizeof(Track));
					strcpy(this_track->name, tag);
					strncpy(this_track->filename, path,30); //get the file path/name..
					this_track->next = this_artist->tracks;
					this_artist->tracks = this_track;
					//rprintf("-> %s\n",tag);
					
					
					path[i] = 0;  //restore path to what it was before.
				}
            }
        }
    } else rprintf("error opening [%i]\n",res);
	
    return res;
}

Artist * get_artist_number(unsigned int a_num, Artist * artist_list){
	unsigned int i = 0;
	while(artist_list != NULL) {
		if (i == a_num) return artist_list;
		artist_list = artist_list->next;
		++i;
	}
	return NULL;
}

Track * get_track_number(unsigned int tra_num, Track * track_list){
	unsigned int j = 0;
	while(track_list != NULL) {
		if (j == tra_num){
			return track_list;
		}
		track_list = track_list->next;
		++j;
	}
	return NULL;
}

Artist * findArtist(char * artist_name){
	Artist * list_cpy = artist_list;
	while(list_cpy != NULL) {
		if (strcmp(list_cpy->name,artist_name) == 0)
			return list_cpy;
		list_cpy = list_cpy->next;
	}
	return NULL;
}

void display_track_list(Artist * llist) {
	while(llist != NULL) {
		rprintf("%s \n",llist->name);
		Track * these_tracks = llist->tracks;
		while(these_tracks != NULL) {
			rprintf("  %s [%s] \n",these_tracks->name,these_tracks->filename);
			these_tracks = these_tracks->next;
		}
		llist = llist->next;
	}
}

void clear_track_list(void) {
	while(artist_list != NULL) {   //looping through each artist.
		while(artist_list->tracks != NULL) {   //looping through each track
			Track * track_tmp = artist_list->tracks;
			artist_list->tracks = artist_list->tracks->next;
			free(track_tmp);
		}
		Artist * artist_tmp = artist_list;
		artist_list = artist_list->next;
		free(artist_tmp);
	}
}

#if 1
Track * bubblesort(Track *head)
{
	int i, j, count = 0;
	Track *p0, *p1, *p2, *p3;
	
    //determine total number of nodes
    for (Track *tmp=head; tmp->next != NULL; tmp=tmp->next) { count++; }

	for(i = 1; i < count; i++)
	{
		p0 = NULL;
		p1 = head;
		p2 = head->next;
		p3 = p2->next;
		
		for(j = 1; j <= (count - i); j++)
		{
			if (strcmp(p1->name, p2->name)==0)  //if(p1->value > p2->value)
			{
				// Adjust the pointers...
				p1->next = p3;
				p2->next = p1;
				if(p0)p0->next=p2;
				
				
				// Set the head pointer if it was changed...
				if(head == p1)head=p2;
				
				// Progress the pointers
				p0 = p2;
				p2 = p1->next;
				p3 = p3->next;
			}
			else
			{
				// Nothing to swap, just progress the pointers...
				p0 = p1;
				p1 = p2;
				p2 = p3;
				p3 = p3->next;
			}
		}
	}
	return head;
}

#else
Track * bubblesort(Track * list)
{
    Track *tmp=list, *lst, *prev = NULL, *potentialprev = list;
    int i_outer, j_inner, n = 0;
	
    //determine total number of nodes
    for (; tmp->next != NULL; tmp=tmp->next) { n++; }
	
    for (i_outer=0; i_outer<n-1; i_outer++) 
    {
        for (j_inner=0,lst=list; lst && lst->next && (j_inner<=n-1-i_outer); j_inner++)
        {
            if (!j_inner)
            {
                //we are at beginning, so treat start 
                //node as prev node
                prev = lst;
            }
			
            //compare the two neighbors
            if ( strcmp ( lst->name, lst->next->name ) == 1 ) 
            {  
                //swap the nodes
                tmp = ((lst->next)? lst->next->next:0);
				
                if (!j_inner && (prev == list))
                {
                    //we do not have any special sentinal nodes
                    //so change beginning of the list to point 
                    //to the smallest swapped node
                    list = lst->next;
                }
                potentialprev = lst->next;
                prev->next = lst->next;
                lst->next->next = lst;
                lst->next = tmp;
                prev = potentialprev;
            }
            else
            {
                lst = lst->next;
                if(j_inner)
                {
                    //just keep track of previous node, 
                    //for swapping nodes this is required
                    prev = prev->next;
                }
            }     
        } 
    }
    return list;
}
#endif

