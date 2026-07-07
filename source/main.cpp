#include <stratosphere.hpp>
#include <stratosphere/rapidjson/stream.h>

#include <cstring>

namespace ams::nsctest {

	namespace {

		constexpr const char ConfigPath[] = "sdmc:/config/NS-ctest-server/config.json";

		constexpr u16 ListenPort = 80;
		constexpr int ListenBacklog = 4;

		constexpr size_t ConfigBufferSize = 512;
		constexpr size_t RequestBufferSize = 512;

		constexpr const char OkResponse[] =
			"HTTP/1.0 200 OK\r\n"
			"Content-Length: 2\r\n"
			"X-Organization: Nintendo\r\n"
			"\r\n"
			"ok";

		constexpr size_t FsHeapSize = 16_KB;
		alignas(os::MemoryPageSize) constinit u8 g_fs_heap_memory[FsHeapSize];
		constinit lmem::HeapHandle g_fs_heap_handle = nullptr;

		using SocketConfigType = socket::SystemConfigLightDefault;
		constexpr int SocketCountMax = 4;
		constexpr size_t SocketAllocatorSize = SocketConfigType::DefaultAllocatorPoolSize;
		constexpr size_t SocketMemoryPoolSize = util::AlignUp(SocketConfigType::PerTcpSocketWorstCaseMemoryPoolSize * SocketCountMax, os::MemoryPageSize);
		constexpr size_t SocketRequiredSize = util::AlignUp(SocketMemoryPoolSize + SocketAllocatorSize, os::MemoryPageSize);
		alignas(os::MemoryPageSize) constinit u8 g_socket_memory[SocketRequiredSize];
		constexpr inline const SocketConfigType SocketConfig(g_socket_memory, sizeof(g_socket_memory), SocketAllocatorSize, SocketCountMax);

		struct ServerConfig {
			socket::InAddrT listen_address;
		};

		void *AllocateForFs(size_t size) {
			return lmem::AllocateFromExpHeap(g_fs_heap_handle, size);
		}

		void DeallocateForFs(void *p, size_t size) {
			AMS_UNUSED(size);
			lmem::FreeToExpHeap(g_fs_heap_handle, p);
		}

		void InitializeFsHeap() {
			g_fs_heap_handle = lmem::CreateExpHeap(g_fs_heap_memory, sizeof(g_fs_heap_memory), lmem::CreateOption_None);
		}

		bool StringEquals(const char *str, rapidjson::SizeType len, const char *expected) {
			return std::strlen(expected) == len && std::memcmp(str, expected, len) == 0;
		}

		bool ParseIpv4Address(socket::InAddrT *out, const char *str, size_t len) {
			if (len == 0 || len > 15) {
				return false;
			}

			u32 octets[4] = {};
			size_t octet_index = 0;
			size_t digit_count = 0;
			u32 value = 0;

			for (size_t i = 0; i < len; ++i) {
				const char c = str[i];
				if ('0' <= c && c <= '9') {
					value = value * 10 + static_cast<u32>(c - '0');
					++digit_count;
					if (digit_count > 3 || value > 255) {
						return false;
					}
				} else if (c == '.') {
					if (digit_count == 0 || octet_index >= 3) {
						return false;
					}
					octets[octet_index++] = value;
					value = 0;
					digit_count = 0;
				} else {
					return false;
				}
			}

			if (digit_count == 0 || octet_index != 3) {
				return false;
			}
			octets[octet_index] = value;

			const u32 host_order = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
			*out = socket::InetHtonl(host_order);
			return true;
		}

		class ConfigJsonHandler final : public rapidjson::BaseReaderHandler<rapidjson::UTF8<char>, ConfigJsonHandler> {
			private:
				ServerConfig *m_config;
				bool m_current_key_is_listen_ip;

			public:
				explicit ConfigJsonHandler(ServerConfig *config) : m_config(config), m_current_key_is_listen_ip(false) { /* ... */ }

				bool Key(const char *str, rapidjson::SizeType len, bool copy) {
					AMS_UNUSED(copy);
					m_current_key_is_listen_ip = StringEquals(str, len, "listen_ip");
					return true;
				}

				bool String(const char *str, rapidjson::SizeType len, bool copy) {
					AMS_UNUSED(copy);

					if (m_current_key_is_listen_ip) {
						socket::InAddrT address;
						if (ParseIpv4Address(std::addressof(address), str, len)) {
							m_config->listen_address = address;
						}
					}

					m_current_key_is_listen_ip = false;
					return true;
				}

				bool Null() { return this->ResetValueKey(); }
				bool Bool(bool) { return this->ResetValueKey(); }
				bool Int(int) { return this->ResetValueKey(); }
				bool Uint(unsigned) { return this->ResetValueKey(); }
				bool Int64(s64) { return this->ResetValueKey(); }
				bool Uint64(u64) { return this->ResetValueKey(); }
				bool Double(double) { return this->ResetValueKey(); }
				bool StartObject() { return true; }
				bool EndObject(rapidjson::SizeType) { return true; }
				bool StartArray() { return this->ResetValueKey(); }
				bool EndArray(rapidjson::SizeType) { return true; }

			private:
				bool ResetValueKey() {
					m_current_key_is_listen_ip = false;
					return true;
				}
		};

