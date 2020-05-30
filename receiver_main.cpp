/* 
 * File:   receiver_main.cpp
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "packet.hpp"


struct sockaddr_in si_me, si_other;
int s, slen;

void diep(const char* s) {
    perror(s);
    exit(1);
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep((char*) "bind");
		


	/* Now receive data and send acknowledgements */  
	socklen_t si_other_size = sizeof(si_other);
	packet out;
	packet* in = (packet*)calloc(1, sizeof(packet));
	std::cout<<"get syn"<<std::endl;
	recvfrom(s, in, sizeof(packet), 0, (struct sockaddr *)&si_other, &si_other_size);
	if(in->syn != 1){
    	printf("failed to SYN\n");
    	return;
    }
	int in_seq = in->seq_num;
	int out_seq = 0;
	out.seq_num = out_seq;
	out.ack_num = in_seq + 1;
	out.syn = 1;
	out.ack = 1;
	out.window = REC_WINDOW;
	std::cout<<"send syn ack"<<std::endl;
	sendto(s, &out, sizeof(packet), 0, (struct sockaddr *)&si_other, sizeof(si_other));
	memset(in, 0, sizeof(packet));
	std::cout<<"get ack"<<std::endl;
	recvfrom(s, in, sizeof(packet), 0, (struct sockaddr *)&si_other, &si_other_size);
	// std::cout<<"got ack"<<std::endl;
	if(in->ack != 1 || in->ack_num != out_seq+1){
		printf("bad synsynackack");
		return;
	}
	std::cout<<"yay we shook hand"<<std::endl;
	std::cout.flush();
	int expectedseq_num = in->seq_num + 1;
	FILE* output = fopen(destinationFile, "w");
	do{
		memset(in, 0, sizeof(packet));
		printf("I'm expecting seq_num: %d\n", expectedseq_num);
		recvfrom(s, in, sizeof(packet), 0, (struct sockaddr *)&si_other, &si_other_size);
		if(in->seq_num != expectedseq_num){
			sendto(s, &out, sizeof(packet), 0, (struct sockaddr *)&si_other, sizeof(si_other));
		}else{
			memset(&out, 0, sizeof(packet));
			expectedseq_num += 1;
			//change this to fwrite
			//fprintf(output, "%.*s", in->buf_size,in->data);
			fwrite(in->data, sizeof(char), in->buf_size, output);
			out.seq_num = in->ack_num;
			out.ack_num = expectedseq_num;
			out.ack = 1;
			out.window = REC_WINDOW;  // dont think is necessary
			out.buf_size = 0;
			sendto(s, &out, sizeof(packet), 0, (struct sockaddr *)&si_other, sizeof(si_other));
		}	
	}while(in->fin != 1);
	std::cout<<"closing"<<std::endl;
    close(s);
	fclose(output);
	printf("%s received.\n", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    
    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

