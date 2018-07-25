#ifndef libnetstation_h
#define libnetstation_h

#include <boost/asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>

#define BOOST_ASIO_ENABLE_OLD_SERVICES

namespace ip = boost::asio::ip;
using io_context = boost::asio::io_service;

namespace NetStation {

constexpr int32_t chartoint(const char c[5]) {
	return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | (c[3] << 0);
}

class EGIConnection {
	using clock_tp = std::chrono::high_resolution_clock::time_point;

protected:
	static const char kQuerySuccess = 'I', kSuccess = 'Z';

	ip::tcp::iostream socket;
	std::chrono::time_point<std::chrono::high_resolution_clock> syncepoch;

	inline void sendCommand(char command, char expected = kSuccess) {
		socket << command << std::flush;
		expect(expected);
	}
	bool sendCommand(const char* command, const int commandSize, char expected=0);
	template <int len> bool sendCommand(const char (&command)[len], char expected=0) {
		return sendCommand(command, len - 1, expected);
	}
	template <class C> bool sendRawStruct(const C& pkt, char expected=0) {
		return sendCommand(reinterpret_cast<const char*>(&pkt), sizeof(C), expected);
	}
	void expect(const char what = 'I');

public:
	EGIConnection(const std::string& address, const std::string& port);
	~EGIConnection() { sendCommand('X'); }
	int64_t synch();
	inline static const clock_tp now() { return std::chrono::high_resolution_clock::now(); }

	inline void trigger(const char code[5], clock_tp timeStamp = now(), int msDuration = 100) {
		trigger(chartoint(code), timeStamp, msDuration);
	}
	void trigger(int32_t code, clock_tp timeStamp = now(), int32_t msDuration = 100);

	void sendBeginRecording() { sendCommand('B'); }
	void sendEndRecording() { sendCommand('E'); }
	// bool sendAttention();
	bool sendSynch(int timeStamp);
};
} // namespace NetStation

#endif
