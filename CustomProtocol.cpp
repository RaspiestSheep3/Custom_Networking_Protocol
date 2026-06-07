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

float RTTMS;
float SSCTMS;
float SRCTMS;
float windowSizeB;

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
	uint8_t payload[1448];

	bool isEmptyPacket; //This is used for packet loss

	uint16_t CalculateChecksum(Packet targetPacket);
};

//Packet functions
uint16_t Packet::CalculateChecksum(Packet targetPacket) {
	//Done using 16-bit CRC
	//Divisor = 0x8005 

	//Data preprocessing
	//724 payload entries + 5 header entries not including checksum
	uint16_t data[729] = { 0 };
	int length = sizeof(data) / sizeof(data[0]);

	data[0] = targetPacket.sourcePort;
	data[1] = targetPacket.destPort;
	data[2] = static_cast<uint16_t>(targetPacket.sequenceNumber >> 8);
	data[3] = static_cast<uint16_t>(targetPacket.sequenceNumber);
	data[4] = (static_cast<uint16_t>(flags) << 8) + static_cast<uint16_t>(targetPacket.version);

	for (int i = 5; i < length; i++) data[i] = (static_cast<uint16_t>(targetPacket.payload[2 * (i - 5)]) << 8) + static_cast<uint16_t>(targetPacket.payload[2 * (i - 5) + 1]);

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

//Network functions
void TransmitPacket(Packet& targetPacket) {
	//Randomly generating the latency according to a normal distribution
	random_device rd{};
	mt19937 gen{ rd() };

	std::normal_distribution<double> normalDistribution{meanLatencyMS, sdLatency};

	auto randomLatency = [&normalDistribution, &gen] {return lround(normalDistribution(gen));  };
	uint32_t latency = static_cast<uint32_t>(randomLatency());

	//Wait for the latency time
	this_thread::sleep_for(std::chrono::milliseconds(latency));

	//Packet loss
	uniform_real_distribution<double> uniformDistribution(0.0, 1.0);
	auto randomPacketLoss = [&uniformDistribution, &gen] {return uniformDistribution(gen);  };
	
	if (randomPacketLoss() <= (packetDropRatePercentage / 100)) targetPacket.isEmptyPacket = true;
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

	RTTMS = settingsData["Protocol Settings"]["RTT ms"];
	SSCTMS = settingsData["Protocol Settings"]["SSCT ms"];
	SRCTMS = settingsData["Protocol Settings"]["SRCT ms"];
	windowSizeB = settingsData["Protocol Settings"]["Window Size B"];

	dbPath = settingsData["Simulation Settings"]["Database Path"];
	runtimeS = settingsData["Simulation Settings"]["Runtime s"];

	//Simulation timer
	time_t now = time(NULL);
	struct tm datetime = *localtime(&now);
	datetime.tm_sec = datetime.tm_sec + runtimeS;
	time_t end = mktime(&datetime);

	while (difftime(end, now) > 0) {
		now = time(NULL);
		struct tm datetime = *localtime(&now);

		//Network logic
	}

	return 0;
}