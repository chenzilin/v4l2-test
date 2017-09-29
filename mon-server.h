#ifndef __MON_SERVER_H__
#define __MON_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MONCMD_VIDEO			0
#define MONCMD_CANBUS			1
#define MONCMD_EXIT				0xffff

//video minor command0
#define VIDEOCMD_START			0	//data, struct rect
#define VIDEOCMD_PAUSE			1	//None data
#define VIDEOCMD_RUN			2	//None data
#define VIDEOCMD_STOP			3	//None data

//can minor command0 is channel
//can minor command1
#define CANBUS_START			0	//used only form client to server
#define CANBUS_DATA				1	//
#define CANBUS_FILTER			2	//used only form client to server
#define CANBUS_STOP				3	//used only form client to server

typedef struct {
	unsigned short major_cmd;
	unsigned char minor_cmd0;
	unsigned char minor_cmd1;
	unsigned int size;	//payload(*data) size
}MonServerData, *PMonServerData;

typedef struct {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
}rect_st, *Prect_st;

typedef struct{
	int channel;
	rect_st show_rect;
}Video_st, *PVideo_st;

typedef struct{
	int channel;
	rect_st show_rect;
	rect_st crop_rect;
}Video_st2, *PVideo_st2;

#ifdef __cplusplus
} /* extern "C"*/
#endif

#endif
