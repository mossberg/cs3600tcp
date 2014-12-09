/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600SENDRECV_H__
#define __3600SENDRECV_H__

#include <stdio.h>
#include <stdarg.h>

#define MAX_WINDOW_SIZE 100
#define DATA_SIZE 1460

typedef struct header_t {
	unsigned int magic:13;
	unsigned int syn:1;
	unsigned int ack:1;
	unsigned int fin:1;
	unsigned short length;
	unsigned int sequence;
	unsigned int acknum;
	unsigned short checksum;
	char filler[6]; //make header 20 bytes
} header; 

typedef struct packet_t {
	header hdr; //20
	unsigned char data[DATA_SIZE]; //1460
} packet; //1480


unsigned int MAGIC;

int valid_checksum(unsigned char * data);
unsigned short calc_checksum(unsigned char * data);
void free_window(packet **window);

void dump_packet(unsigned char *data, int size);
header *make_header(int sequence, int length, int fin, int ack);
header *get_header(void *data);
char *get_data(void *data);
char *timestamp();
void mylog(char *fmt, ...);

#endif

