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
#include <stdarg.h>
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

unsigned int MAGIC = 0x0bee;

char ts[16];

/**
 * Returns a properly formatted timestamp
 */
char *timestamp() {
  time_t ltime;
  ltime=time(NULL);
  struct tm *tm;
  tm=localtime(&ltime);
  struct timeval tv1;
  gettimeofday(&tv1, NULL);

  sprintf(ts,"%02d:%02d:%02d %03d.%03d", tm->tm_hour, tm->tm_min, tm->tm_sec, (int) (tv1.tv_usec/1000), (int) (tv1.tv_usec % 1000));
  return ts;
}

/**
 * Logs debugging messages.  Works like printf(...)
 */
void mylog(char *fmt, ...) {
  va_list args;
  va_start(args,fmt);
  fprintf(stderr, "%s: ", timestamp());
  vfprintf(stderr, fmt,args);
  va_end(args);
}

/* returns 1 if valid, 0 if not */
int valid_checksum_pkt(unsigned short checksum, packet  *pkt) {
    header tmphdr;
    memcpy(&tmphdr, &pkt->hdr, sizeof(pkt->hdr));
    tmphdr.checksum = 0; /* remove checksum from header before calculating */

    /* verify 2 stage checksum */
    unsigned short hdr_cksum = calc_checksum((unsigned char *) &(tmphdr), sizeof(header));
    unsigned short data_chksum = calc_checksum((unsigned char *) pkt->data, DATA_SIZE);
    unsigned short tmp = hdr_cksum + data_chksum;
    if(tmp < hdr_cksum) {tmp++;}

    return checksum == tmp;
}

/* returns 1 if valid, 0 if not */
int valid_checksum_hdr(unsigned short checksum, header *hdr) {
    header tmphdr;
    memcpy(&tmphdr, hdr, sizeof(*hdr));
    tmphdr.checksum = 0; /* remove checksum from header before calculating */
    unsigned short hdr_cksum = calc_checksum((unsigned char *) &(tmphdr), sizeof(header));
    return checksum == hdr_cksum;
}

unsigned short calc_checksum(unsigned char * data, int count) {
    return ~ones_sum(data, count);
}

/*
 * ones_sum - Calculates checksum of data
 * @data: data buffer
 * @count: length of data
 *
 * Assumes data can be divided into 2 byte words.
 */
unsigned short ones_sum(unsigned char * _data, int count) {
    /* recast so data++ advances by 2 bytes instead of 1 */
    unsigned short * data = (unsigned short *) _data;
    int sum = 0;
    count /= 2;

    /* sum all 2 byte data words */
    while (count--) {
        sum += *data++;
        /* overflow check */
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum += 1;
        }
    }

    return sum;
}

void free_window(packet **window) {
    for (int i = 0; i < MAX_WINDOW_SIZE; i++) {
        free(window[i]);
    }
}

/**
 * This function takes in a bunch of header fields and 
 * returns a brand new header.  The caller is responsible for
 * eventually free-ing the header.
 */
header *make_header(int sequence, short length, int fin, int ack) {
	header *myheader = (header *) calloc(1, sizeof(header));
	myheader->magic = MAGIC;
	myheader->fin = fin;
	if(ack) {
		myheader->acknum = htonl(sequence);
	} else {
		myheader->sequence = htonl(sequence);
	}
	myheader->length = htons(length);
	myheader->ack = ack;
    /* myheader->checksum = 0; */

    /* calculate checksum in host order */
    header cksum_header;
    memset(&cksum_header, 0, sizeof(cksum_header));
    cksum_header.magic = MAGIC;
    cksum_header.fin = fin;
	if(ack) {
		cksum_header.acknum = sequence;
	} else {
		cksum_header.sequence = sequence;
	}
	cksum_header.length = length;
	cksum_header.ack = ack;
    cksum_header.checksum = 0;

    /* incomplete checksum (just of header) */
    myheader->checksum = calc_checksum((unsigned char *) &cksum_header, sizeof(cksum_header));

	return myheader;
}

/**
 * This function takes a returned packet and returns a header pointer.  It
 * does not allocate any new memory, so no free is needed.
 */
header *get_header(void *data) {
  header *h = (header *) data;
  h->acknum = ntohl(h->acknum);
  h->sequence = ntohl(h->sequence);
  h->length = ntohs(h->length);
  h->checksum = ntohs(h->checksum);

  return h;
}

/**
 * This function takes a returned packet and returns a pointer to the data.  It
 * does not allocate any new memory, so no free is needed.
 */
unsigned char *get_data(void *data) {
  return (unsigned char *) data + sizeof(header);
}

/**
 * This function will print a hex dump of the provided packet to the screen
 * to help facilitate debugging.  
 *
 * DO NOT MODIFY THIS FUNCTION
 *
 * data - The pointer to your packet buffer
 * size - The length of your packet
 */
void dump_packet(unsigned char *data, int size) {
    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((unsigned int)(intptr_t) p-(unsigned int)(intptr_t) data) );
        }
            
        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}
