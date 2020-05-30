/* 
 * File:   sender_main.cpp
 * Author: 
 *
 * Created on 
 */

#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
//#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
//#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
// #include <string.h>
#include <math.h>
#include <inttypes.h>
// #include <iostream.h>
//for the timing
#include "packet.hpp"

struct sockaddr_in si_other;
int s, slen;

void diep(const char* s) {
	perror(s);
	exit(1);
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	/* Determine how many bytes to transfer */
	slen = sizeof (si_other);
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		diep((char*) "socket");

	memset((char *) &si_other, 0, sizeof (si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(hostUDPport);
	if (inet_aton(hostname, &si_other.sin_addr) == 0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	// set up file steam
	std::ifstream fp(filename, std::ifstream::binary);
	if (!fp){
		printf("Could not open file to send.");
		exit(1);
	}
	fp.seekg(0, fp.end);
	//file_len will either be bytes to trans for the length of the file
	unsigned long long int file_len = std::min((unsigned long long int)fp.tellg(), bytesToTransfer);
	printf("file length is: %d", file_len);
	fp.seekg(0, fp.beg);

	/* Send data and receive acknowledgements on s*/
	socklen_t si_other_size = sizeof(si_other);
	packet* in = (packet*)calloc(1, sizeof(packet));
	packet out = {};
	int seq_num = 0;
	out.seq_num = seq_num;
	out.syn = 1;
	std::cout<<"send syn packet"<<std::endl;
	sendto(s, &out, sizeof(packet), 0, (struct sockaddr *)&si_other, sizeof(si_other));
	std::cout<<"getting syn ack"<<std::endl;
	recvfrom(s, in, sizeof(packet), 0, (struct sockaddr *)&si_other, &si_other_size);
	int reciverWindow = in->window;
	int ssthresh = reciverWindow/2;
	if(in->ack != 1 || in->ack_num != seq_num+1)
		return;
	memset(&out, 0, sizeof(packet));
	out.ack = 1;
	out.ack_num = in->seq_num +1;
	out.buf_size = 0;
	std::cout<<"send ack"<<std::endl;
	sendto(s, &out, sizeof(packet), 0, (struct sockaddr *)&si_other, sizeof(si_other));
	std::cout<<"sent ack handshake complete"<<std::endl;

	// start actually sending data...
	std::vector<packet> window;
	int windowMaxSize = 1; // initial size of window
	window.clear();
	unsigned long long int sent_out = 0;
	seq_num = 1;
	packet data_pack = {};

	// send packets until done or window is maxed out
	bool make_thread = true;
	std::thread th0;
	uint32_t timeout = 1;
	while ( 1 ){
		if( (sent_out < file_len) && (window.size() <= windowMaxSize) ){
			memset(&data_pack, 0, sizeof(packet));
			fp.read(data_pack.data, (MAX_BUFF<(file_len-sent_out))? MAX_BUFF:file_len-sent_out);
			data_pack.buf_size = fp.gcount();
			data_pack.seq_num = seq_num;
			printf("I'm sending seq_num: %d\n", seq_num);
			if( (unsigned long long int) (sent_out += data_pack.buf_size) >= file_len){
				data_pack.fin = 1;
				std::cout<<"sent final packet"<<std::endl;
			}
			sendto(s, &data_pack, sizeof(data_pack), 0, (struct sockaddr *)&si_other, sizeof(si_other));
			seq_num += 1;
			window.push_back(data_pack);
			if(make_thread){
				std::cout<<"thread made"<<std::endl;
				unsigned int base = window[0].seq_num;
				th0 = std::thread( recvThread, s, (struct sockaddr *)&si_other, sizeof(si_other), timeout, base );
				make_thread = false;
			}
		}
		ack_mut.lock();
		if(timeup){
			std::cout<<"retransmiting on timeout"<<std::endl;
			for(auto& pac :  window){
				sendto( s, &pac, sizeof(pac), 0,  (struct sockaddr *)&si_other, sizeof(si_other) );
			}
			timeup = false;
			ssthresh = windowMaxSize/2;
			windowMaxSize = 1;
		}else if(acknum != 0){
			//remove elements up to acknum
			int numToRm = acknum - window[0].seq_num;
			acknum = 0;
			window.erase(window.begin(), window.begin()+numToRm);
			windowMaxSize = std::min(reciverWindow, (windowMaxSize < ssthresh) ? windowMaxSize*2 : windowMaxSize+1);
		}
		ack_mut.unlock();
		if(th0.joinable()){
			th0.join();
			make_thread = true;
			std::cout<<"thread joined"<<std::endl;
		}
		if( window.size() == 0 && sent_out == file_len)
			break;
	}
	std::cout<<windowMaxSize<<std::endl;
	printf("Closing the socket\n");
	close(s);
	return;

}

/*
 * 
 */
int main(int argc, char** argv) {

	unsigned short int udpPort;
	unsigned long long int numBytes;

	if (argc != 5) {
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	udpPort = (unsigned short int) atoi(argv[2]);
	numBytes = atoll(argv[4]);



	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


	return (EXIT_SUCCESS);
}


