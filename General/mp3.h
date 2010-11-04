/*
 * mp3.h
 *
 *  Created on: Oct 26, 2010
 *      Author: timo
 */

#ifndef MP3_H_
#define MP3_H_

#define STOP 1
#define PAUSE 2
#define RESUME 3
#define PLAY_PAUSE 4
#define SEEK_F_8X 5
#define SEEK_F_2X 6
#define SEEK_R_8X 7
#define SEEK_R_2X 8

struct {
	unsigned int has_song : 1;  //if there's a song being played, may be paused.
	unsigned int playing : 1;   //if there's a song activly being played
	unsigned int position;      //data position in the track
	unsigned int legnth;        //what the data position is out of
	unsigned int read_speed;    //how fast we're reading the SD data
} player_status;

void mp3_task(void *pvParameters);



#endif /* MP3_H_ */
