#define _WIN32_WINNT 0x0601


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define HOST "example.com"
#define PORT "80"



int main(void)
{
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


      result = getaddrinfo(HOST, PORT, &hints, &res);
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
      printf("Unable to connect to %s\n", HOST);
      WSACleanup();
      return 1;
  }



  const char *request = 
      "GET / HTTP/1.1\r\n"
      "HOST: example.com\r\n"
      "Connection: close\r\n"
      "\r\n";



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

  int total = 0;
  int bytes = 0;
  char buffer[4096];

  
  while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
      fwrite(buffer, 1, bytes, fp);
      total += bytes;
  }


  printf("Received %d bytes total\n", total);

//
  if (bytes == SOCKET_ERROR) {
      printf("\nrecv failed: %d\n", WSAGetLastError());
  }

  fclose(fp);
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

  printf("Read %d bytes from file\n", got);


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


//      printf("header_end = %d\n", header_end);
   int header_bytes = header_end;
   int body_start   = header_end + 4;
   int body_bytes   = got - body_start;

   printf("header_end: %d\n", header_end);
   printf("Header bytes: %d\n", header_bytes);
   printf("Body bytes: %d\n", body_bytes);
  

  closesocket(sock);
  WSACleanup();



 return 0;


}
