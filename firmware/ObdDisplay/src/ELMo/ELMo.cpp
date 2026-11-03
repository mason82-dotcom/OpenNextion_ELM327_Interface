#include <Arduino.h>
#include "ELMo.h"

#define TIMEOUT "ELM TIMEOUT"
#define NO_DATA "NO DATA"
#define NOT_INITIALIZED "NOT INITIALIZED"

ELMo::ELMo() {
	ip = IPAddress(192, 168, 0, 10);
	port = 35000;
	_debug = false;
	_responseTimeout = 60;
	_initialized = false;
	_baudRate = 115200;
}

// Local integration changes: see README.md alongside this file.
bool ELMo::send(const char *message, String &received, uint32_t timeoutMs,
                void (*idle)()) {
    // Nested calls from an idle/UI callback must never interleave requests.
    if (_sending || transaction.state()!=obd_transport::State::Idle) return false;
    received = "";
    if (!message || !*message || timeoutMs == 0) {
        _lastStatus = Status::InvalidCommand;
        return false;
    }
    char frame[34];
    size_t length = 0;
    for (; message[length]; ++length) {
        const unsigned char c = static_cast<unsigned char>(message[length]);
        if (length >= sizeof(frame) - 2 || c < 32 || c > 126) {
            _lastStatus = Status::InvalidCommand;
            return false;
        }
        frame[length] = static_cast<char>(c);
    }
    frame[length++] = '\r'; // one write, no extra LF / repeated ELM command
    if (!client.connected()) {
        _lastStatus = Status::Disconnected;
        return false;
    }
    if (!received.reserve(64)) {
        _lastStatus = Status::OutOfMemory;
        client.stop();
        _initialized = false;
        return false;
    }
    _sending = true;
    if (client.write(reinterpret_cast<const uint8_t *>(frame), length) != length) {
        _lastStatus = Status::WriteError;
    } else {
        const uint32_t started = millis();
        _lastStatus = Status::Timeout;
        size_t rawBytes = 0;
        while (static_cast<uint32_t>(millis() - started) < timeoutMs) {
            if (idle) idle();
            // Bound each burst, even if a peer streams non-printable bytes.
            unsigned burst = 0;
            while (client.available() && burst++ < 64) {
                const int value = client.read();
                if (value < 0) break;
                const char c = static_cast<char>(value);
                if (c == '>') {
                    _lastStatus = Status::Ok;
                    _sending = false;
                    return true;
                }
                if (++rawBytes > 1024) {
                    _lastStatus = Status::Overflow;
                    break;
                }
                if (c == '\r' || c == '\n') {
                    if (!received.concat('\n')) _lastStatus = Status::OutOfMemory;
                } else if (c >= 32 && c <= 126) {
                    if (!received.concat(c)) _lastStatus = Status::OutOfMemory;
                }
                if (_lastStatus != Status::Timeout) break;
            }
            if (_lastStatus != Status::Timeout) break;
            if (!client.connected()) {
                _lastStatus = Status::Disconnected;
                break;
            }
            delay(2); // yield to ESP32 networking/watchdog, keep LVGL responsive
        }
    }
    // An incomplete response can never belong to the next request.
    client.stop();
    _initialized = false;
    _sending = false;
    return false;
}

String ELMo::send(String message) {
    if (!_initialized) return NOT_INITIALIZED;
    return _sendUninitialized(message);
}

String ELMo::_sendUninitialized(String message) {
    String received;
    if (!send(message.c_str(), received,
              static_cast<uint32_t>(_responseTimeout) * 1000U))
        return TIMEOUT;
    if (_debug) {
        Serial.print("ELMo: Received: ");
        Serial.println(received);
    }
    if (message.indexOf("AT") == -1 && received.indexOf(NO_DATA) > -1)
        return NO_DATA;
    return received;
}

bool ELMo::initialize() {
	_initialized = false;
	if (client.connect(ip, port)) {
	    if (_debug) Serial.println("ELMo: Connected to ELM327");
	    _sendUninitialized("AT Z");
	    _sendUninitialized("AT E0"); // disables echo
	    _sendUninitialized("AT H0"); // disables headers
	    if (!_sendUninitialized("AT SP 0").substring(0, 2).equals("OK")) return false;
	    if (_sendUninitialized("0100").indexOf("UNABLE TO CONNECT") > -1) return false; // queries for supported PIDs and prepares ELM

	    if (_debug) Serial.println("ELMo: Initialized ELM327");
	}
	else {
	    if (_debug) Serial.println("ELMo: Connection to ELM failed");
	    return false;
	}
	_initialized = true;
	return true;
}

bool ELMo::getDebug() {
	return _debug;
}

bool ELMo::setDebug(bool value) {
	if (value == false) {
		_debug = false;
		return _debug;
	} else {
		if (!Serial) {
			Serial.begin(_baudRate);
			unsigned long startTime = millis();

		  	while (!Serial && ((millis() - startTime) < 10000));

		  	if (!Serial) {
		  		_debug = false;
		  		return _debug;
		  	} else {
		  		_debug = true;
		  		return _debug;
		  	}
		} else {
			_debug = true;
			return _debug;
		}
		
	}
	
}

int ELMo::getBaud() {
	return _baudRate;
}

int ELMo::setBaud(int rate) {
	if (!Serial) {
		_baudRate = rate;
		return _baudRate;
	} else {
		Serial.end();

		_baudRate = rate;
		Serial.begin(_baudRate);
		unsigned long startTime = millis();

	  	while (!Serial && ((millis() - startTime) < 10000));

	  	if (!Serial) {
	  		return 0;
	  	} else {
	  		return _baudRate;
	  	}
	}
}

bool ELMo::connected() {
	bool conn = client.connected();
	if (!conn) {
		_initialized = false;
		if (_debug) Serial.println("ELMo: CONNECTION LOST, ELM UNINITIALIZED");
	}
	return conn;
}

bool ELMo::stop() {
	// Socket-only close: never send an implicit adapter reset.
	_initialized = false;
	client.stop();
	return true;
}

int ELMo::getTimeout() {
	return _responseTimeout;
}

int ELMo::setTimeout(int seconds) {
	_responseTimeout = seconds;
	return _responseTimeout;
}
