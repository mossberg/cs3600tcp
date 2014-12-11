/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "3600sendrecv.h"

unsigned int sequence = 0;
unsigned int acknum = 0;

unsigned int ack_counter = 0;
packet * window[MAX_WINDOW_SIZE];
unsigned int CUR_WINDOW_SIZE = MAX_WINDOW_SIZE;

void usage() {
  printf("Usage: 3600send host:port\n");
  exit(1);
}

/**
 * Reads the next block of data from stdin
 */
int get_next_data(char *data, int size) {
  return read(0, data, size);
}

/**
 * Builds and returns the next packet, or NULL
 * if no more data is available.
 */
packet *get_next_packet(int *len, unsigned int index) {
	if(window[index % MAX_WINDOW_SIZE] != NULL) {
		*len = ntohs(window[index % MAX_WINDOW_SIZE]->hdr.length) + sizeof(header);
		return window[index % MAX_WINDOW_SIZE];
	}
	char *data = malloc(DATA_SIZE);
	int data_len = get_next_data(data, DATA_SIZE);

	if(data_len < DATA_SIZE) {
		data_len += get_next_data(data + data_len, DATA_SIZE - data_len);	
	}
	if (data_len == 0) {
		free(data);
		mylog("[no data left, index empty] index: %u\n",index);
		return NULL;
		/*
		header *myheader = make_header(sequence, 0, 1, 0);
		packet *next_packet = malloc(sizeof(packet));
		next_packet->hdr = *myheader;
		window[index % MAX_WINDOW_SIZE] = next_packet;
		free(myheader);
		*len = sizeof(header);
		return next_packet;
		*/
	}


	header *myheader = make_header(sequence, data_len, 0, 0);
	sequence += data_len;
	packet *next_packet = malloc(sizeof(packet));
	next_packet->hdr = *myheader;
	memcpy(next_packet->data, data, data_len);

	window[index % MAX_WINDOW_SIZE] = next_packet;

	free(data);
	free(myheader);

	*len = sizeof(header) + data_len;

	return next_packet;
}

int send_next_packet(int sock, struct sockaddr_in out, unsigned int index, int send_eof) {
	int packet_len = 0;
	packet *next_packet = get_next_packet(&packet_len, index);

	if (next_packet == NULL) {
		if(send_eof) {
			mylog("[send eof]\n");
			header *myheader = make_header(sequence, 0, 1, 0);

			if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
				perror("sendto");
				exit(1);
			}
		}
		return 0;
	}
	mylog("[send data] %d (%d) window index:%d\n", ntohl(next_packet->hdr.sequence), packet_len - sizeof(header), index);

	if (sendto(sock, next_packet, packet_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
		perror("sendto");
		exit(1);
	}

	return 1;
}

