// header for packet structs

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <numeric>
#include <cmath>
#include <sstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <mutex>
#include <algorithm>

#include <sys/types.h> 
#define REC_WINDOW (66)
#define MAX_BUFF (1024)

typedef struct __attribute__((__packed__))
{
	unsigned int seq_num;
	unsigned int ack_num;
	unsigned rst : 1;
	unsigned syn : 1;
	unsigned fin : 1;
	unsigned ack : 1;
	unsigned padding : 4;
	short unsigned window;
	short unsigned buf_size;
	char data[MAX_BUFF];
}packet;

//sleep timer
bool timeup=false;
unsigned int acknum=0;
std::mutex ack_mut;
void recvThread(int s, sockaddr* si, socklen_t si_size, uint32_t mill_timeout, unsigned int base){
	packet in = {0};
	struct timeval read_timeout, start, stop;
	bool first = true;
	int oldacks = 0;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 1000*mill_timeout;
	gettimeofday(&start, NULL);
	do{
		if(first){
			setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
			first = false;
		}else{
			read_timeout.tv_sec = 0;
			read_timeout.tv_usec -= (stop.tv_usec - start.tv_usec);
			setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
		}
		if( recvfrom(s, &in, sizeof(packet), 0, si, &si_size) > 0 ) {
			ack_mut.lock();
			if(in.ack_num > base && in.ack){
				acknum = in.ack_num;
				timeup = false;
				ack_mut.unlock();
				return;
			}else{
				std::cout<<"got old ack"<<std::endl;
				if(++oldacks == 3){
					acknum = 0;
					timeup = true;
					ack_mut.unlock();
					return;
				}
				ack_mut.unlock();
			}
		}else{
			std::cout<<"timed out on recv"<<std::endl;
			ack_mut.lock();
			acknum = 0;
			timeup = true;
			ack_mut.unlock();
		}
		gettimeofday(&stop, NULL);
	}while( (stop.tv_usec - start.tv_usec)*1000 < mill_timeout);
	ack_mut.lock();
	acknum = 0;
	timeup = true;
	ack_mut.unlock();
	return;
}