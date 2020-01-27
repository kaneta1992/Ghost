#pragma once
#pragma comment(lib, "winmm")

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT

#include <string>
#include <cmath>
#include <map>
#include <exception>
#include "minimp3_ex.h"
#include "AudioFile.h"

#define A_PI 3.14159265358979323846


class PCMAudio {
public:
	PCMAudio() {}
	float* GetBuffer() const { return this->buffer; }
	int GetChannels() const { return this->channels; }
	int GetBitDepth() const { return this->bitDepth; }
	int GetSampleRate() const { return this->sampleRate; }
	int GetSamples() const { return this->samples; }
	int GetIntSampleAt(int index) const {
		return (int)(buffer[index] * (float)((1 << bitDepth) / 2 - 1));
	}
	bool IsValid() {
		return this->buffer != nullptr;
	}
protected:
	void Initialize(float* buffer, int channels, int bitDepth, int sampleRate, int samples) {
		this->buffer = buffer;
		this->channels = channels;
		this->bitDepth = bitDepth;
		this->sampleRate = sampleRate;
		this->samples = samples;
	}
	float* buffer = nullptr;
	int channels;
	int bitDepth;
	int sampleRate;
	int samples;
};

class MP3Audio : public PCMAudio {
public:
	MP3Audio() {}
	~MP3Audio() {
		free(buffer);
	}
	void LoadFromFile(std::string filename) {
		mp3dec_t mp3d;
		mp3dec_file_info_t info;
		if (mp3dec_load(&mp3d, filename.c_str(), &info, NULL, NULL))
		{
			throw std::runtime_error("mp3 failed to load");
		}
		Initialize(info.buffer, info.channels, 16, info.hz, info.samples);
	}
private:
};

class SinAudio : public PCMAudio {
public:
	SinAudio() {}
	~SinAudio() {
		delete buffer;
	}
	void Create(float hz) {
		int samples = 44100 * 10;
		float *buf = new float[samples];
		for (int i = 0; i < samples; i++) {
			buf[i] = sinf(hz * (i / 44100.0) * A_PI * 2.0);
		}
		Initialize(buf, 1, 16, 44100, samples);
	}
private:
};

class NokogiriAudio : public PCMAudio {
public:
	NokogiriAudio() {}
	~NokogiriAudio() {
		delete buffer;
	}
	void Create(float hz) {
		double rate = 44100.0;
		int samples = (int)(rate) * 1;
		float* buf = new float[samples];
		for (int i = 0; i < samples; i++) {
			double t = i / rate;
			buf[i] = fmod(t * hz, 1.0) * 2.0 - 1.0;
			//buf[i] = sinf(440.0 * t * A_PI * 2.0);
			//buf[i] = sin(2 * A_PI * 400 * t);
			//buf[i] = sin(2 * A_PI * 400 * t) + 2 * sin(2 * A_PI * 800 * t) + 2 * sin(2 * A_PI * 1280 * t);
		}
		Initialize(buf, 1, 16, (int)rate, samples);
	}
private:
};

class PCMAudioPlayer {
public:
	PCMAudioPlayer() {}
	~PCMAudioPlayer() {
		Close();
	}

	void SetAudio(PCMAudio const& audio) {
		Close();
		samples = audio.GetSamples();
		wfe.wFormatTag = WAVE_FORMAT_PCM;
		wfe.nChannels = audio.GetChannels();								// Channels
		wfe.wBitsPerSample = audio.GetBitDepth();									// Bit Depth
		wfe.nBlockAlign = wfe.nChannels * wfe.wBitsPerSample / 8;	// Byte per Minimum Unit
		wfe.nSamplesPerSec = audio.GetSampleRate();								// Sample Rate
		wfe.nAvgBytesPerSec = wfe.nSamplesPerSec * wfe.nBlockAlign;	// Byte per One Second

		waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfe, (DWORD)waveOutProc, 0, CALLBACK_FUNCTION);
		playerMap[hWaveOut] = this;

