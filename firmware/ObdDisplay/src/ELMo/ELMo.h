/* ELMo 1.1.2, Copyright (c) 2021 Rudy de Lorenzo, MIT License.
 * Modified for OpenNextion; see README.md and LICENSE in this directory.
 */
#ifndef ELMo_h
#define ELMo_h
#include <Arduino.h>
#include <WiFi.h>
#include "../../ObdTransaction.h"

class ELMo {
private:
	bool _initialized;
	String _sendUninitialized(String message);
	bool _debug;
	int _baudRate;
	int _responseTimeout;
	bool _sending = false;

public:
	ELMo();
	String send(String message);
	// Managed-session API: caller owns TCP connect and adapter initialization.
	// No implicit commands. Complete raw reply (without prompt), CR/LF -> LF.
	enum class Status { Ok, Disconnected, Timeout, Overflow, WriteError,
	                    InvalidCommand, OutOfMemory };
	bool send(const char *message, String &received, uint32_t timeoutMs,
	          void (*idle)() = nullptr);
	Status lastStatus() const { return _lastStatus; }
	// Typed incremental session used by firmware, including async DTC receive.
	obd_transport::Transaction transaction;
	bool begin(obd_policy::Command command, uint32_t now, uint32_t timeoutMs) {
		return !_sending && transaction.begin(client, command, now, timeoutMs);
	}
	void poll(uint32_t now) { transaction.poll(client, now); }
	bool initialize();
	bool stop();
	bool connected();
	bool getDebug();
	bool setDebug(bool value);
	int getBaud();
	int setBaud(int rate);
	int getTimeout();
	int setTimeout(int seconds);

	WiFiClient client;
	IPAddress ip;
	int port;

private:
	Status _lastStatus = Status::Disconnected;
};

#endif
