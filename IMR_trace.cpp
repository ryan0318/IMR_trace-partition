#include "IMR_trace.h"
using namespace std;
long long *map; //store pba
long long *pba_to_lba; //store lba
long long *track_write;
vector <Partition> partition;
fstream outfile;
int mode;
vector <int> mapping_cache;
long long hot_write_pos=1; //write possition
long long cold_write_pos = track_head(PARTITION_SIZE - 2);
int clean_buffer_count = 0,coldupdate=0;
int cw = 0;
long long clean_access = 0;
//long long cold_used_when_cleanbuf = 0, total_cold_when_cleanbuf = 0;
int last_part = 0;
//long long last_bot_write_time;///////////////used for testing
int switch_part_count = 0;
int load_map_count = 0;
int WriteBack_map_count = 0;
int main(int argc, char *argv[]) {
	if (argc != 3) {
		cout << "error: wrong argument number" << endl;
		exit(1);
	}//IMR_trace.exe test0713.trace 1 outfile.out
	
	create_map();
	
	runtrace(argv);
	
	cout << partition[partition.size() - 1].cold_direction << endl;

	/*if(partition[partition.size() - 1].cold_direction==1)
		for (int i = partition[partition.size() - 1].head + partition[partition.size() - 1].size - 1; i > partition[partition.size() - 1].head + partition[partition.size() - 1].hotsize
			+ BUFFER_SIZE; i--) {
			if (!isTop(track_head(i)) && track_write[i] != 0) {
				cout << "last bottom track: " << i << endl;
				break;
			}
		}
	else
		for (int i = partition[partition.size() - 1].head + partition[partition.size() - 1].hotsize
			+ BUFFER_SIZE; i < (partition[partition.size() - 1].head + partition[partition.size() - 1].size); i++) {
			if (!isTop(track_head(i)) && track_write[i] != 0) {
				cout << "last bottom track: " << i << endl;
				break;
			}
		}*/
	cout << "switch partition times: " << switch_part_count << endl; //////used for testing
	cout << "load partition map times: " << load_map_count << endl;
	cout << "write back partition map times: " << WriteBack_map_count << endl;
	cout << "cold write count: " << cw << endl;
	int avg=0;
	int hot_avg = 0;
	
	for(int i = 0; i < partition.size(); i++) {
		avg+=partition[i].size;
		hot_avg += partition[i].hotsize;
	}
	avg /= partition.size();
	hot_avg /= partition.size();
	cout << "total partitions: " << partition.size() << endl;
	cout << "avg partitions size: " << avg <<" tracks"<< endl;
	cout << "avg hot tracks size: " << hot_avg << " tracks" << endl;
	cout << "cold update times: " << coldupdate << endl;
	cout << "clean buffer times: " << clean_buffer_count << endl;
	cout << "access during clean buffer: " << clean_access << endl;

	double used = 0; 
	for (int i = 1; i < LBATOTAL + 1; i++) {
		if (map[i] != -1) {
			used++;
		}
	}
	double tn = LBATOTAL;
	double ratio = used / tn;
	cout << "total sector used: " << used << "/" << LBATOTAL << " " << ratio*100<<"%" << endl;
	used = 0;
	for (int i = 0; i < TRACK_NUM; i++) {
		if (track_write[i] != 0) {
			used++;
		}
	}
	tn = TRACK_NUM;
	ratio = used / tn;
	cout << "total tracks used: " << used << "/" << TRACK_NUM << " " << ratio * 100 << "%" << endl;
	cout << "Partition load_count" << endl;
		for (int i = 0; i < partition.size(); i++) {
			cout << "\t" << i << "\t" << partition[i].loaded_count << endl;
		}
/*	double a = cold_used_when_cleanbuf, b = total_cold_when_cleanbuf;
	cout << "avg cold tracks usage when clean buffer: " 
		<< cold_used_when_cleanbuf / clean_buffer_count << "/" << total_cold_when_cleanbuf / clean_buffer_count << " " << (a / b) * 100 << "%" << endl;*/
	exit(0);
}