		switch (wfe.wBitsPerSample)
		{
		case 8:
			wave = new BYTE[audio.GetSamples()];
			break;
		case 16:
			wave = new short[audio.GetSamples()];
			break;
		default:
			throw std::runtime_error("Not support this bit depth");
			break;
		}

		for (unsigned int i = 0; i < audio.GetSamples(); i++) {

			switch (wfe.wBitsPerSample)
			{
			case 8:
				((BYTE*)wave)[i] = (BYTE)(audio.GetIntSampleAt(i));
				break;
			case 16:
				((short*)wave)[i] = (short)(audio.GetIntSampleAt(i));
				break;

			default:
				throw std::runtime_error("Not support this bit depth");
				break;
			}
		}

		whdr.lpData = (LPSTR)wave;
		whdr.dwBufferLength = audio.GetSamples() * (audio.GetBitDepth()/8);
		whdr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
		whdr.dwLoops = 1;

		waveOutPrepareHeader(hWaveOut, &whdr, sizeof(WAVEHDR));
	}

	void Start() {
		waveOutWrite(hWaveOut, &whdr, sizeof(WAVEHDR));
	}
	void Pause() {
		waveOutPause(hWaveOut);
	}
	void Restart() {
		waveOutRestart(hWaveOut);
	}
	void Stop() {
		isStopping = true;
		waveOutReset(hWaveOut);
	}
	void Close() {
		Stop();
		waveOutUnprepareHeader(hWaveOut, &whdr, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		playerMap[hWaveOut] = nullptr;
		isStopping = false;
		delete wave;
	}
	int GetPosition() {
		MMTIME mmt;
		int ms;
		mmt.wType = TIME_SAMPLES;
		waveOutGetPosition(hWaveOut, &mmt, sizeof(MMTIME));
		ms = mmt.u.ms;
		return ms % std::max(samples / std::max((int)wfe.nChannels, 1), 1);
	}
	void SetLoop(bool loop) {
		isLoop = loop;
	}
private:
	WAVEFORMATEX wfe;
	HWAVEOUT hWaveOut;
	WAVEHDR whdr;
	void* wave;
	int samples;
	bool isLoop;
	bool isStopping;

	static std::map<HWAVEOUT, PCMAudioPlayer*> playerMap;

	static 	void CALLBACK waveOutProc(
		HWAVEOUT hwo,
		UINT uMsg,
		DWORD dwInstance,
		DWORD dwParam1,
		DWORD dwParam2
	)
	{
		switch (uMsg)
		{
		case WOM_OPEN:
			printf("waveOutOpen\n");
			break;
		case WOM_CLOSE:
			printf("waveOutClose\n");
			break;
		case WOM_DONE:
			printf("End\n");
			
			// FIXME:
			// どうやらコールバック中にAPIを利用するとフリーズすることがあるらしい（実際にする）
			// ループ機能を消すか、キュー方式に変更する必要がある
			auto player = playerMap[hwo];
			if (player->isStopping) {
				player->isStopping = false;
			}
			else {
				if (player->isLoop) {
					player->Start();
				}
			}
			break;
		}
	}
};
std::map<HWAVEOUT, PCMAudioPlayer*> PCMAudioPlayer::playerMap;

void SaveAudioToWaveFile(PCMAudio const& audio, std::string filename) {
	AudioFile<double> audioFile;
	AudioFile<double>::AudioBuffer buffer;

	int numChannels = audio.GetChannels();
	int numSamplesPerChannel = audio.GetSamples() / numChannels;
	float sampleRate = (float)audio.GetSampleRate();

	buffer.resize(numChannels);

	buffer[0].resize(numSamplesPerChannel);

	if (numChannels == 2) {
		buffer[1].resize(numSamplesPerChannel);
	}

	for (int i = 0; i < numSamplesPerChannel; i++)
	{
		for (int channel = 0; channel < numChannels; channel++)
		{
			buffer[channel][i] = audio.GetBuffer()[i * numChannels + channel];
		}
	}

	audioFile.setAudioBuffer(buffer);

	audioFile.setBitDepth(audio.GetBitDepth());
	audioFile.setSampleRate(audio.GetSampleRate());
	audioFile.save(filename.c_str());
}