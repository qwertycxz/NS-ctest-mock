#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <switch.h>
#include <switch/services/bsd.h>
#include <threads.h>

/**
 * Exit the BSD service when the application exits.
 */
void __appExit() {
	bsdExit();
}

// clang-format off
/**
 * Response for connection test.
 * @see http://ctest.cdn.nintendo.net
 */
static const char HTTP_RESPONSE[] =
	"HTTP/1.0 200 OK\r\n"
	"Content-Length: 2\r\n"
	"X-Organization: Nintendo\r\n"
	"\r\n"
	"ok";
// clang-format on

/**
 * Constants.
 */
enum {
	ADDRESS_LENGTH = sizeof(struct sockaddr_in),
	BUFFER_LENGTH = 0x8000,
	HTTP_LENGTH = sizeof(HTTP_RESPONSE) - 1,
	OPTION_LENGTH = sizeof(uint32_t),
	PAGE_LENGTH = 0x1000,
};

/**
 * Listen 127.0.0.1:80 and respond with a simple HTTP response.
 * @return no return on success, and an errno value on failure.
 */
int main() {
	do {
		const auto server = bsdSocketExempt(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (server < 0) continue;

		struct sockaddr_in address = {
			.sin_len = ADDRESS_LENGTH,
			.sin_family = AF_INET,
			.sin_port = htons(80),
		};

		if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
			bsdSetSockOpt(server, SOL_SOCKET, SO_REUSEADDR, &(bool) { true }, OPTION_LENGTH) ||
			bsdBind(server, (const struct sockaddr* const)&address, ADDRESS_LENGTH) ||
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
	while (!thrd_sleep(
		&(const struct timespec) {
			.tv_sec = 1,
		},
		nullptr
	));
	return errno;
}

/**
 * TCP transfer memory buffer for the BSD service.
 */
static alignas(PAGE_LENGTH) uint8_t transfer_buffer[BUFFER_LENGTH];

/**
 * Initialize the BSD service.
 */
void __appInit() {
	const auto manager = smInitialize();
	if (R_FAILED(manager)) {
		diagAbortWithResult(manager);
	}

	const auto bsd = bsdInitialize(
		&(BsdInitConfig) {
			.version = 2,
			.tmem_buffer = transfer_buffer,
			.tmem_buffer_size = BUFFER_LENGTH,
			.tcp_tx_buf_size = PAGE_LENGTH,
			.tcp_rx_buf_size = PAGE_LENGTH,
			.sb_efficiency = 1,
		},
		1,
		BsdServiceType_Auto
	);
	if (R_FAILED(bsd)) {
		diagAbortWithResult(bsd);
	}
	smExit();
}

/**
 * Fake heap for libnx.
 */
extern void* fake_heap_start;

/**
 * Fake heap end for libnx.
 */
extern void* fake_heap_end;

/**
 * Our fake heap. Only 1 page.
 */
static alignas(PAGE_LENGTH) uint8_t fake_heap[PAGE_LENGTH];

/**
 * Initialize the heap for libnx.
 */
void __libnx_initheap() {
	fake_heap_start = fake_heap;
	fake_heap_end = fake_heap + PAGE_LENGTH;
}

/**
 * Applet type. None.
 */
const uint32_t __nx_applet_type = AppletType_None;
