/* Copyright (C) 2019 The University of Alabama
* Author: William (Bill) Engelke, AB4EJ
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* External packages:
 - fftw
*/

#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <complex.h>
#include <fftw3.h>
#include <math.h>

#define FFT_N 1024
#define UDPPORT_IN 7791

struct flexDataSample
{
  float I_val;
  float Q_val;
};

// FlexRadio outputs IQ buffer in conformance to VITA-49 standard
// In the standard, data length can vary, but we code for 512 samples
// per buffer, as Flex uses. Each sample is a pair of 32-bit floating
// point IQ values (i.e., a single complex number) *big-endian* byte order

typedef struct __attribute__((__packed__)) flexDataBuf // no padding - this layout must be exact
{
  uint8_t VITA_hdr1[2];  // rightmost 4 bits is a packet counter
  uint16_t  VITA_packetsize; // size in words (32bits)
  union {
    uint8_t stream_ID[4];
    uint32_t stream_ID32;
  };
  uint8_t class_ID[8];
  uint32_t time_stamp;
  uint64_t fractional_seconds;
  struct flexDataSample flexDatSample[512];
} FLEXBUF;

static struct sockaddr_in flex_tcp_addr;
static struct sockaddr_in flex_udp_addr;

static int sock, tcpsock;  // sockets
static uint8_t flex_reply[3072]; // largest response seen 2282b
static uint8_t bigendian; // 1: big-endian, 0: little-endian

static  int fd;
//static int fd2;
static struct flexDataBuf iqbuffer;
static fftwf_complex FFT_in[FFT_N];
static fftwf_complex FFT_out[FFT_N];
static fftwf_plan p;

float iqscale = 1.0 / 32768.0; // (1.0 / 2^15) [from FlexLib 3.x source]

