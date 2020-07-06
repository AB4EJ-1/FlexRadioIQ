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
// point IQ values (i.e., a single complex number)

typedef struct flexDataBuf
 {
 char VITA_hdr1[2];  // rightmost 4 bits is a packet counter
 int16_t  VITA_packetsize;
 char stream_ID[4];
 char class_ID[8];
 uint32_t time_stamp;
 uint64_t fractional_seconds;
 struct flexDataSample flexDatSample[512];
 } FLEXBUF;

static struct sockaddr_in flex_tcp_addr;
static struct sockaddr_in flex_udp_addr;

static int sock, tcpsock;  // sockets
static char flex_reply[3000];

static  int fd;
//static int fd2;
static struct flexDataBuf iqbuffer;
static fftwf_complex FFT_in[FFT_N];
static fftwf_complex FFT_out[FFT_N];
static fftwf_plan p;

void main() {

 fd_set readfd;
 fd_set writefd;
 int count;
 //memset(&tdataBuf,sizeof(tdataBuf),0);

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

 int len =sizeof(iqbuffer);

// port for reading flex
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
  if(ret > 0){
   if (FD_ISSET(sock, &readfd)){

// This is a simplistic example which builds an array of 1,024 complex samples to pass
// to fftw, using exactly 2 Flex buffers. You can, of course, expand this to a larger
// array (with necessary logic) for a bigger FFT if you need greater frequency bin precision.

    bufcount = 0;
    for(int j = 0; j <= 1; j++)
     {
      count = recvfrom(sock, &iqbuffer, sizeof(iqbuffer),0, (struct sockaddr*)&flex_udp_addr, &addr_len);
   //   printf("buffer %i bytes received = %i\n",j,count);
   //   printf("VITA header= %x %x\n",iqbuffer.VITA_hdr1[0],iqbuffer.VITA_hdr1[1]);
    //  printf("stream ID= %02x %02x %02x %02x\n", iqbuffer.stream_ID[0],iqbuffer.stream_ID[1], iqbuffer.stream_ID[2],iqbuffer.stream_ID[3]);

   //   printf("timestamp = %i \n",iqbuffer.time_stamp/16777216);

      for(int i=0;i<512;i++) {  // iterate thru 512 IQ samples
    //   printf("%f %f \n",iqbuffer.flexDatSample[i].I_val_int,iqbuffer.flexDatSample[i].Q_val_int);
       FFT_in[bufcount] = iqbuffer.flexDatSample[i].I_val + ( iqbuffer.flexDatSample[i].Q_val * I);
       bufcount++;
      // printf("\n");
       }
     }
    fftwf_execute(p);
    float maxval = 0;
    int maxbin = 0;
    float M;
    for(int i=0;i<=FFT_N;i++)
      {   
      M = sqrt( creal(FFT_out[i])*creal(FFT_out[i])+cimag(FFT_out[i])*cimag(FFT_out[i]) );
         if(M>maxval)
          {
          maxval = M;
          maxbin = i;
          }
      }
     printf("maxbin = %i   %f\n",maxbin,maxval/1000000.0);
     
    }
  }  // end of repeating loop
 }
 

 printf("end\n");

}
