#ifndef IMR_TRACE_H
#define IMR_TRACE_H
//IMR_trace_systor17_4_compact10.h
//IMR_trace_sys17_2_new 72602933/90560800 used sectors 46501 tracks
//IMR_trace_sys17_4_new 103219550 sectors 53001 tracks
#define LBATOTAL 103219550
#define TRACK_NUM 53001
#define SECTORS_PER_BOTTRACK 2050
#define SECTORS_PER_TOPTRACK 1845
#define SEGMENT_SIZE 41
#define TOTAL_BOTTRACK	26500
#define TOTAL_TOPTRACK	26501
//#define TOPTRACK_PBA 54925399	//pba start from 1, TOPTRACK_PBA is first address of top tracks
#define SEQUENTIAL_IN_PLACE 1
#define SEQUENTIAL_OUT_PLACE 2
#define CROSSTRACK_IN_PLACE 3
#define CROSSTRACK_OUT_PLACE 4
#define HOTDATA_MAXSIZE 64	//in sectors 32KB
#define PARTITION_SIZE 500	//default partition size, in tracks
#define MAX_PARTITION_SIZE 2000
#define BUFFER_SIZE 21	//tracks
#define MAPPING_CACHE_SIZE 10
#include<iostream>
#include<fstream>
#include<stdlib.h>
#include<vector>
#include<string>
#include<math.h>

struct access {
	long long time;
	char iotype;
	long long address;  //lba, may be pba when write file
	int size;
	int device = 0;
};

struct Partition {
	long long head;	//in tracks
	int size;	//in tracks
	int hotsize; // in tracks
	int mapsize = 1; //in tracks
	int buffer_pos; //in sectors
	bool buffer_full = false;
	int cold_direction = 0;
	int loaded_count = 0;
	//use int for now
	//int *buffer_map = new int[(BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK)];  //(BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK)
	std::vector<long long> buffer_map;
	int cold_used = 0; // in tracks
};

void create_map();
void runtrace(char **argv);
void read(access request);
void hotdata_write(access &request);
void colddata_write(access &request);
void clean_buffer(long long curr_part,access write_temp);
void switch_part(long long time, int newpart);
//void seqtrack_write(access &request, int mode);
//void crosstrack_write(access &request, int mode);
void writefile(access Access);
long long partition_no(long long track);
long long get_pba(long long lba);
long long track(long long pba); //return which track is the pba on
long long track_head(long long t); //t is track
//bool isUpdate(access request);
bool isTop(long long pba);

#endif