void create_map()
{
	map = new long long[LBATOTAL+1];	//establish map
	pba_to_lba = new long long[LBATOTAL + 1];
	track_write = new long long[TRACK_NUM];	//establish track write record
	for (int i = 0; i < LBATOTAL + 1; i++) {
		map[i] = -1;
		pba_to_lba[i] = -1;
	}
	for (int i = 0; i < TRACK_NUM; i++) {
		track_write[i] = 0;
	}
	Partition temp;
	temp.buffer_map.resize((BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK));
	temp.head = 0;
	temp.size = PARTITION_SIZE;
	temp.hotsize = (PARTITION_SIZE - BUFFER_SIZE) / 10 * 4;
	if (temp.hotsize % 2 != 0) {			
		temp.hotsize += 1;
	}
	temp.buffer_pos = track_head(temp.hotsize);
	for (int i = 0; i < (BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK); i++) {
		temp.buffer_map[i] = -1;
	}
	partition.push_back(temp);
	
}

void runtrace(char **argv){
	
	fstream infile;
	
	infile.open(argv[1], ios::in);	//input file
	
	outfile.open(argv[2], ios::out);	//output file
	int p = 0;
	access temp;
	while (infile>>temp.time>>temp.iotype>>temp.address>>temp.size) {
		
		temp.time *= 1000;
		//outfile << "rt" << endl;////debug
		if (temp.iotype=='R'|| temp.iotype == '1') {	//it's read request
			read(temp);
		}
		else {	//it's write request
			temp.iotype = '0';
			if (temp.size > HOTDATA_MAXSIZE) {
				colddata_write(temp);
				cw++;
			}
			else {
				
				hotdata_write(temp);
			}
		}
	}
	
	infile.close();
	outfile.close();
}

