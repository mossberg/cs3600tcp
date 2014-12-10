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

int main() {
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

  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the local port
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(0);
  out.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *) &out, sizeof(out))) {
    perror("bind");
    exit(1);
  }

  struct sockaddr_in tmp;
  int len = sizeof(tmp);
  if (getsockname(sock, (struct sockaddr *) &tmp, (socklen_t *) &len)) {
    perror("getsockname");
    exit(1);
  }

  mylog("[bound] %d\n", ntohs(tmp.sin_port));

  // wait for incoming packets
  struct sockaddr_in in;
  socklen_t in_len = sizeof(in);

  // construct the socket set
  fd_set socks;

  // construct the timeout
  struct timeval t;
  t.tv_sec = 3000;
  t.tv_usec = 0;

  // our receive buffer
	int buf_len = 1500;
	void* buf = malloc(buf_len);

	packet * window[MAX_WINDOW_SIZE] = {0};

	unsigned int counter = 0;
	unsigned int ack = 0;
	
	unsigned window_size = MAX_WINDOW_SIZE;

	while(0) {
		//TCP Handshake
		//Initialize 
	}

  // wait to receive, or for a timeout
  while (1) {
    FD_ZERO(&socks);
    FD_SET(sock, &socks);

    if (select(sock + 1, &socks, NULL, NULL, &t)) {
      int received;
      /* this is necessary because the sender only sends what data is reads
         so if it reads less data than DATA_SIZE and we don't memset buf, we'll
         have leftover memory */
      memset(buf, 0, buf_len);
      if ((received = recvfrom(sock, buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
        perror("recvfrom");
		free(buf);
		//Cleanup Window
	    exit(1);
      }

        /* interpret incoming packet bytes */
		header *myheader = get_header(buf);
		unsigned char *data = get_data(buf);
		if(myheader->sequence > ack + DATA_SIZE * window_size) {
			mylog("[received packet outside of window]\n");
			continue;
		} else if(!valid_checksum_pkt(myheader->checksum,  buf)) {
			mylog("[data corrupted]\n");
			continue;
		} else if(myheader->magic != MAGIC) {
			mylog("[unrecognized packet]\n");
			continue;
		}
		
		//Got the sequence value that we were expecting
		if(myheader->sequence == ack) {
			write(1, data, myheader->length);
			mylog("[recv data] %d (%d) %s\n", myheader->sequence, myheader->length, "ACCEPTED (in-order)");
			ack += myheader->length;
			int fin = myheader->fin;
			counter++;
			while(window[counter % window_size] != NULL) {
				ack += window[counter % window_size]->hdr.length;
				fin = window[counter % window_size]->hdr.fin;
				write(1, window[counter % window_size]->data, window[counter % window_size]->hdr.length),
				free(window[counter % window_size]); 
				window[counter % window_size] = NULL;
				counter++;
			}

			mylog("[send ack] %d\n",ack);

            /* the receiver doesn't have to send full packets, just headers */
			header *responseheader = make_header(ack, 0, fin, 1);
            /* make_header() doesn't conver the cksum to network order 
             * so do it manually */
            responseheader->checksum = htons(responseheader->checksum);

			if (sendto(sock, responseheader, sizeof(header), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0) {
			  perror("sendto");
		      free_window(window);
			  exit(1);
			}
			if (fin) {
			  mylog("[recv fin]\n");
			  mylog("[completed]\n");
			  exit(0);
			}
		//TODO: bug if sequence, ack both near 2^32, sequence could be out of order and roll-over and be caught by this case
		//Received duplicate packet
		} else if(myheader->sequence < ack) {
			mylog("[recv dup] %d\n", myheader->sequence);
			//Send ack for what we have already accepted
			header *responseheader = make_header(ack, 0, myheader->fin, 1);
			if (sendto(sock, responseheader, sizeof(struct header_t), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0) {
			  perror("sendto");
		      free_window(window);
			  exit(1);
			}
		//Received a packet above the expected packet, but within window size
		} else {
			mylog("[recv data] %d (%d) %s\n", myheader->sequence, myheader->length, "ACCEPTED (out-of-order)");
			header *responseheader = make_header(ack, 0, 0, 1);
			mylog("[send ack] %d\n",ack);
			if (sendto(sock, responseheader, sizeof(header), 0, (struct sockaddr *) &in, (socklen_t) sizeof(in)) < 0) {
			  perror("sendto");
				free_window(window);
			  exit(1);
			}

			packet * pkt = malloc(1480);
			pkt->hdr = *myheader;
			memcpy(pkt->data, data, DATA_SIZE);
			unsigned int index = counter + (myheader->sequence - ack)/DATA_SIZE;
			if(myheader->sequence - ack < DATA_SIZE) {
				index++;
			}
			window[index % window_size] = pkt;		
		}

    } else {
      mylog("[error] timeout occurred\n");
		free_window(window);
      exit(1);
    }
  }


  return 0;
}
