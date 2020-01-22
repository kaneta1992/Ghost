#pragma once
#pragma comment(lib, "winmm")

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT

#include <string>
#include <exception>
#include "minimp3_ex.h"
#include "AudioFile.h"

class PCMAudio {
public:
	PCMAudio() {}
	float* GetBuffer() const { return this->buffer; }
	int GetChannels() const { return this->channels; }
	int GetBitDepth() const { return this->bitDepth; }
	int GetSampleRate() const { return this->sampleRate; }
	int GetSamples() const { return this->samples; }
	DWORD GetIntSampleAt(int index) const {
		return (DWORD)(buffer[index] * (float)((1 << bitDepth) / 2 - 1));
	}
protected:
	void Initialize(float* buffer, int channels, int bitDepth, int sampleRate, int samples) {
		this->buffer = buffer;
		this->channels = channels;
		this->bitDepth = bitDepth;
		this->sampleRate = sampleRate;
		this->samples = samples;
	}
	float* buffer;
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

class PCMAudioPlayer {
public:
	PCMAudioPlayer() {}
	~PCMAudioPlayer() {
		Close();
	}
	void SetAudio(PCMAudio const& audio) {
		Close();
		wfe.wFormatTag = WAVE_FORMAT_PCM;
		wfe.nChannels = audio.GetChannels();								// Channels
		wfe.wBitsPerSample = audio.GetBitDepth();									// Bit Depth
		wfe.nBlockAlign = wfe.nChannels * wfe.wBitsPerSample / 8;	// Byte per Minimum Unit
		wfe.nSamplesPerSec = audio.GetSampleRate();								// Sample Rate
		wfe.nAvgBytesPerSec = wfe.nSamplesPerSec * wfe.nBlockAlign;	// Byte per One Second

		waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfe, 0, 0, CALLBACK_NULL);

		switch (wfe.wBitsPerSample)
		{
		case 8:
			wave = new BYTE[audio.GetSamples()];
			break;
		case 16:
			wave = new WORD[audio.GetSamples()];
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
				((WORD*)wave)[i] = (WORD)(audio.GetIntSampleAt(i));
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
	void Stop() {
		waveOutReset(hWaveOut);
	}
	void Close() {
		waveOutReset(hWaveOut);
		waveOutUnprepareHeader(hWaveOut, &whdr, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		delete wave;
	}
private:
	WAVEFORMATEX wfe;
	HWAVEOUT hWaveOut;
	WAVEHDR whdr;
	void* wave;
};

void SaveAudioToWaveFile(PCMAudio const& audio, std::string filename) {
	AudioFile<double> audioFile;
	AudioFile<double>::AudioBuffer buffer;

	int numChannels = audio.GetChannels();
	int numSamplesPerChannel = audio.GetSamples() / numChannels;
	float sampleRate = (float)audio.GetSampleRate();

	buffer.resize(numChannels);

	buffer[0].resize(numSamplesPerChannel);
	buffer[1].resize(numSamplesPerChannel);


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