		ServerConfig GetDefaultConfig() {
			return {
				.listen_address = socket::InAddr_Any,
			};
		}

		ServerConfig LoadConfig() {
			ServerConfig config = GetDefaultConfig();

			fs::FileHandle file;
			if (R_FAILED(fs::OpenFile(std::addressof(file), ConfigPath, fs::OpenMode_Read))) {
				return config;
			}
			ON_SCOPE_EXIT { fs::CloseFile(file); };

			s64 file_size = 0;
			if (R_FAILED(fs::GetFileSize(std::addressof(file_size), file)) || file_size <= 0 || file_size >= static_cast<s64>(ConfigBufferSize)) {
				return config;
			}

			char buffer[ConfigBufferSize] = {};
			size_t read_size = 0;
			if (R_FAILED(fs::ReadFile(std::addressof(read_size), file, 0, buffer, static_cast<size_t>(file_size))) || read_size != static_cast<size_t>(file_size)) {
				return config;
			}
			buffer[read_size] = '\x00';

			rapidjson::Reader reader;
			rapidjson::InsituStringStream stream(buffer);
			ConfigJsonHandler handler(std::addressof(config));
			if (!reader.Parse<rapidjson::kParseInsituFlag>(stream, handler)) {
				return GetDefaultConfig();
			}

			return config;
		}

		void CloseSocket(s32 desc) {
			if (desc >= 0) {
				socket::Shutdown(desc, socket::ShutdownMethod::Shut_RdWr);
				socket::Close(desc);
			}
		}

		s32 CreateListener(socket::InAddrT listen_address) {
			const s32 desc = socket::SocketExempt(socket::Family::Af_Inet, socket::Type::Sock_Stream, socket::Protocol::IpProto_Tcp);
			if (desc < 0) {
				return -1;
			}

			u32 enable = 1;
			if (socket::SetSockOpt(desc, socket::Level::Sol_Socket, socket::Option::So_ReuseAddr, std::addressof(enable), sizeof(enable)) < 0) {
				CloseSocket(desc);
				return -1;
			}

			const socket::SockAddrIn sockaddr = {
				.sin_len = 0,
				.sin_family = socket::Family::Af_Inet,
				.sin_port = socket::InetHtons(ListenPort),
				.sin_addr = { listen_address },
				.sin_zero = {},
			};

			if (socket::Bind(desc, reinterpret_cast<const socket::SockAddr *>(std::addressof(sockaddr)), sizeof(sockaddr)) < 0) {
				CloseSocket(desc);
				return -1;
			}

			if (socket::Listen(desc, ListenBacklog) < 0) {
				CloseSocket(desc);
				return -1;
			}

			return desc;
		}

		void SendAll(s32 desc, const char *data, size_t size) {
			size_t sent = 0;
			while (sent < size) {
				const ssize_t cur = socket::Send(desc, data + sent, size - sent, socket::MsgFlag::Msg_None);
				if (cur <= 0) {
					return;
				}
				sent += static_cast<size_t>(cur);
			}
		}

		void HandleClient(s32 client) {
			const socket::TimeVal timeout = {
				.tv_sec = 1,
				.tv_usec = 0,
			};
			socket::SetSockOpt(client, socket::Level::Sol_Socket, socket::Option::So_RcvTimeo, std::addressof(timeout), sizeof(timeout));

			char request_buffer[RequestBufferSize];
			socket::Recv(client, request_buffer, sizeof(request_buffer), socket::MsgFlag::Msg_None);

			SendAll(client, OkResponse, sizeof(OkResponse) - 1);
			CloseSocket(client);
		}

		void RunServer(s32 listener) {
			while (true) {
				socket::SockAddrIn client_address = {};
				socket::SockLenT client_address_len = sizeof(client_address);
				const s32 client = socket::Accept(listener, reinterpret_cast<socket::SockAddr *>(std::addressof(client_address)), std::addressof(client_address_len));
				if (client < 0) {
					return;
				}

				HandleClient(client);
			}
		}

	}

	void Initialize() {
		InitializeFsHeap();

		R_ABORT_UNLESS(sm::Initialize());

		fs::InitializeForSystem();
		fs::SetAllocator(AllocateForFs, DeallocateForFs);
		fs::SetEnabledAutoAbort(false);

		const Result mount_result = fs::MountSdCard("sdmc");
		AMS_UNUSED(mount_result);

		R_ABORT_UNLESS(socket::Initialize(SocketConfig));

		ams::CheckApiVersion();
	}

	void Loop() {
		while (true) {
			const ServerConfig config = LoadConfig();
			const s32 listener = CreateListener(config.listen_address);
			if (listener >= 0) {
				RunServer(listener);
				CloseSocket(listener);
			}

			os::SleepThread(TimeSpan::FromSeconds(1));
		}
	}

}

namespace ams::init {

	void InitializeSystemModule() {
		nsctest::Initialize();
	}

	void FinalizeSystemModule() { /* ... */ }

	void Startup() { /* ... */ }

}

namespace ams {

	void Main() {
		os::SetThreadNamePointer(os::GetCurrentThread(), "NS-ctest-server");
		nsctest::Loop();
	}

}