int send_next_window(int sock, struct sockaddr_in out) {
	for(unsigned int i = 0; i < CUR_WINDOW_SIZE; ++i) {
		//If there is no more data, and we have received an ack for every packet that we have sent,
		//we can exit.
		if(!send_next_packet(sock, out, ack_counter + i, 0)) {
			CUR_WINDOW_SIZE = i+1;
			break;
		}
	}
	if(acknum == sequence) {
		return 0;
	}

	return 1;
}
void send_final_packet(int sock, struct sockaddr_in out) {
  header *myheader = make_header(sequence, 0, 1, 0);
  mylog("[send eof]\n");

  if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
    perror("sendto");
    exit(1);
  }
}
int recv_final_packet(int sock , fd_set * socks, struct sockaddr *in, socklen_t in_len, struct timeval * t) {
	FD_ZERO(socks);
	FD_SET(sock, socks);

	// wait to receive, or for a timeout
	if (select(sock + 1, socks, NULL, NULL, t)) {
		unsigned char buf[10000];
		int buf_len = sizeof(buf);
		int received;
		if ((received = recvfrom(sock, &buf, buf_len, 0, in, (socklen_t *) &in_len)) < 0) {
		  perror("recvfrom");
		  exit(1);
		}
		header *myheader = get_header(buf);

		if(!valid_checksum(buf)) {
			mylog("[ack checksum invalid] %d \n", myheader->sequence);
			return 0;
		} else if(myheader->magic != MAGIC) {
			mylog("[ack magic invalid] %x %d\n", myheader->sequence);
			return 0;
		} else if(myheader->ack != 1) {
			mylog("[ack not an ack] %d\n", myheader->sequence);
			return 0;
		} else if(myheader->fin) {
			//free_window(window);
			return 1;
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a UDP socket in C, 
   * binding to a empheral port, printing out the port number.
   * 
   * I've also included a very simple transport protocol that simply
   * acknowledges every received packet.  It has a header, but does
   * not do any error handling (i.e., it does not have sequence 
   * numbers, timeouts, retries, a "window"). You will
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */

  // extract the host IP and port
  if ((argc != 2) || (strstr(argv[1], ":") == NULL)) {
    usage();
  }

  char *tmp = (char *) malloc(strlen(argv[1])+1);
  strcpy(tmp, argv[1]);

  char *ip_s = strtok(tmp, ":");
  char *port_s = strtok(NULL, ":");
 
  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the local port
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(atoi(port_s));
  out.sin_addr.s_addr = inet_addr(ip_s);

  // socket for received packets
  struct sockaddr_in in;
  socklen_t in_len = sizeof(in);

  // construct the socket set
  fd_set socks;

	//Handshake
	
	while(0) {
		//determine rtt
		//determine window size
		//setup congestion control
	}

	int millisecond = 1000;
	int rtt_sec = 0;
	int rtt_usec = 125 * millisecond;


  // construct the timeout
  struct timeval t;
  t.tv_sec = rtt_sec;
  t.tv_usec = rtt_usec;

	int out_of_data = 0;

  while (send_next_window(sock, out)) {
    int done = 0;

	int dup_counter = 0;

	t.tv_sec = rtt_sec;
	t.tv_usec = rtt_usec;

    while (! done) {
      FD_ZERO(&socks);
      FD_SET(sock, &socks);

      // wait to receive, or for a timeout
      if (select(sock + 1, &socks, NULL, NULL, &t)) {
        unsigned char buf[10000];
        int buf_len = sizeof(buf);
        int received;
        if ((received = recvfrom(sock, &buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
          perror("recvfrom");
          exit(1);
        }

        header *myheader = get_header(buf);

		if(!valid_checksum(buf)) {
            mylog("[ack checksum invalid] %d \n", myheader->sequence);
			break;
		} else if(myheader->magic != MAGIC) {
			mylog("[ack magic invalid] %x %d\n", myheader->sequence);
			break;
		} else if(myheader->ack != 1) {
			mylog("[ack not an ack] %d\n", myheader->sequence);
			break;
		} 

        if (myheader->acknum > acknum) {
			mylog("[recv ack] %d, %d\n", myheader->acknum, acknum);
			dup_counter = 0;
			acknum = myheader->acknum;		
			
			//Packets in the window are stored in network order
			while(window[ack_counter % MAX_WINDOW_SIZE] != NULL) {
				if(acknum == ntohl(window[ack_counter % MAX_WINDOW_SIZE]->hdr.sequence)) {
					break;
				}
				free(window[ack_counter % MAX_WINDOW_SIZE]);
				window[ack_counter % MAX_WINDOW_SIZE] = NULL;			
				if(!out_of_data) {
					out_of_data = !send_next_packet(sock, out, ack_counter, 0);
				}
				ack_counter++;
			}
			
			t.tv_usec += millisecond;
			if(rtt_usec > 10 * millisecond) {
				rtt_usec -= millisecond;
			}
			if(CUR_WINDOW_SIZE < MAX_WINDOW_SIZE) { CUR_WINDOW_SIZE++; }
			
			if(sequence == acknum) {
				done = 1;
			}
						
		} else if(myheader->acknum == acknum) {
			//Increment rtt, and wait for additional time for an extra packet to return
			rtt_usec += millisecond;
			//t.tv_usec += millisecond;
			mylog("[recv out of order or dup ack] %d\n", acknum);
			dup_counter++;
			if(CUR_WINDOW_SIZE > 3) { CUR_WINDOW_SIZE--; }
			if(dup_counter == 6) {
				mylog("[resend data] %d\n", acknum);
				//Send three times just in case, since the last time may have been lost as well
				send_next_packet(sock, out, ack_counter, 0);
				send_next_packet(sock, out, ack_counter, 0);
				send_next_packet(sock, out, ack_counter, 0);
				continue;
			}
			if(dup_counter == 3) {
				mylog("[resend data] %d\n", acknum);
				send_next_packet(sock, out, ack_counter, 0);
				//dup_counter = 0;
			}
        } else {
			mylog("[recv old ack] %d %d\n", myheader->acknum, acknum);
			continue;
		}
      } else {
        mylog("[error] timeout occurred\n");
		//Increase the timeout amount
		rtt_sec *= 2;
		rtt_usec *= 2;
		//Set a hard limit for how large our timeout can be (3 seconds)
		if(rtt_usec > 3000 * millisecond) { rtt_usec = 3000*millisecond; }
		//If we waited a timeout without sending anything, and didn't receive any
		//Response packets during it, optimistically increase the window to resend
		//data
		if(CUR_WINDOW_SIZE <= 4) {
			CUR_WINDOW_SIZE = 10;
		//Decrease the window size to 1/2
		} else {
			CUR_WINDOW_SIZE /= 2;
		}
	
		done = 1;
      }
    }
  }
	//Send the final packet a bunch of times for good luck
	for(int i = 0; i < 5; ++i) {
		send_final_packet(sock, out);
	}
	// Don't even worry about receiving the final ack, they probably got it anyways with the 5 EOFs
	mylog("[completed]\n");
	return 0;
}
