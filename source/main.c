#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <switch.h>
#include <switch/services/bsd.h>

#define KB(x) ((x) * 1024u)
#define ALIGN_UP(value, alignment) (((value) + ((alignment) - 1u)) & ~((alignment) - 1u))
#define ALIGN_MSS(value) ((((value) + 1499u) / 1500u) * 1500u)

enum {
	ListenPort = 80,
	ListenBacklog = 4,
	RequestBufferSize = 512,
	InnerHeapSize = KB(64),
	SocketCountMax = 4,
	SocketLibraryVersion = 7,
	SocketTcpBufferSize = KB(16),
	SocketUdpSendBufferSize = KB(9),
	SocketUdpReceiveBufferSize = 42240,
	SocketBufferEfficiency = 2,
	SocketTransferMemorySize = ALIGN_UP(
		(ALIGN_MSS(SocketTcpBufferSize) * SocketBufferEfficiency * 2u) * SocketCountMax,
		0x1000u),
};

static const char OkResponse[] =
	"HTTP/1.0 200 OK\r\n"
	"Content-Length: 2\r\n"
	"X-Organization: Nintendo\r\n"
	"\r\n"
	"ok";

u32 __nx_applet_type = AppletType_None;

static alignas(0x1000) uint8_t g_socket_transfer_memory[SocketTransferMemorySize];

void __libnx_initheap(void) {
	static uint8_t inner_heap[InnerHeapSize];
	extern void *fake_heap_start;
	extern void *fake_heap_end;

	fake_heap_start = inner_heap;
	fake_heap_end = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
	auto result = smInitialize();
	if (R_FAILED(result)) {
		diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
	}

	const BsdInitConfig bsd_config = {
		.version = SocketLibraryVersion,
		.tmem_buffer = g_socket_transfer_memory,
		.tmem_buffer_size = sizeof(g_socket_transfer_memory),
		.tcp_tx_buf_size = SocketTcpBufferSize,
		.tcp_rx_buf_size = SocketTcpBufferSize,
		.tcp_tx_buf_max_size = 0,
		.tcp_rx_buf_max_size = 0,
		.udp_tx_buf_size = SocketUdpSendBufferSize,
		.udp_rx_buf_size = SocketUdpReceiveBufferSize,
		.sb_efficiency = SocketBufferEfficiency,
	};

	result = bsdInitialize(&bsd_config, SocketCountMax, BsdServiceType_System);
	if (R_FAILED(result)) {
		diagAbortWithResult(result);
	}

	smExit();
}

void __appExit(void) {
	bsdExit();
}

static void close_socket(int desc) {
	if (desc >= 0) {
		(void)bsdShutdown(desc, SHUT_RDWR);
		(void)bsdClose(desc);
	}
}

static int create_listener(void) {
	const auto desc = bsdSocketExempt(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (desc < 0) {
		return -1;
	}

	const int enable = 1;
	if (bsdSetSockOpt(desc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		close_socket(desc);
		return -1;
	}

	const struct sockaddr_in address = {
		.sin_len = sizeof(address),
		.sin_family = AF_INET,
		.sin_port = htons(ListenPort),
		.sin_addr = {
			.s_addr = htonl(INADDR_LOOPBACK),
		},
	};

	if (bsdBind(desc, (const struct sockaddr *)&address, sizeof(address)) < 0) {
		close_socket(desc);
		return -1;
	}

	if (bsdListen(desc, ListenBacklog) < 0) {
		close_socket(desc);
		return -1;
	}

	return desc;
}

static void send_all(int desc, const char *data, size_t size) {
	size_t sent = 0;

	while (sent < size) {
		const auto cur = bsdSend(desc, data + sent, size - sent, 0);
		if (cur <= 0) {
			return;
		}

		sent += (size_t)cur;
	}
}

static void handle_client(int client) {
	const struct timeval timeout = {
		.tv_sec = 1,
		.tv_usec = 0,
	};
	(void)bsdSetSockOpt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	char request_buffer[RequestBufferSize];
	(void)bsdRecv(client, request_buffer, sizeof(request_buffer), 0);

	send_all(client, OkResponse, sizeof(OkResponse) - 1);
	close_socket(client);
}

static void run_server(int listener) {
	for (;;) {
		struct sockaddr_in client_address = {};
		socklen_t client_address_len = sizeof(client_address);
		const auto client = bsdAccept(listener, (struct sockaddr *)&client_address, &client_address_len);
		if (client < 0) {
			return;
		}

		handle_client(client);
	}
}

int main() {
	while (true) {
		const auto listener = create_listener();
		if (listener >= 0) {
			run_server(listener);
			close_socket(listener);
		}
		svcSleepThread(1000000000LL);
	}
}
