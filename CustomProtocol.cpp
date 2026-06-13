// CustomProtocol.cpp : Defines the entry point for the application.
//

#include "CustomProtocol.h"

using namespace std;
using json = nlohmann::json;
namespace fs = filesystem;

//Settings variables
float meanLatencyMS;
float sdLatency;
float packetDropRatePercentage;
uint32_t fileSizeB;

uint16_t RTTMS;
float SSCTMS;
float SRCTMS;
uint32_t windowSizeB;

string dbPath;
float runtimeS;

//Packet declaration
struct Packet {
	uint16_t sourcePort;
	uint16_t destPort;
	uint32_t sequenceNumber;
	uint16_t checkSum;
	uint8_t flags;
	uint8_t version; //0x00
	vector<uint8_t> payload; //Max size = 1448 entries

	bool isEmptyPacket = false; //This is used for packet loss

	uint16_t CalculateChecksum();
};

//Packet functions
uint16_t Packet::CalculateChecksum() {
	//Done using 16-bit CRC
	//Divisor = 0x8005 

	//Data preprocessing
	//724 payload entries + 5 header entries not including checksum
	uint16_t data[729] = { 0 };
	int length = sizeof(data) / sizeof(data[0]);

	data[0] = sourcePort;
	data[1] = destPort;
	data[2] = static_cast<uint16_t>(sequenceNumber >> 8);
	data[3] = static_cast<uint16_t>(sequenceNumber);
	data[4] = (static_cast<uint16_t>(flags) << 8) + static_cast<uint16_t>(version);

	for (int i = 5; i < length; i++) data[i] = (static_cast<uint16_t>(payload[2 * (i - 5)]) << 8) + static_cast<uint16_t>(payload[2 * (i - 5) + 1]);

	//CRC
    uint16_t crc = 0x0000;

    for (size_t i = 0; i < length; ++i) {
        crc ^= (data[i] << 8);

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            }
            else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

//Helper functions

uniform_int_distribution<uint32_t> uniformUInt32Distribution{0, UINT32_MAX };
void GenerateRandomBinary(uint8_t* buffer, uint32_t length, mt19937 &gen, uniform_int_distribution<uint32_t>& normalUInt32Distribution) {
	uint32_t mask = 0xFF;
	int iterationCounter = 0;
	uint32_t random = 0;
	
	auto generator = [&normalUInt32Distribution, &gen] {return normalUInt32Distribution(gen);  };

	for (int i = 0; i < length; i++) {
		if (iterationCounter == 0) {
			random = generator();
			iterationCounter = 4;
		}

		buffer[i] = ((random >> ((4 - iterationCounter) * 8)) & mask);

		iterationCounter--;
	}
}

//Network functions
void TransmitPacket(Packet& targetPacket, mt19937 gen) {
	targetPacket.isEmptyPacket = false;

	//Randomly generating the latency according to a normal distributiom
	normal_distribution<double> normalDistribution{meanLatencyMS, sdLatency};

	auto randomLatency = [&normalDistribution, &gen] {return lround(normalDistribution(gen));  };
	uint32_t latency = static_cast<uint32_t>(randomLatency());

	//Wait for the latency time
	this_thread::sleep_for(chrono::milliseconds(latency));

	//Packet loss
	uniform_real_distribution<double> uniformDistribution(0.0, 1.0);
	auto randomPacketLoss = [&uniformDistribution, &gen] {return uniformDistribution(gen);  };
	
	if (randomPacketLoss() <= (packetDropRatePercentage / 100)) targetPacket.isEmptyPacket = true;
}

void RunTransmission(mt19937& gen, uniform_int_distribution<uint32_t> d) {
	cout << "Run Start" << endl;

	//Setup
	uint8_t buffer[1448];

	GenerateRandomBinary(
		buffer,
		4,
		gen,
		d
	);

	uint32_t senderSequenceNumber = ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]) & 0x7FFFFFF; //We have to set the first bit as 0
	uint32_t receiverSequenceNumber = ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]) | 0x80000000; //We have to set the first bit as 1


	//Step 1 - Initial Handshake
	{

		//SYN1
		vector<uint8_t> syn1Payload = {
			static_cast<uint8_t>((windowSizeB >> 24) & 0xFF),
			static_cast<uint8_t>((windowSizeB >> 16) & 0xFF),
			static_cast<uint8_t>((windowSizeB >> 8) & 0xFF),
			static_cast<uint8_t>((windowSizeB) & 0xFF),
			static_cast<uint8_t>((RTTMS >> 8) & 0xFF),
			static_cast<uint8_t>((RTTMS) & 0xFF)
		};

		Packet SYN1 = Packet{
			0x00,
			0x00,
			senderSequenceNumber,
			0x00,
			0x01,
			0x00,
			syn1Payload,
			false
		};
		SYN1.CalculateChecksum();
		senderSequenceNumber++;

		vector<uint8_t> syn1AckPayload = {
			static_cast<uint8_t>((windowSizeB >> 24) & 0xFF),
			static_cast<uint8_t>((windowSizeB >> 16) & 0xFF),
			static_cast<uint8_t>((windowSizeB >> 8) & 0xFF),
			static_cast<uint8_t>((windowSizeB) & 0xFF),
		};

		//SYN1-ACK
		Packet SYN1Ack = Packet{
			0x00,
			0x00,
			receiverSequenceNumber,
			0x00,
			0x03,
			0x00,
			syn1AckPayload,
			false
		};
		SYN1Ack.CalculateChecksum();
		receiverSequenceNumber++;

		//1st data packet - 3rd packet of 3 way handshake
		int noOfPackets = 1 + ceil((fileSizeB - 1444) / 1448);
		uint32_t finalSequenceNumber = senderSequenceNumber + noOfPackets - 1;

		vector<uint8_t> firstDataPacketPayload = {
			static_cast<uint8_t>((finalSequenceNumber >> 24) & 0xFF),
			static_cast<uint8_t>((finalSequenceNumber >> 16) & 0xFF),
			static_cast<uint8_t>((finalSequenceNumber >> 8) & 0xFF),
			static_cast<uint8_t>((finalSequenceNumber) & 0xFF),
		};

		Packet firstDataPacket = Packet{
			0x00,
			0x00,
			senderSequenceNumber,
			0x00,
			0x20,
			0x00,
			firstDataPacketPayload,
			false
		};
		firstDataPacket.CalculateChecksum();
		senderSequenceNumber++;

		TransmitPacket(SYN1, gen);
		if (!SYN1.isEmptyPacket) TransmitPacket(SYN1Ack, gen);
		else {
			while (SYN1.isEmptyPacket) {
				//Wait for RTTMS, then resend packet
				this_thread::sleep_for(chrono::milliseconds(RTTMS));
				TransmitPacket(SYN1, gen);
			}
		}
		if (!SYN1Ack.isEmptyPacket) TransmitPacket(firstDataPacket, gen);

		else {
			while (SYN1Ack.isEmptyPacket) {
				//As above
				this_thread::sleep_for(chrono::milliseconds(RTTMS));
				TransmitPacket(SYN1Ack, gen);
			}
		}

		while (firstDataPacket.isEmptyPacket) {
			//As above
			this_thread::sleep_for(chrono::milliseconds(RTTMS));
			TransmitPacket(SYN1Ack, gen);
		}

	}

	cout << "Run End" << endl;
}

