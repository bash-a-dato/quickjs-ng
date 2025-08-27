#include "quickjs-debugger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif


struct js_transport_data {
  int handle;
} js_transport_data;

static size_t js_transport_read(void *udata, char *buffer, size_t length)
{
  printf("[DEBUG] js_transport_read called, length: %zu\n", length);
  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) {
    printf("[DEBUG] js_transport_read: invalid handle\n");
    return -1;
  }

  if (length == 0) {
    printf("[DEBUG] js_transport_read: zero length\n");
    return -2;
  }

  if (buffer == NULL) {
    printf("[DEBUG] js_transport_read: null buffer\n");
    return -3;
  }

  // ssize_t ret = read(data->handle, (void *)buffer, length);
  ssize_t ret = recv(data->handle, (void *)buffer, length, 0);
  
  if (ret == SOCKET_ERROR) {
    printf("[DEBUG] js_transport_read: recv failed with error: %d\n", WSAGetLastError());
    return -4;
  }

  if (ret == 0) {
    printf("[DEBUG] js_transport_read: connection closed\n");
    return -5;
  }

  if (ret > length) {
    printf("[DEBUG] js_transport_read: received more than requested\n");
    return -6;
  }

  printf("[DEBUG] js_transport_read: successfully read %zd bytes\n", ret);
  
  // Log the received data for debugging
  printf("[DEBUG] Received data: ");
  for (int i = 0; i < ret && i < 200; i++) {  // Limit to first 200 chars
    char c = buffer[i];
    if (c >= 32 && c <= 126) {
      printf("%c", c);
    } else if (c == '\n') {
      printf("\\n");
    } else if (c == '\r') {
      printf("\\r");
    } else {
      printf("\\x%02x", (unsigned char)c);
    }
  }
  printf("\n");
  
  return ret;
}

static size_t js_transport_write(void *udata, const char *buffer, size_t length)
{
  printf("[DEBUG] js_transport_write called, length: %zu\n", length);
  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) {
    printf("[DEBUG] js_transport_write: invalid handle\n");
    return -1;
  }

  if (length == 0) {
    printf("[DEBUG] js_transport_write: zero length\n");
    return -2;
  }

  if (buffer == NULL) {
    printf("[DEBUG] js_transport_write: null buffer\n");
    return -3;
  }

  // size_t ret = write(data->handle, (const void *) buffer, length);
  size_t ret = send(data->handle, (const void *)buffer, length, 0);
  if (ret == SOCKET_ERROR) {
    printf("[DEBUG] js_transport_write: send failed with error: %d\n", WSAGetLastError());
    return -4;
  }
  if (ret <= 0 || ret > (ssize_t)length) {
    printf("[DEBUG] js_transport_write: unexpected return value: %zu\n", ret);
    return -4;
  }

  printf("[DEBUG] js_transport_write: successfully wrote %zu bytes\n", ret);
  
  // Log the sent data for debugging
  printf("[DEBUG] Sent data: ");
  for (int i = 0; i < ret && i < 200; i++) {  // Limit to first 200 chars
    char c = buffer[i];
    if (c >= 32 && c <= 126) {
      printf("%c", c);
    } else if (c == '\n') {
      printf("\\n");
    } else if (c == '\r') {
      printf("\\r");
    } else {
      printf("\\x%02x", (unsigned char)c);
    }
  }
  printf("\n");
  
  return ret;
}

static size_t js_transport_peek(void *udata)
{
  WSAPOLLFD fds[1];
  int poll_rc;

  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) return -1;

  fds[0].fd = data->handle;
  fds[0].events = POLLIN;
  fds[0].revents = 0;

  poll_rc = WSAPoll(fds, 1, 0);
  if (poll_rc < 0) return -2;
  if (poll_rc > 1) return -3;
  // no data
  if (poll_rc == 0) return 0;
  // has data
  return 1;
}

static void js_transport_close(JSRuntime *rt, void *udata)
{
  printf("[DEBUG] js_transport_close called\n");
  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) {
    printf("[DEBUG] Invalid handle, nothing to close\n");
    return;
  }

  printf("[DEBUG] Closing socket: %d\n", data->handle);
  closesocket(data->handle);
  data->handle = 0;

  free(udata);
  printf("[DEBUG] Transport data freed\n");

  WSACleanup();
  printf("[DEBUG] WSACleanup completed\n");
}

