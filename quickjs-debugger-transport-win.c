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
  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) return -1;

  if (length == 0) return -2;

  if (buffer == NULL) return -3;

  // ssize_t ret = read(data->handle, (void *)buffer, length);
  ssize_t ret = recv(data->handle, (void *)buffer, length, 0);

  if (ret == SOCKET_ERROR) return -4;

  if (ret == 0) return -5;

  if (ret > length) return -6;

  return ret;
}

static size_t js_transport_write(void *udata, const char *buffer, size_t length)
{
  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) return -1;

  if (length == 0) return -2;

  if (buffer == NULL) {
    return -3;
  }

  // size_t ret = write(data->handle, (const void *) buffer, length);
  size_t ret = send(data->handle, (const void *)buffer, length, 0);
  if (ret <= 0 || ret > (ssize_t)length) return -4;

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

static void js_transport_close(JSContext *ctx, void *udata)
{
  struct js_transport_data *data = (struct js_transport_data *)udata;
  if (data->handle <= 0) return;

  return;
  close(data->handle);
  data->handle = 0;

  free(udata);

  WSACleanup();
}

void js_debugger_connect(JSContext *ctx, const char *address)
{
  printf("[DEBUG] js_debugger_connect: Starting connection to %s\n", address);

  WSADATA wsaData;
  int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  printf("[DEBUG] js_debugger_connect: WSAStartup result: %d\n", wsa_result);

  char *port_string = strstr(address, ":");
  assert(port_string);
  printf("[DEBUG] js_debugger_connect: Found port string: %s\n", port_string);

  int port = atoi(port_string + 1);
  assert(port);
  printf("[DEBUG] js_debugger_connect: Parsed port: %d\n", port);

  int client = socket(AF_INET, SOCK_STREAM, 0);
  assert(client > 0);
  printf("[DEBUG] js_debugger_connect: Created socket: %d\n", client);
  
  char host_string[256];
  strcpy(host_string, address);
  host_string[port_string - address] = 0;
  printf("[DEBUG] js_debugger_connect: Host string: %s\n", host_string);

  struct hostent *host = gethostbyname(host_string);
  if (!host) {
    int err = WSAGetLastError();
    printf("[DEBUG] js_debugger_connect: gethostbyname failed with error: %d\n", err);
  }
  assert(host);
  printf("[DEBUG] js_debugger_connect: Host resolved successfully\n");

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy((char *)&addr.sin_addr.s_addr, (char *)host->h_addr, host->h_length);
  addr.sin_port = htons(port);
  printf("[DEBUG] js_debugger_connect: Address structure prepared\n");

  //__asm__ volatile("int $0x03");
  printf("[DEBUG] js_debugger_connect: Attempting to connect...\n");
  int connect_result = connect(client, (const struct sockaddr *)&addr, sizeof(addr));
  if (connect_result != 0) {
    int err = WSAGetLastError();
    printf("[DEBUG] js_debugger_connect: connect failed with error: %d\n", err);
  }
  assert(!connect_result);
  printf("[DEBUG] js_debugger_connect: Connected successfully!\n");

  struct js_transport_data *data = (struct js_transport_data *)malloc(sizeof(struct js_transport_data));
  data->handle = client;
  printf("[DEBUG] js_debugger_connect: Calling js_debugger_attach...\n");
  js_debugger_attach(ctx, js_transport_read, js_transport_write, js_transport_peek, js_transport_close, data);
  printf("[DEBUG] js_debugger_connect: js_debugger_attach completed\n");
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
    printf("[DEBUG] js_debugger_wait_connection: Starting server on %s\n", address);
    
	WSADATA wsaData;
	int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	printf("[DEBUG] js_debugger_wait_connection: WSAStartup result: %d\n", wsa_result);

    printf("[DEBUG] js_debugger_wait_connection: Parsing socket address...\n");
    struct sockaddr_in addr = js_debugger_parse_sockaddr(address);
    printf("[DEBUG] js_debugger_wait_connection: Socket address parsed successfully\n");

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        int err = WSAGetLastError();
        printf("[DEBUG] js_debugger_wait_connection: socket creation failed with error: %d\n", err);
    }
    assert(server != INVALID_SOCKET);
    printf("[DEBUG] js_debugger_wait_connection: Server socket created: %d\n", server);

    int reuseAddress = 1;
    int setsockopt_result = setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddress, sizeof(reuseAddress));
    if (setsockopt_result < 0) {
        int err = WSAGetLastError();
        printf("[DEBUG] js_debugger_wait_connection: setsockopt failed with error: %d\n", err);
    }
    assert(setsockopt_result >= 0);
    printf("[DEBUG] js_debugger_wait_connection: Socket options set successfully\n");

    int bind_result = bind(server, (struct sockaddr *)&addr, sizeof(addr));
    if (bind_result < 0) {
        int err = WSAGetLastError();
        printf("[DEBUG] js_debugger_wait_connection: bind failed with error: %d\n", err);
    }
    assert(bind_result >= 0);
    printf("[DEBUG] js_debugger_wait_connection: Socket bound successfully\n");

    int listen_result = listen(server, 1);
    if (listen_result < 0) {
        int err = WSAGetLastError();
        printf("[DEBUG] js_debugger_wait_connection: listen failed with error: %d\n", err);
    }
    printf("[DEBUG] js_debugger_wait_connection: Listening for connections...\n");

    struct sockaddr_in client_addr;
    int client_addr_size = sizeof(client_addr);
    printf("[DEBUG] js_debugger_wait_connection: Waiting for client connection...\n");
    int client = accept(server, (struct sockaddr*)&client_addr, &client_addr_size);
    //close(server);
    if (client == INVALID_SOCKET) {
        int err = WSAGetLastError();
        printf("[DEBUG] js_debugger_wait_connection: accept failed with error: %d\n", err);
    }
    assert(client != INVALID_SOCKET);
    printf("[DEBUG] js_debugger_wait_connection: Client connected: %d\n", client);

    struct js_transport_data* data = (struct js_transport_data*)malloc(sizeof(struct js_transport_data));
    memset(data, 0, sizeof(js_transport_data));
    data->handle = client;
    printf("[DEBUG] js_debugger_wait_connection: Calling js_debugger_attach...\n");
    js_debugger_attach(ctx, js_transport_read, js_transport_write, js_transport_peek, js_transport_close, data);
    printf("[DEBUG] js_debugger_wait_connection: js_debugger_attach completed\n");
}
