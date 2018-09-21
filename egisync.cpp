#include "netstation.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <condition_variable>
#include <forward_list>
#include <iostream>
#include <lsl_cpp.h>
#include <mutex>
#include <queue>
#include <thread>

namespace ip = boost::asio::ip;
using socket_stream = ip::tcp::iostream;
using Time = std::chrono::high_resolution_clock::time_point;

template <typename Duration> inline double timediff_secs(Duration d) {
	return std::chrono::nanoseconds(d).count() / 1000000000.;
}

// Locking MPSC Queue
struct TriggerQueue {
	using value_type = std::pair<Time, char>;
	std::condition_variable newcontent;
	std::mutex queuelock;
	std::queue<value_type> triggers;
	std::atomic<bool> closed{false};

	bool enqueue(Time tp, char c) {
		if (closed) return false;
		std::lock_guard<std::mutex> lock(queuelock);
		triggers.push(std::make_pair(tp, c));
		newcontent.notify_one();
		return true;
	}
	bool try_pop(value_type& res,
	             const std::chrono::milliseconds& timeout = std::chrono::milliseconds(500)) {
		if (closed) return false;

		std::unique_lock<std::mutex> lock(queuelock);
		if (!newcontent.wait_for(lock, timeout, [this] { return !triggers.empty(); })) return false;

		res = triggers.front();
		triggers.pop();
		return true;
	}
};

class TriggerSourceContainer {
public:
	template <typename... Args>
	void addSource(void (*sourceFn)(TriggerQueue&, Args&&...), Args&&... args) {
		threads.emplace_front(std::thread(sourceFn, queue, std::forward(args)...));
	}

	virtual ~TriggerSourceContainer() {}

public:
	friend class TriggerSource;
	TriggerQueue queue;
	std::forward_list<std::thread> threads;
};

class TriggerSource {
public:
	template <typename... Args>
	TriggerSource(TriggerSourceContainer& ts, void (*sourceFn)(TriggerSourceContainer&, Args&&...),
	              Args&&... args) {
		ts.threads.emplace_front(std::thread(sourceFn, std::forward(args)...));
	}
};

class TriggerSink {
public:
	using Time = std::chrono::high_resolution_clock::time_point;
	virtual void trigger(char c, Time tp) = 0;
	virtual ~TriggerSink() = default;
};

class EGITriggerSink : public TriggerSink {
private:
	NetStation::EGIConnection connection;

public:
	EGITriggerSink(const char* address, const char* port = "55513") : connection(address, port) {
		std::cout << "time difference between EGI and local clock: " << connection.synch() << std::endl;
	}
	virtual void trigger(char c, Time tp) override;
	virtual ~EGITriggerSink() override = default;
};
void EGITriggerSink::trigger(char c, TriggerSink::Time tp) {
	auto triggercode = static_cast<int32_t>(c);
	connection.trigger(triggercode, tp);
}

class OStreamTriggerSink : public TriggerSink {
private:
	std::ostream& os_;
	Time start;

public:
	OStreamTriggerSink(std::ostream& os)
	    : os_(os), start(std::chrono::high_resolution_clock::now()) {
		os_.precision(5);
	}
	virtual void trigger(char c, Time tp) override;
	virtual ~OStreamTriggerSink() override = default;
};
void OStreamTriggerSink::trigger(char c, TriggerSink::Time tp) {
	os_ << "Trigger " << timediff_secs(tp - start) << ": " << c << ' ' << static_cast<int>(c)
	    << '\n';
}

class LSLTriggerSink : public TriggerSink {
private:
	lsl::stream_outlet outlet;

public:
	LSLTriggerSink(const std::string& name)
	    : outlet(lsl::stream_info(name, "trigger", 1, lsl::IRREGULAR_RATE, lsl::cf_string), 1) {
		std::cout << "Create LSL outlet '" << name << "'\n";
	}

	virtual void trigger(char c, Time tp) override {
		outlet.push_sample(&c, timediff_secs(tp.time_since_epoch()));
	}
	virtual ~LSLTriggerSink() override = default;
};

class TriggerSinkContainer {
private:
	std::vector<TriggerSink*> sinks;

public:
	void trigger(char c, Time tp = std::chrono::high_resolution_clock::now()) {
		for (TriggerSink* sink : sinks) sink->trigger(c, tp);
	}

	template <class C, typename... Args> C* addSink(Args&&... args) {
		C* sink = new C(std::forward<Args>(args)...);
		sinks.push_back(sink);
		return sink;
	}

	template <class C, typename... Args> bool tryAddSink(Args&&... args) {
		try {
			addSink<C>(std::forward<Args>(args)...);
			return true;
		} catch (std::exception& e) { std::cout << "Error adding sink: " << e.what() << std::endl; }
		return false;
	}
};

int main(int argc, char** argv) {
	TriggerSinkContainer sinks;
	{
		std::cout << "Connecting to NetStation..." << std::endl;
		const char* address = argc > 1 ? argv[1] : "127.0.0.1";
		const char* port = argc > 2 ? argv[2] : "55513";
		try {
			sinks.addSink<EGITriggerSink>(address, port);
		} catch (std::exception& e) {
			std::cout << "Error connecting to NetStation: " << e.what() << std::endl;
			return 1;
		}
	}
	std::cout << "Creating LSL outlet" << std::endl;
	sinks.addSink<LSLTriggerSink>("egisync");
	std::cout << "Creating cout outlet" << std::endl;
	sinks.addSink<OStreamTriggerSink>(std::cout);

	std::cout << "Sending test triggers" << std::endl;
	sinks.trigger(1);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	sinks.trigger(2);

	const int16_t listenport = 2000;
	std::cout << "Listening on port " << listenport << " for triggers" << std::endl;
	io_context svc;
	ip::tcp::acceptor acc(svc, ip::tcp::endpoint(ip::tcp::v4(), listenport));
	acc.listen();
	while (true) {
		ip::tcp::iostream stream;
		// socket_stream server;
		acc.accept(*stream.rdbuf());
		std::cout << "Accepted, now waiting for triggers..." << std::endl;
		char c;
		while (stream.good() && (c = static_cast<char>(stream.get())) > 0) { sinks.trigger(c); }
	}

	return 0;
}
