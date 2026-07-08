#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <switch.h>
#include <switch/services/bsd.h>
#include <threads.h>

#define KB(x) ((x) * 1024u)
#define ALIGN_UP(value, alignment) (((value) + ((alignment) - 1u)) & ~((alignment) - 1u))
#define ALIGN_MSS(value) ((((value) + 1499u) / 1500u) * 1500u)

enum {
	RequestBufferSize = 512,
	InnerHeapSize = KB(64),
	SocketCountMax = 4,
	SocketLibraryVersion = 7,
	SocketTcpBufferSize = KB(16),
	SocketUdpSendBufferSize = KB(9),
	SocketUdpReceiveBufferSize = 42240,
	SocketBufferEfficiency = 2,
	SocketTransferMemorySize = ALIGN_UP((ALIGN_MSS(SocketTcpBufferSize) * SocketBufferEfficiency * 2u) * SocketCountMax, 0x1000u),
};

u32 __nx_applet_type = AppletType_None;

static alignas(0x1000) uint8_t g_socket_transfer_memory[SocketTransferMemorySize];

void __libnx_initheap(void) {
	static uint8_t inner_heap[InnerHeapSize];
	extern void* fake_heap_start;
	extern void* fake_heap_end;

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

// clang-format off
static const char HTTP_RESPONSE[] =
	"HTTP/1.0 200 OK\r\n"
	"Content-Length: 2\r\n"
	"X-Organization: Nintendo\r\n"
	"\r\n"
	"ok";
// clang-format on

static const auto ADDRESS_LENGTH = sizeof(struct sockaddr_in);
static const auto HTTP_LENGTH = sizeof(HTTP_RESPONSE) - 1;
static const auto OPTION_LENGTH = sizeof(uint32_t);

int main() {
	do {
		const auto server = bsdSocketExempt(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (server < 0) continue;

		if (bsdSetSockOpt(server, SOL_SOCKET, SO_NOSIGPIPE, &(bool) { true }, OPTION_LENGTH) ||
			bsdSetSockOpt(server, SOL_SOCKET, SO_REUSEADDR, &(bool) { true }, OPTION_LENGTH) ||
			bsdBind(
				server,
				(const struct sockaddr* const)&(const struct sockaddr_in) {
					.sin_len = ADDRESS_LENGTH,
					.sin_family = AF_INET,
					.sin_port = htons(80),
					.sin_addr = {
						.s_addr = htonl(INADDR_LOOPBACK),
					},
				},
				ADDRESS_LENGTH
			) ||
			bsdListen(server, SOMAXCONN)) {
			goto close;
		}

		while (true) {
			const auto client = bsdAccept(server, nullptr, nullptr);
			if (client < 0) break;

			auto cursor = 0;
			while (cursor < HTTP_LENGTH) {
				const auto sent = bsdSend(client, HTTP_RESPONSE + cursor, HTTP_LENGTH - cursor, 0);
				if (sent <= 0) break;
				cursor += sent;
			}
			bsdClose(client);
		}

	close:
		bsdClose(server);
	}
	while (!thrd_sleep(&(const struct timespec) { 1 }, nullptr));
	return errno;
}