void main() {

 fd_set readfd;
 fd_set writefd;
 int count;
 //memset(&tdataBuf,sizeof(tdataBuf),0);

#ifdef DEBUG

 printf("ntohl() endian test: ");
 if (ntohl(0x12345678) == 0x12345678) {
   printf("big-endian\n");
 } else if (ntohl(0x12345678) == 0x78563412) {
   printf("little-endian\n");
 } else {
   printf("middle-endian??\n");
 }

 printf("compiler __BYTE_ORDER test: ");
#if __BYTE_ORDER == __BIG_ENDIAN
 printf("big-endian\n");
#elif __BYTE_ORDER == __LITTLE_ENDIAN
 printf("little-endian\n");
#elif
 printf("middle-endian??\n");

#endif // if __BYTE_ORDER
#endif // ifdef DEBUG

#if __BYTE_ORDER == __BIG_ENDIAN
 bigendian = 1;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
 bigendian = 0;
#endif // if __BYTE_ORDER

 sock = socket(AF_INET, SOCK_DGRAM, 0);
 if(sock < 0) {
  printf("sock error\n");
  return;
  }

// port for broadcast 
 printf("bcast socket setup...\n");
// fd2 = socket(AF_INET, SOCK_DGRAM, 0);
 int broadcast = 1;

 struct hostent* he = gethostbyname("localhost");

 int len = sizeof(iqbuffer);

// port for receiving flex IQ stream
 int addr_len = sizeof(struct sockaddr_in);
 memset((void*)&flex_udp_addr, 0, addr_len);
 flex_udp_addr.sin_family = AF_INET;
 flex_udp_addr.sin_addr.s_addr = htons(INADDR_ANY);
 flex_udp_addr.sin_port = htons(UDPPORT_IN);
 int ret = bind(sock, (struct sockaddr*)&flex_udp_addr, addr_len);
 if (ret < 0){
  printf("bind error\n");
  return;
 }
 FD_ZERO(&readfd);
 FD_SET(sock, &readfd);
 printf("read from port %i\n",UDPPORT_IN);

 int bufcount = 0;
 ret = 1;

 p = fftwf_plan_dft_1d(FFT_N, FFT_in, FFT_out, FFTW_FORWARD, FFTW_ESTIMATE);

 while(1==1) {  // repeating loop

  if(ret > 0) {
   if (FD_ISSET(sock, &readfd)) {

// This is a simplistic example which builds an array of 1,024 complex samples to pass
// to fftw, using exactly 2 Flex buffers. You can, of course, expand this to a larger
// array (with necessary logic) for a bigger FFT if you need greater frequency bin precision.

    bufcount = 0;
    for(int j = 0; j <= 1; j++) {
      count = recvfrom(sock, &iqbuffer, sizeof(iqbuffer),0, (struct sockaddr*)&flex_udp_addr, &addr_len);

#ifdef DEBUG
      printf("\nbuffer #%i bytes received = %i\n", j, count); // always buffer #0 bytes received = 4128  (??)
      printf("VITA header= %x %x\n", iqbuffer.VITA_hdr1[0], iqbuffer.VITA_hdr1[1]);
      uint16_t t = iqbuffer.VITA_packetsize;
      printf("VITA packet= %u words (%u bytes)\n", ( bigendian==1 ? t : ntohs(t) ), ( bigendian==1 ? t*4 : ntohs(t*4) ));
      printf("  Stream ID= %#010x\n\n", ( bigendian==1 ? iqbuffer.stream_ID32 : ntohl(iqbuffer.stream_ID32)));

      // printf("timestamp = %i \n",iqbuffer.time_stamp/16777216);
#endif // ifdef DEBUG

      for(int i=0; i < 512; i++) {  // iterate thru 512 IQ samples
#ifdef DATADUMP
	printf("%8.2f %8.2f\t", iqbuffer.flexDatSample[i].I_val, iqbuffer.flexDatSample[i].Q_val);

	// fmt %#010x produces 0000000000 when value is 0
	printf("0x%08x 0x%08x\t", (uint32_t)iqbuffer.flexDatSample[i].I_val, (uint32_t)iqbuffer.flexDatSample[i].Q_val);
	printf("0x%08x 0x%08x\t", ntohl((uint32_t)iqbuffer.flexDatSample[i].I_val), ntohl((uint32_t)iqbuffer.flexDatSample[i].Q_val));

	printf("%14.2f %14.2f\n", (float)ntohl((uint32_t)iqbuffer.flexDatSample[i].I_val), (float)ntohl((uint32_t)iqbuffer.flexDatSample[i].Q_val));
#endif // ifdef DATADUMP

	// unexpectedly: IQ payload is delivered LITTLE-endian (unlike Audio payload and header values)
	float x1 = ( !bigendian ? iqbuffer.flexDatSample[i].I_val : (float)ntohl((uint32_t)iqbuffer.flexDatSample[i].I_val) );
	float x2 = ( !bigendian ? iqbuffer.flexDatSample[i].Q_val : (float)ntohl((uint32_t)iqbuffer.flexDatSample[i].Q_val) );

	// language supports (x1) + ((x2) * I) construct for complex datatype [since C99] - "x + iy"
	FFT_in[bufcount] = x1 + ( x2 * I );
	bufcount++;
      }
    }

    fftwf_execute(p);

    float maxval = 0;
    int maxbin = 0;
    float M;

    for(int i=0;i<FFT_N;i++) { // i<=FFT_N allowed i to be equal to FFT_N (the n+1'th value) inside loop

      // creal() expects double complex Z; crealf() - float complex Z; sqrt() - double X; sqrtf() - float X;

      // M = (float)sqrt( (double)(crealf(FFT_out[i]) * crealf(FFT_out[i])) + (double)(cimagf(FFT_out[i]) * cimagf(FFT_out[i])) );

      // or more concisely: cabsf(FFT_out[i])
      M = cabsf( FFT_out[i] );

      if(M>maxval) {
	maxval = M;
	maxbin = i;
      }
    }
    // printf("maxbin = %i   %f\n",maxbin,maxval/1000000.0); // always maxbin = 1024 (??). always scale maxval * 1e-6 (??)

    // +freq bins 0-511, -freq bins 512-1023
    printf("\nmaxbin# %4i\t(value:) %f\t(scaled:) %f\n", maxbin, maxval, maxval*iqscale);
 
   } // end if (ret > 0)
  } // end of repeating loop
 }
 // NOTREACHED

 printf("end\n");

}
