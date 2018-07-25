#include <chrono>
#include <cstdint>
#include <cstring>

#include "netstation.hpp"
#include <iostream>

namespace NetStation {

#pragma pack(1)
struct TriggerPacket {
	const char _command = 'D';
	const uint16_t _dataSize = sizeof(TriggerPacket) - sizeof(_command) - sizeof(_dataSize);
	const int32_t _timeStamp;
	const int32_t _msDuration;
	int32_t _code;
	//const char _end[13] = {0};
	TriggerPacket(int32_t code, int32_t timeStamp, int32_t msDuration = 100)
	    : _timeStamp(timeStamp), _msDuration(msDuration), _code(code) {}
};
struct SynchPacket {
	const char cmd = 'T';
	const int32_t _timeStamp;
	SynchPacket(int32_t timeStamp) : _timeStamp(timeStamp) {}
};
#pragma pack()

template<typename T>
inline std::ostream& debugchar(std::ostream& os, const T c) {
	return os << '"' << c << "\" (" << std::hex << static_cast<int>(c) << std::dec << ')';
}

template<typename T>
inline std::string debugchar(const T c) {
	std::ostringstream os;
	debugchar(os, c);
	return os.str();
}

void handleResp(ip::tcp::iostream& socket) {
	auto resp = socket.get();
	switch (resp) {
	case 'F': throw std::runtime_error("Received error from NetStation");
	case 'I': return;
	default: throw std::runtime_error("Unknown response " + debugchar(resp));
	}
}

void EGIConnection::expect(const char what) {
	auto resp = static_cast<char>(socket.get());
	if (resp != what)
		std::cerr << "\nExpected " + debugchar(what) + ", got " + debugchar(resp)+"\nCode: "
	                             + static_cast<char>(socket.get()) << std::endl;
}

void getExtra(ip::tcp::iostream& socket) {
	auto b = socket.rdbuf()->available();
	if(!b) return;
	char* buf = new char[b+1];
	buf[b]=0;
	socket.readsome(buf, b);
	std::cout << "Got " << b << " extra bytes: '";
	for(uint i=0; i<b; i++) std::cout << static_cast<int>(buf[i]) << ' ';
	std::cout << '\'' << std::endl;
	delete[] buf;
}

bool EGIConnection::sendCommand(const char* command, const int commandSize, char expected)
{
	getExtra(socket);
	socket.write(command, commandSize);
	socket.flush();
	if(expected) expect(expected);
	return true;
}

EGIConnection::EGIConnection(const std::string& address, const std::string& port) {
	socket.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);

	// Connection timeout
	//socket.expires_from_now(boost::posix_time::seconds(2));
	socket.connect(ip::tcp::resolver::query(address, port));
	socket.rdbuf()->set_option(ip::tcp::no_delay(true));
	//socket.expires_from_now(boost::posix_time::hours(24 * 365));

	// Tell the NetStation we're little endian
	sendCommand("QNTEL", 'I');
	expect(1);
	sendCommand('B', 'Z');
	getExtra(socket);
}

int64_t EGIConnection::synch() {
	sendCommand('A', 'Z');
	auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::high_resolution_clock::now() - syncepoch);
	for (int n = 0; n < 100 && diff_ms > std::chrono::milliseconds(2); ++n) {
		syncepoch = std::chrono::high_resolution_clock::now();
		sendRawStruct(SynchPacket(0), 'Z');
		diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		    std::chrono::high_resolution_clock::now() - syncepoch);
	}
	return diff_ms.count();
}

void EGIConnection::trigger(int32_t code, clock_tp timeStamp, int32_t msDuration) {
	const int32_t ts = static_cast<int32_t>(
	    std::chrono::duration_cast<std::chrono::milliseconds>(timeStamp - syncepoch).count());
	sendRawStruct(TriggerPacket(code, ts, msDuration));
	std::cout << "Sent EGI trigger " << code << " at " << ts << std::endl;
	expect('Z');
}

} // namespace NetStation