void read(access request){	//may need more error handling
	//outfile << "rd" << endl;////debug
	request.iotype = '1';
	vector <access>reading;
	reading.push_back(request);
	reading[0].size = 0;
	if (get_pba(request.address) != -1)
		reading[0].address = get_pba(request.address);
	else
		reading[0].address = request.address;
	long long prev_pba = -1;
	long long target;
	for (int i = 0; i < request.size; i++) {
		if (get_pba(request.address + i) != -1)
			target = get_pba(request.address + i);
		else
			target = request.address + i;
		if (partition_no(track(target)) != last_part) {	//need to load diff partition's map track
			switch_part(request.time, partition_no(track(target)));
		}
		if ((target == (prev_pba + 1)) || i == 0) { //if target pba is sequential
			reading[reading.size() - 1].size++;
		}
		else {		//if target pba is not sequential
			reading.push_back(request);
			reading[reading.size() - 1].size = 1;
			reading[reading.size() - 1].address = target;
		}
		prev_pba = target;

	}

	for (int i = 0; i < reading.size(); i++) {	//print read trace
		writefile(reading[i]);
	}

}
void hotdata_write(access & request)
{
	//outfile << "hw" << endl;////debug
	int coldcounted = 0;
	vector <access>result;
	access write_temp = request;
	access read_temp = request;
	read_temp.iotype = '1';
	
	long long pba;
	long long prev_pba = -1;
	long long prev_pba2 = -1;
	long long prev_pba3 = -1;
	//outfile << "hw2" << endl;////debug
	Partition new_part;
	new_part.buffer_map.resize((BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK));
	//outfile << "hw3" << endl;////debug
	for (int i = 0; i < request.size; i++) {
		pba = get_pba(request.address + i);
		if (pba == -1) {//if pba didn't already exist (is not an update request)
			if (partition_no(track(hot_write_pos)) != last_part) {	//need to load diff partition's map track
				switch_part(request.time, partition_no(track(hot_write_pos)));
			}

			write_temp.address = hot_write_pos;
			write_temp.size = 1;
			result.push_back(write_temp);
			track_write[track(hot_write_pos)] = 1;
			
			map[request.address + i] = hot_write_pos;	//update mapping
			pba_to_lba[hot_write_pos] = request.address + i;
			if (track(hot_write_pos) != track(hot_write_pos + 1)) {
				hot_write_pos += SECTORS_PER_TOPTRACK;
			}
			hot_write_pos++;
			if (track(hot_write_pos) > 
				partition[partition.size()-1].head + partition[partition.size()-1].hotsize - 2) {	
				//create a new partition
				new_part.head = partition[partition.size()-1].head + partition[partition.size()-1].size;
				new_part.hotsize = partition[partition.size()-1].hotsize * 2; //change the hot tracks number
				if (new_part.hotsize > (PARTITION_SIZE*0.8))
					new_part.hotsize = PARTITION_SIZE * 0.8;
				if (new_part.hotsize % 2 != 0) {			
					new_part.hotsize += 1;
				}
				new_part.size = PARTITION_SIZE;
				new_part.buffer_pos = track_head(new_part.head + new_part.hotsize);
				for (int i = 0; i < (BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK); i++) {
					new_part.buffer_map[i] = -1;
				}
				partition.push_back(new_part);
				hot_write_pos = track_head(new_part.head);
				cold_write_pos = track_head(new_part.head + new_part.size - 2); //colddata write from the last bottom track of the partition
				switch_part(request.time, partition.size() - 1);
			}
		}
		else if (track(pba) < partition[partition_no(track(pba))].head +
			partition[partition_no(track(pba))].hotsize) {		//it's an hot data update
			if (partition_no(track(pba)) != last_part) {	//need to load diff partition's map track
				switch_part(request.time, partition_no(track(pba)));
			}
			if (track(pba) == 0) {
				if (track_write[track(pba) + 1] == 0) {
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
			}
			else if (isTop(pba) || (track_write[track(pba) + 1] == 0 && track_write[track(pba) - 1] == 0 && !isTop(pba))) {
				write_temp.address = pba;
				write_temp.size = 1;
				result.push_back(write_temp);
			}
			else {
				/*Do InPace Update (shouldn't happen)*/
			}
		}
		else {	//it's an cold data update
			if (coldcounted == 0) {
				coldupdate++;
				coldcounted = 1;
			}
			
			long long curr_part = partition_no(track(pba));
			if (curr_part != last_part) {	//need to load diff partition's map track
				switch_part(request.time, curr_part);
			}
			if (track(pba) < partition[curr_part].head + partition[curr_part].hotsize + BUFFER_SIZE) {
				//if itself is a buffer update, do in-place update
				write_temp.address = pba;
				write_temp.size = 1;
				result.push_back(write_temp);
			}
			else {	//not a buffer update
				if (isTop(pba)) {		//in place update top tracks
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
				else {	//bottom update

					if (!partition[curr_part].buffer_full) {	// if buffer is available
						write_temp.address = partition[curr_part].buffer_pos;
						write_temp.size = 1;
						result.push_back(write_temp);
						track_write[track(partition[curr_part].buffer_pos)] = 1;

						map[request.address + i] = partition[curr_part].buffer_pos;	//update mapping
						pba_to_lba[partition[curr_part].buffer_pos] = request.address + i;
						partition[curr_part].buffer_map[ partition[curr_part].buffer_pos -	//store original location
							track_head(partition[curr_part].head + partition[curr_part].hotsize) ] = pba;
						/*
						//if itself is a buffer update, invalid the old buffer
						if (track(pba) < partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize + BUFFER_SIZE) {

							partition[curr_part].buffer_map[ partition[curr_part].buffer_pos -	//store old original location
								track_head(partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize) ] =
								partition[curr_part].buffer_map[pba - track_head(partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize)];

							partition[curr_part].buffer_map[pba - track_head(partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize)] = -1;
						}*/

						if (track(partition[curr_part].buffer_pos + 1) !=	//moving buffer_pos
							track(partition[curr_part].buffer_pos)) {
							partition[curr_part].buffer_pos = track_head(track(partition[curr_part].buffer_pos) + 2);

							if (track(partition[curr_part].buffer_pos) >= partition[curr_part].head +
								partition[curr_part].hotsize + partition[curr_part].mapsize + BUFFER_SIZE) {
								partition[curr_part].buffer_full = true;

								clean_buffer(curr_part, write_temp);
							}
						}
						else {
							partition[partition_no(track(pba))].buffer_pos++;
						}
					}
					else {	//buffer full

						clean_buffer(curr_part, write_temp);
					}
				}
			}
		}
	}
	
	vector <access>output;
	
	output.push_back(result[0]);
	
	prev_pba = result[0].address;
	
	for (int i = 1; i < result.size(); i++) {
		
		if (result[i].address == (prev_pba + 1) && track(result[i].address) == track(prev_pba)) {
			output[output.size() - 1].size++;		//if pba is sequential to its previous write pba
		}
		else {	//if pba is not sequential
			write_temp.address = result[i].address;
			write_temp.size = result[i].size;
			write_temp.iotype = '0';
			output.push_back(write_temp);
		}
		prev_pba = result[i].address;
	}
	
	for (int i = 0; i < output.size(); i++) {
		writefile(output[i]);
	}
}

void colddata_write(access & request)
{
	//outfile << "cw" << endl;////debug
	vector <access>result;
	access write_temp = request;
	access read_temp = request;
	read_temp.iotype = '1';
	long long pba;
	long long prev_pba = -1;
	long long prev_pba2 = -1;
	int coldcounted = 0;
	Partition new_part;
	new_part.buffer_map.resize((BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK));
	for (int i = 0; i < request.size; i++) {
		pba = get_pba(request.address + i);
		if (pba == -1) {//if pba didn't already exist (is not an update request)
			if (partition_no(track(cold_write_pos)) != last_part) {	//need to load diff partition's map track
				switch_part(request.time, partition_no(track(cold_write_pos)));
			}
			
			write_temp.address = cold_write_pos;
			write_temp.size = 1;
			result.push_back(write_temp);
			track_write[track(cold_write_pos)] = 1;
		/*	if (!isTop(cold_write_pos)) {                ///////////used for testing
				last_bot_write_time = write_temp.time;
			}*/
			map[request.address + i] = cold_write_pos;	//update mapping
			pba_to_lba[cold_write_pos] = request.address + i;
			if (track(cold_write_pos) != track(cold_write_pos + 1)) {
				partition[partition.size() - 1].cold_used += 1;
				if (partition[partition.size()-1].cold_direction == 0) {
					if (isTop(cold_write_pos)) {
						cold_write_pos = track_head(track(cold_write_pos) - 3);
					}				/*moving cold_write_pos */
					else {
						if (track(cold_write_pos) == partition[partition.size()-1].head + PARTITION_SIZE - 2) {
							cold_write_pos = track_head(track(cold_write_pos) - 2);
							
						}
						else {		//move to next top track
							cold_write_pos++;	
							if (track(cold_write_pos) == partition[partition.size() - 1].head + PARTITION_SIZE - 3) {
								//can't write map track
								cold_write_pos = track_head(track(cold_write_pos) - 3);
							}
						}
					}
					//check if cold tracks is full
					if (track(cold_write_pos) < partition[partition.size()-1].head +
						partition[partition.size()-1].hotsize + BUFFER_SIZE + 1) {	//limit is the leftest cold bottom track 
						if (partition[partition.size()-1].size < MAX_PARTITION_SIZE) {
							partition[partition.size()-1].cold_direction = 1;	//start expand
							partition[partition.size()-1].size += 2;
							cold_write_pos = track_head(partition[partition.size()-1].head + partition[partition.size() - 1].size - 2);

						}
					}

				}
				else {	
					if (isTop(cold_write_pos)) {
						if (partition[partition.size()-1].size < MAX_PARTITION_SIZE) {	//keep expanding
							partition[partition.size()-1].size += 2;
							cold_write_pos = track_head(partition[partition.size()-1].head + partition[partition.size()-1].size - 2);
						}
						else {	//create new partition
							new_part.head = partition[partition.size()-1].head + partition[partition.size()-1].size;
							new_part.hotsize = partition[partition.size()-1].hotsize / 2; //change the hot tracks number
							if (new_part.hotsize % 2 != 0) {
								new_part.hotsize += 1;
							}
							new_part.size = PARTITION_SIZE;
							new_part.buffer_pos = track_head(new_part.head + new_part.hotsize);
							for (int i = 0; i < (BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK); i++) {
								new_part.buffer_map[i] = -1;
							}
							partition.push_back(new_part);
							hot_write_pos = track_head(new_part.head);
							cold_write_pos = track_head(new_part.head + new_part.size - 2); //colddata write from last bottom of the partition
							switch_part(request.time, partition.size() - 1);
						}
						
					}
					else {
						cold_write_pos = track_head(track(cold_write_pos) - 1);
					}
					
				}
				
			}
			else {
				cold_write_pos++;
				
			}
			
		}
		else if (track(pba) < partition[partition_no(track(pba))].head +
			partition[partition_no(track(pba))].hotsize) {		//it's an hot data update
			if (partition_no(track(pba)) != last_part) {	//need to load diff partition's map track
				switch_part(request.time, partition_no(track(pba)));
			}
			if (track(pba) == 0) {
				if (track_write[track(pba) + 1] == 0) {
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
			}
			else if (isTop(pba) || (track_write[track(pba) + 1] == 0 && track_write[track(pba) - 1] == 0 && !isTop(pba))) {
				write_temp.address = pba;
				write_temp.size = 1;
				result.push_back(write_temp);
			}
			else {
				/*Do InPace Update (shouldn't happen)*/
			}
		}
		else {	//it's an cold data update
			if (coldcounted == 0) {
				coldupdate++;
				coldcounted = 1;
			}

			long long curr_part = partition_no(track(pba));
			if (curr_part != last_part) {	//need to load diff partition's map track
				switch_part(request.time, curr_part);
			}
			if (track(pba) < partition[curr_part].head + partition[curr_part].hotsize + BUFFER_SIZE) {
				//if itself is a buffer update, do in-place update
				write_temp.address = pba;
				write_temp.size = 1;
				result.push_back(write_temp);
			}
			else {	//not a buffer update
				if (isTop(pba)) {		//in place update top tracks
					write_temp.address = pba;
					write_temp.size = 1;
					result.push_back(write_temp);
				}
				else {	//bottom update

					if (!partition[curr_part].buffer_full) {	// if buffer is available
						write_temp.address = partition[curr_part].buffer_pos;
						write_temp.size = 1;
						result.push_back(write_temp);
						track_write[track(partition[curr_part].buffer_pos)] = 1;

						map[request.address + i] = partition[curr_part].buffer_pos;	//update mapping
						pba_to_lba[partition[curr_part].buffer_pos] = request.address + i;
						partition[curr_part].buffer_map[partition[curr_part].buffer_pos -	//store original location
							track_head(partition[curr_part].head + partition[curr_part].hotsize)] = pba;
						/*
						//if itself is a buffer update, invalid the old buffer
						if (track(pba) < partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize + BUFFER_SIZE) {

							partition[curr_part].buffer_map[ partition[curr_part].buffer_pos -	//store old original location
								track_head(partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize) ] =
								partition[curr_part].buffer_map[pba - track_head(partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize)];

							partition[curr_part].buffer_map[pba - track_head(partition[curr_part].head + partition[curr_part].hotsize + partition[curr_part].mapsize)] = -1;
						}*/

						if (track(partition[curr_part].buffer_pos + 1) !=	//moving buffer_pos
							track(partition[curr_part].buffer_pos)) {
							partition[curr_part].buffer_pos = track_head(track(partition[curr_part].buffer_pos) + 2);

							if (track(partition[curr_part].buffer_pos) >= partition[curr_part].head +
								partition[curr_part].hotsize + partition[curr_part].mapsize + BUFFER_SIZE) {
								partition[curr_part].buffer_full = true;

								clean_buffer(curr_part, write_temp);
							}
						}
						else {
							partition[partition_no(track(pba))].buffer_pos++;
						}
					}
					else {	//buffer full

						clean_buffer(curr_part, write_temp);
					}
				}
			}
		}
	}
	
	vector <access>output;
	output.push_back(result[0]);
	prev_pba = result[0].address;
//	cout << result[0].address << "'s track_no: " << track(result[0].address) << endl;
	for (int i = 1; i < result.size(); i++) {
		
		if (result[i].address == (prev_pba + 1) && track(result[i].address) == track(prev_pba)) {
			output[output.size() - 1].size++;		//if pba is sequential to its previous write pba
		}
		else {	//if pba is not sequential
//			cout << prev_pba << "'s track_no: " << track(prev_pba) << endl;
			write_temp.address = result[i].address;
			write_temp.size = result[i].size;
			write_temp.iotype = '0';
			output.push_back(write_temp);
		}
		prev_pba = result[i].address;
	}

	for (int i = 0; i < output.size(); i++) {
		writefile(output[i]);
	}
}

void clean_buffer(long long curr_part,access write_temp)
{
	//outfile << "cb" << endl;////debug
	clean_buffer_count++;
	/*cold_used_when_cleanbuf += partition[curr_part].cold_used;
	total_cold_when_cleanbuf += partition[curr_part].size - partition[curr_part].hotsize - partition[curr_part].mapsize - BUFFER_SIZE;*/
/*	float a = partition[curr_part].cold_used, b = partition[curr_part].size - partition[curr_part].hotsize - partition[curr_part].mapsize - BUFFER_SIZE;
	float usage = a / b * 100;
	cout << "Partition " << curr_part << "'s Cold Tracks Usage: "
		<< partition[curr_part].cold_used << "/" << partition[curr_part].size - partition[curr_part].hotsize - partition[curr_part].mapsize - BUFFER_SIZE
		<< " " << usage << "%" << endl;*/
	vector <access>result;
	access read_temp = write_temp;
	read_temp.iotype = '1';
	int seq_size;
	long long prev,target; //target is buffer's original pba
	long long lba;
	long long segments;	//can change it to int
	for (int i = 0; i < (BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK);) {
		/*calculating size of a sequential data*/
		prev = partition[curr_part].buffer_map[i];
		target = partition[curr_part].buffer_map[i];	//target is buffer's original pba
		if (target != -1) {
			for (seq_size = 1; i + seq_size < (BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK); seq_size++) {
				if ((partition[curr_part].buffer_map[i + seq_size] != (prev + 1)) ||
					track(partition[curr_part].buffer_map[i + seq_size]) != track(prev) ||
					partition[curr_part].buffer_map[i + seq_size] == -1 ||
					seq_size >= SECTORS_PER_TOPTRACK) {
					break;
				}
				prev = partition[curr_part].buffer_map[i + seq_size];
			}
			//---------------

			//read from buffer
			read_temp.address = track_head(partition[curr_part].head + partition[curr_part].hotsize) + i;
			read_temp.size = seq_size;
			result.push_back(read_temp);

			//read top tracks
			if (track_write[track(target) - 1] != 0) {
				read_temp.address = track_head(track(target) - 1);
				read_temp.size = SECTORS_PER_TOPTRACK;
				result.push_back(read_temp);
			}
			if (track_write[track(target) + 1] != 0) {
				read_temp.address = track_head(track(target) + 1);
				read_temp.size = SECTORS_PER_TOPTRACK;
				result.push_back(read_temp);
			}
			//read target segment
			read_temp.address = track_head(track(target)) + ((target - track_head(track(target))) / SEGMENT_SIZE)*SEGMENT_SIZE;
			segments = (target + seq_size - read_temp.address) / SEGMENT_SIZE + ((target + seq_size - read_temp.address) % SEGMENT_SIZE != 0);//Calculating how many segments needed
			if (segments == 0) segments += 1;//new fix 20/12/21
			read_temp.size = segments * SEGMENT_SIZE;
			result.push_back(read_temp);
		
			//write top track's segments to bottom tracks
			if (track_write[track(target) + 1] != 0 ||
				(track(target) + 1 >= partition[curr_part].head + partition[curr_part].size - 1 && track_write[track(target) - 1] != 0)) { //last cold tracks can't be written
				write_temp.address = read_temp.address;
				write_temp.size = segments * SEGMENT_SIZE;
				result.push_back(write_temp);

			}
			//write back top tracks
			if (track_write[track(target) - 1] != 0) {
				write_temp.address = track_head(track(target) - 1);
				write_temp.size = SECTORS_PER_TOPTRACK;
				result.push_back(write_temp);
			}
			if (track_write[track(target) + 1] != 0) {
				write_temp.address = track_head(track(target) + 1);
				write_temp.size = SECTORS_PER_TOPTRACK;
				result.push_back(write_temp);
			}

			//write segments from bottom tracks into top tracks
			if (track(target) + 1 < partition[curr_part].head + partition[curr_part].size - 1) { //last cold tracks can't be written
				write_temp.address = track_head(track(target) + 1);
				write_temp.size = segments * SEGMENT_SIZE;
				result.push_back(write_temp);

				//update mapping
				for (int j = 0; j < write_temp.size; j++) {
					//move segments from top to bottom
					lba = pba_to_lba[write_temp.address + j];
					map[lba] = read_temp.address + j;
					pba_to_lba[read_temp.address + j] = lba;

				/*	//move segments from bottom to top
					lba = pba_to_lba[read_temp.address + j];
					map[lba] = write_temp.address + j;
					pba_to_lba[write_temp.address + j] = lba;*/
				}
				//update mapping of sectors from buffer
				for (int j = 0; j < seq_size; j++) {
					lba = pba_to_lba[track_head(partition[curr_part].head + partition[curr_part].hotsize) + i + j];
					map[lba] = target + j - read_temp.address + write_temp.address;
					pba_to_lba[map[lba]] = lba;
				}
			}
			else {
				write_temp.address = track_head(track(target) - 1);
				write_temp.size = segments * SEGMENT_SIZE;
				result.push_back(write_temp);

				//update mapping
				for (int j = 0; j < write_temp.size; j++) {
					//move segments from top to bottom
					lba = pba_to_lba[write_temp.address + j];
					map[lba] = read_temp.address + j;
					pba_to_lba[read_temp.address + j] = lba;

					//move segments from bottom to top
				/*	lba = pba_to_lba[read_temp.address + j];
					map[lba] = write_temp.address + j;
					pba_to_lba[write_temp.address + j] = lba;*/
				}
				//update mapping of sectors from buffer
				for (int j = 0; j < seq_size; j++) {
					lba = pba_to_lba[track_head(partition[curr_part].head + partition[curr_part].hotsize) + i + j];
					map[lba] = target + j - read_temp.address + write_temp.address;
					pba_to_lba[map[lba]] = lba;
				}
			}

			i += seq_size;
		}
		else {
			i += 1;
		}
	}
	partition[curr_part].buffer_full = false;
	partition[curr_part].buffer_pos = track_head(partition[curr_part].head + partition[curr_part].hotsize);
	
	for (int i = 0; i < (BUFFER_SIZE - BUFFER_SIZE / 2) * SECTORS_PER_BOTTRACK + (BUFFER_SIZE / 2 * SECTORS_PER_TOPTRACK); i++) {
		partition[curr_part].buffer_map[i] = -1;
	}
	clean_access += result.size();
	for (int i = 0; i < result.size(); i++) {
	//	if (result[i].size == 0)cout << "wrong:¡@" << result[i].time << endl;////////////////debug
		writefile(result[i]);
	}
}

void switch_part(long long time, int newpart)
{
	bool check = false;
	switch_part_count++;
	for (int i = 0; i < mapping_cache.size(); i++) {
		if (mapping_cache[i] == newpart) {
			check = true;
			break;
		}
	}
	if (check == false) {
		//outfile << "sp" << endl;////debug
		
		access temp;

		if (mapping_cache.size() >= MAPPING_CACHE_SIZE) {
			//write map back into old partition's map track
			temp.address = track_head(partition[mapping_cache[0]].head + PARTITION_SIZE - 3);
			temp.iotype = '0';
			temp.size = SECTORS_PER_TOPTRACK;
			temp.time = time;
			writefile(temp);
			WriteBack_map_count++;
			mapping_cache.erase(mapping_cache.begin());
		}
		//read new partition's map track
		temp.address = track_head(partition[newpart].head + PARTITION_SIZE - 3);
		temp.iotype = '1';
		temp.size = SECTORS_PER_TOPTRACK;
		temp.time = time;
		writefile(temp);
		load_map_count++;
		partition[newpart].loaded_count++;

		mapping_cache.push_back(newpart);

	}
	last_part = newpart;
}


void writefile(access Access){
	//outfile << "wf" << endl;////debug
	outfile << Access.time << "\t" << Access.device << "\t"
		<< Access.address << "\t" << Access.size << "\t" << Access.iotype << endl;
}

long long partition_no(long long track)
{

	long long N = 0;
	long long total = 0;
	for (N = 0; N < partition.size(); N++) {
		if (track >= total && track < (total+partition[N].size))
			return N;
		total += partition[N].size;
	}
	return 0;
}

long long get_pba(long long lba)
{
	return map[lba];
}

long long track(long long pba)
{		//Track start from 0, LBA start from 1
	if (pba == -1)
		return -1;
	
	long long n = pba / (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK);
	if (pba - n * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK) > SECTORS_PER_BOTTRACK)
		return (2 * n + 1);
	else if (pba - n * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK) > 0)
		return 2 * n;
	else
		return (2 * n - 1);
	
}

long long track_head(long long t)
{						//LBA start from 1

	if (t % 2 != 0) {	//if isTop
		return (t / 2 * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK)) + SECTORS_PER_BOTTRACK + 1;
	}
	else {
		return (t / 2 * (SECTORS_PER_BOTTRACK + SECTORS_PER_TOPTRACK)) + 1;
	}
	
}

bool isTop(long long pba)
{
	if (pba == -1)
		return false;
	if (track(pba) % 2 != 0)
		return true;
	return false;
}

