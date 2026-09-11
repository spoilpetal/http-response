#define _WIN32_WINNT 0x0601
// this file contains how to get http response (but in input domain with user agent)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define HOST "example.com"
#define PORT "80"



static int hex_value(int c)
{
   if (c >= '0' && c <= '9') return c - '0';
   if (c >= 'a' && c <= 'f') return c - 'a' + 10;
   if (c >= 'A' && c <= 'F') return c - 'A' + 10;
   return -1;
}


int main(int argc, char *argv[])
{
      if (argc < 3) {
          printf("Usage: %s <host> <path>\n", argv[0]);
          return 1;
      }

      WSADATA wsaData;
      int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
      if (result != 0) {
          printf("WSAStartup failed; %d\n", result);
          return 1;

      }

      struct addrinfo hints;
      struct addrinfo *res = NULL;
      struct addrinfo *p = NULL;

      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;  // IPv4 for simplicity now
      hints.ai_socktype = SOCK_STREAM;  //TCP
      hints.ai_protocol = IPPROTO_TCP;


      result = getaddrinfo(argv[1], PORT, &hints, &res);
      if (result != 0) {
          printf("getaddrinfo failed: %d\n", result);
          WSACleanup();
          return 1;
      }


      SOCKET sock = INVALID_SOCKET;




      for (p = res; p != NULL; p = p->ai_next) {
          sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
          if (sock == INVALID_SOCKET) {
              continue;
          }

      if (connect(sock, p->ai_addr, (int)p->ai_addrlen) != SOCKET_ERROR) {
          break;
      }



      closesocket(sock);
      sock = INVALID_SOCKET;
  }



  freeaddrinfo(res);


  if (sock == INVALID_SOCKET) {
      printf("Unable to connect to %s\n", argv[1]);
      WSACleanup();
      return 1;
  }



  char request[512];
  snprintf(request, sizeof(request),
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: BigFish/0.1\r\n"
    "Connection: close\r\n"
    "\r\n",
    argv[2], argv[1]);


  int sent = send(sock, request, (int)strlen(request), 0);
  if (sent == SOCKET_ERROR) {
      printf("send failed: %d\n", WSAGetLastError());
      closesocket(sock);
      WSACleanup();
      return 1;
  }

//  send();
  printf("Sent %d bytes\n\n", sent);



//


  FILE *fp = fopen("raw_response.bin", "wb");
  if (fp == NULL) {
      printf("Could not open file for writing\n");
      closesocket(sock);
      WSACleanup();
      return 1;

  }

//  int total = 0;
  int bytes = 0;
  char buffer[4096];

  
  while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
      fwrite(buffer, 1, bytes, fp);
     // total += bytes;
  }


//  printf("Received %d bytes total\n", total);


  if (bytes == SOCKET_ERROR) {
      printf("\nrecv failed: %d\n", WSAGetLastError());
      fclose(fp);
      closesocket(sock);
      WSACleanup();
      return 1;
  }

  FILE *in = fopen("raw_response.bin", "rb");
  if (in == NULL) {
      printf("Could not open raw_response.bin for reading\n");
      closesocket(sock);
      WSACleanup();
      return 1;
  }

  char raw[16384];
  int got = (int)fread(raw, 1, sizeof(raw), in);
  fclose(in);

 // printf("Read %d bytes from file\n", got);


  int header_end = -1;

  for (int i = 0; i + 3 < got; i++) {
      if (raw[i]     == 0x0D &&
          raw[i + 1] == 0x0A &&
          raw[i + 2] == 0x0D &&
          raw[i + 3] == 0x0A) {
          header_end = i;
          break;
      }
  }


  if (header_end == -1) {
      printf("Could not find end of headers\n");
      closesocket(sock);
      WSACleanup();
      return 1;
  }



  // int header_bytes = header_end;
   int body_start   = header_end + 4;
   int body_bytes   = got - body_start;

//   printf("header_end: %d\n", header_end);
  // printf("Header bytes: %d\n", header_bytes);
   char decoded[16384];
   int  decoded_len = 0;
   int pos = body_start;
   int chunk_size = 0;

   //printf("Chunk sizes in body:\n");
   
   while (1) {
     /////////////1. Parse the hex chunk size starting at pos
     chunk_size = 0;
     while (1) {
         int v = hex_value((unsigned char)raw[pos]);
         if (v < 0) break;
         chunk_size = chunk_size * 16 + v;
         pos++;
     }

     ////2. The teo bytes after the hex digits must be \r\n
     if (raw[pos] != '\r' || raw[pos + 1] != '\n') {
        printf("Malformed chunk eader at byte %d\n", pos);
        break;
     }

     //printf("  chunk_size = %d\n", chunk_size);

     ////3. chunk_size 0 means end of body
     if (chunk_size == 0) {
        break;
     }


     ////4. Skip the \r\n after the hex digits, then the data
     pos += 2;

     memcpy(decoded + decoded_len, raw + pos, chunk_size);
     decoded_len += chunk_size;


     ////skip the data
     pos += chunk_size;


     ////5. After the data there must be another \r\n
     if (raw[pos] != '\r' || raw[pos +1] != '\n') {
        printf("Malformed chunk trailer at byte %d\n", pos);
        break;
     }
     pos += 2;


   }
  
  // printf("Decoded body length: %d bytes\n", decoded_len);
   
   FILE *out = fopen("decoded_body.html", "wb");
   if (out == NULL) {
      printf("Could not open decoded_body.html for writing\n");
      closesocket(sock);
      WSACleanup();
      return 1;
   }

   fwrite(decoded, 1, decoded_len, out);
   fclose(out);

   //printf("Wrote decoded_body.html\n");




   printf("Body bytes: %d\n", body_bytes);
   //Print the headers, byte by byte as text
  /*
   printf("\n--- HEADERS ---\n");
   for (int i = 0; i < header_bytes; i++) {
        putchar(raw[i]);
   }
   printf("\n--- END HEADERS ---\n");
*/
   // peek at the first 8 bytes of the body in hex
  /* printf("\n--- FIRST 8 BODY BYTES (hex) ---\n");
   for (int i = body_start; i < body_start + 8 && i < got; i++) {
        printf("%02x ", (unsigned char)raw[i]);
   }
   printf("\n");
*/
  closesocket(sock);
  WSACleanup();



 return 0;


}