int main()
{
	//Loading in settings data
	fs::path filePath = "NetworkSettings.json";
	ifstream file(filePath);
	json settingsData;

	if (!file.is_open())
	{
		cout << "Failed to open JSON file\n";
		return 1;
	}

	file >> settingsData;

	meanLatencyMS = settingsData["Network Settings"]["Mean Latency ms"];
	sdLatency = settingsData["Network Settings"]["Latency Standard Deviation"];
	packetDropRatePercentage = settingsData["Network Settings"]["Packet Drop Rate %"];
	fileSizeB = settingsData["Network Settings"]["File Size B"];

	RTTMS = settingsData["Protocol Settings"]["RTT ms"];
	SSCTMS = settingsData["Protocol Settings"]["SSCT ms"];
	SRCTMS = settingsData["Protocol Settings"]["SRCT ms"];
	windowSizeB = settingsData["Protocol Settings"]["Window Size B"];

	dbPath = settingsData["Simulation Settings"]["Database Path"];
	runtimeS = settingsData["Simulation Settings"]["Runtime s"];

	//Randomness
	random_device rd{};
	mt19937 gen{ rd() };

	//Simulation timer
	time_t now = time(NULL);
	struct tm datetime = *localtime(&now);
	datetime.tm_sec = datetime.tm_sec + runtimeS;
	time_t end = mktime(&datetime);

	while (difftime(end, now) > 0) {
		now = time(NULL);
		struct tm datetime = *localtime(&now);

		//Network logic
		RunTransmission(
			gen,
			uniformUInt32Distribution
		);
	}

	return 0;
}