void js_debugger_connect(JSContext *ctx, const char *address)
{
  printf("[DEBUG] js_debugger_connect called with address: %s\n", address);

  WSADATA wsaData;
  int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaResult != 0) {
    printf("[DEBUG] WSAStartup failed with error: %d\n", wsaResult);
    return;
  }
  printf("[DEBUG] WSAStartup successful\n");

  char *port_string = strstr(address, ":");
  if (!port_string) {
    printf("[DEBUG] Invalid address format - no port found\n");
    WSACleanup();
    return;
  }

  int port = atoi(port_string + 1);
  if (port <= 0) {
    printf("[DEBUG] Invalid port number: %d\n", port);
    WSACleanup();
    return;
  }
  printf("[DEBUG] Connecting to port: %d\n", port);

  int client = socket(AF_INET, SOCK_STREAM, 0);
  if (client == INVALID_SOCKET) {
    printf("[DEBUG] Socket creation failed with error: %d\n", WSAGetLastError());
    WSACleanup();
    return;
  }
  printf("[DEBUG] Client socket created: %d\n", client);

  char host_string[256];
  strcpy(host_string, address);
  host_string[port_string - address] = 0;
  printf("[DEBUG] Connecting to host: %s\n", host_string);

  struct hostent *host = gethostbyname(host_string);
  if (!host) {
    printf("[DEBUG] gethostbyname failed with error: %d\n", WSAGetLastError());
    closesocket(client);
    WSACleanup();
    return;
  }
  printf("[DEBUG] Host resolved successfully\n");

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy((char *)&addr.sin_addr.s_addr, (char *)host->h_addr, host->h_length);
  addr.sin_port = htons(port);

  printf("[DEBUG] Attempting to connect...\n");
  int connectResult = connect(client, (const struct sockaddr *)&addr, sizeof(addr));
  if (connectResult != 0) {
    printf("[DEBUG] Connect failed with error: %d\n", WSAGetLastError());
    closesocket(client);
    WSACleanup();
    return;
  }
  printf("[DEBUG] Connected successfully!\n");

  struct js_transport_data *data = (struct js_transport_data *)malloc(sizeof(struct js_transport_data));
  data->handle = client;
  
  printf("[DEBUG] Calling js_debugger_attach...\n");
  js_debugger_attach(ctx, js_transport_read, js_transport_write, js_transport_peek, js_transport_close, data);
  printf("[DEBUG] js_debugger_attach completed\n");
}

// todo: fixup asserts to return errors.
static struct sockaddr_in js_debugger_parse_sockaddr(const char *address)
{
  char *port_string = strstr(address, ":");
  assert(port_string);

  int port = atoi(port_string + 1);
  assert(port);

  char host_string[256];
  strcpy(host_string, address);
  host_string[port_string - address] = 0;

  struct hostent *host = gethostbyname(host_string);
  if (!host) {
    int err = WSAGetLastError();
    printf("WSAGetLastError: %i\n", err);
  }
  assert(host);
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy((char *)&addr.sin_addr.s_addr, (char *)host->h_addr, host->h_length);
  addr.sin_port = htons(port);

  return addr;
}

void js_debugger_wait_connection(JSContext* ctx, const char* address) {
    printf("[DEBUG] js_debugger_wait_connection called with address: %s\n", address);
    
	WSADATA wsaData;
	int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        printf("[DEBUG] WSAStartup failed with error: %d\n", wsaResult);
        return;
    }
    printf("[DEBUG] WSAStartup successful\n");

    struct sockaddr_in addr = js_debugger_parse_sockaddr(address);
    printf("[DEBUG] Parsed address successfully\n");

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        printf("[DEBUG] Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }
    printf("[DEBUG] Server socket created: %d\n", server);

    int reuseAddress = 1;
    int sockoptResult = setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddress, sizeof(reuseAddress));
    if (sockoptResult < 0) {
        printf("[DEBUG] setsockopt failed with error: %d\n", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return;
    }
    printf("[DEBUG] Socket options set successfully\n");

    int bindResult = bind(server, (struct sockaddr *)&addr, sizeof(addr));
    if (bindResult < 0) {
        printf("[DEBUG] Bind failed with error: %d\n", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return;
    }
    printf("[DEBUG] Socket bound successfully to %s\n", address);

    int listenResult = listen(server, 1);
    if (listenResult < 0) {
        printf("[DEBUG] Listen failed with error: %d\n", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return;
    }
    printf("[DEBUG] Listening for connections on %s...\n", address);

    struct sockaddr_in client_addr;
    int client_addr_size = sizeof(addr);
    
    printf("[DEBUG] Waiting for connection (blocking accept)...\n");
    int client = accept(server, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client == INVALID_SOCKET) {
        printf("[DEBUG] Accept failed with error: %d\n", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return;
    }
    
    printf("[DEBUG] Client connected! Client socket: %d\n", client);
    
    closesocket(server);
    printf("[DEBUG] Server socket closed, proceeding with debugger attach\n");

    struct js_transport_data* data = (struct js_transport_data*)malloc(sizeof(struct js_transport_data));
    memset(data, 0, sizeof(js_transport_data));
    data->handle = client;
    
    printf("[DEBUG] Calling js_debugger_attach...\n");
    js_debugger_attach(ctx, js_transport_read, js_transport_write, js_transport_peek, js_transport_close, data);
    printf("[DEBUG] js_debugger_attach completed\n");
}
