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
packet * window[WINDOW_SIZE];

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
	if(window[index % WINDOW_SIZE] != NULL) {
		*len = ntohs(window[index % WINDOW_SIZE]->hdr.length) + sizeof(header);
		return window[index % WINDOW_SIZE];
	}
	char *data = malloc(DATA_SIZE);
	int data_len = get_next_data(data, DATA_SIZE);

	if (data_len == 0) {
		free(data);
		return NULL;
	}

	header *myheader = make_header(sequence, data_len, 0, 0);
	sequence += data_len;
	packet *next_packet = malloc(sizeof(packet));
	next_packet->hdr = *myheader;
	memcpy(next_packet->data, data, data_len);

	window[index % WINDOW_SIZE] = next_packet;

	free(data);
	free(myheader);

	*len = sizeof(header) + data_len;

	return next_packet;
}

int send_next_packet(int sock, struct sockaddr_in out, unsigned int index) {
	int packet_len = 0;
	packet *next_packet = get_next_packet(&packet_len, index);

	if (next_packet == NULL) {
	  /*
		header *myheader = make_header(sequence, 0, 1, 0);
	  mylog("[send eof]\n");

	  if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
		perror("sendto");
		exit(1);
	  }
	*/
		return 0;
	}
	mylog("[send data] %d (%d)\n", ntohl(next_packet->hdr.sequence), packet_len - sizeof(header));

	if (sendto(sock, next_packet, packet_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
		perror("sendto");
		exit(1);
	}

	return 1;
}

int send_next_window(int sock, struct sockaddr_in out) {
	for(int i = 0; i < WINDOW_SIZE; ++i) {
		//If there is no more data, and we have received an ack for every packet that we have sent,
		//we can exit.
		if(!send_next_packet(sock, out, ack_counter + i)) {
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

  // construct the timeout
  struct timeval t;
  t.tv_sec = 5;
  t.tv_usec = 0;

  while (send_next_window(sock, out)) {
    int done = 0;

	int dup_counter = 0;

	t.tv_sec += 5;

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

        if ((ntohl(myheader->acknum) > acknum)) {
			mylog("[recv ack] %d, %d\n", ntohl(myheader->acknum), acknum);
			acknum = ntohl(myheader->acknum);		
			
			while(window[ack_counter % WINDOW_SIZE] != NULL) {
				if(acknum == ntohl(window[ack_counter % WINDOW_SIZE]->hdr.sequence)) {
					break;
				}
				free(window[ack_counter % WINDOW_SIZE]);
				window[ack_counter % WINDOW_SIZE] = NULL;			

				ack_counter++;
			}
			if(sequence == acknum) {
				done = 1;
			}			
        } else if(ntohl(myheader->acknum) == acknum) {
			mylog("[recv out of order or dup ack] %d\n", acknum);
			dup_counter++;
			if(dup_counter == 3) {
				mylog("[resend data] %d\n", acknum);
				send_next_packet(sock, out, ack_counter);
				dup_counter = 0;
			}
        } else {
			mylog("[recv old ack] %d %d\n", ntohl(myheader->acknum), acknum);
			continue;
		}
      } else {
        mylog("[error] timeout occurred\n");
	//	send_next_packet(sock, out, ack_counter);
		t.tv_sec += 5;
		done = 1;
      }
    }
  }

  send_final_packet(sock, out);

  mylog("[completed]\n");

  return 0;
}
