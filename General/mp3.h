/*
 * mp3.h
 *
 *  Created on: Oct 26, 2010
 *      Author: timo
 */

#ifndef MP3_H_
#define MP3_H_


typedef enum {
	RESUME = 0,
	STOP,
	PAUSE,
	PLAY_PAUSE,
	NEXT_T,
	PREV_T,
	SEEK_F_8X,
	SEEK_F_2X,
	SEEK_R_8X,
	SEEK_R_2X
} MP3_CONTROL;


struct {
	unsigned int has_song : 1;  //if there's a song being played, may be paused.
	unsigned int playing : 1;   //if there's a song activly being played
	unsigned int playlist_pos;
	unsigned int position;      //data position in the track
	unsigned int legnth;        //what the data position is out of
	unsigned int read_speed;    //how fast we're reading the SD data
} player_status;

void mp3_task(void *pvParameters);




typedef struct Track {
	unsigned int id;
	char name[20];
	char filename[30];
	struct Track * next;
}Track;

typedef struct Artist {
	unsigned int id;
	char name[20];
	Track * tracks;
	struct Artist * next;
}Artist;

Artist * artist_list;
unsigned int num_of_artists;

void scan_root(void); //fills in artist_list and num_of_artists
Artist * get_artist_number(unsigned int a_num, Artist * artist_list);
Track * get_track_number(unsigned int tra_num, Track * track_list);

void display_track_list(Artist * llist);
void clear_track_list(void);
Track * bubblesort(Track *);

#endif /* MP3_H_ */
