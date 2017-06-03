#include <dicto-types.h>

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <windows.h>
#include <mmsystem.h>

using namespace std;

namespace dicto
{
	typedef function<void(HWAVEIN handle, WAVEHDR &data)> handler_function;

	shared_ptr<void> open(const handler_function &handler, int samplerate)
	{
		struct inner
		{
			static void CALLBACK waveInProc(HWAVEIN handle, UINT message, DWORD_PTR instance,
				DWORD_PTR param1, DWORD_PTR /*param2*/)
			{
				if (WIM_DATA == message)
				{
					auto handler = reinterpret_cast<const handler_function *>(instance);
					(*handler)(handle, *reinterpret_cast<WAVEHDR *>(param1));
				}
			}
		};

		unique_ptr<handler_function> handler_(new handler_function(handler));
		auto handler_p = handler_.get();
		HWAVEIN h = NULL;
		WAVEFORMATEX wf = { 0 };

		wf.cbSize = sizeof(WAVEFORMATEX);
		wf.wFormatTag = WAVE_FORMAT_PCM;
		wf.wBitsPerSample = 16;
		wf.nChannels = 1;
		wf.nSamplesPerSec = samplerate;
		wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nChannels * wf.wBitsPerSample / 8;
		wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;

		auto result = waveInOpen(&h, WAVE_MAPPER, &wf, reinterpret_cast<DWORD_PTR>(&inner::waveInProc),
			reinterpret_cast<DWORD_PTR>(handler_p), CALLBACK_FUNCTION | WAVE_FORMAT_DIRECT);

		if (MMSYSERR_NOERROR != result)
			throw runtime_error("Unable to open device!");
		handler_.release();
		return shared_ptr<void>(h, [handler_p] (HWAVEIN handle) {
			waveInClose(handle);
			delete handler_p;
		});
	}

	template <size_t sample_size = 10000>
	struct buffer : WAVEHDR
	{
		enum { n_samples = sample_size } ;

		buffer()
		{
			dwBufferLength = sizeof(data);
			lpData = reinterpret_cast<LPSTR>(data);
		}

		void add(HWAVEIN handle)
		{
			dwFlags = 0;
			waveInPrepareHeader(handle, this, sizeof(WAVEHDR));
			waveInAddBuffer(handle, this, sizeof(WAVEHDR));
		}

		void release(HWAVEIN handle)
		{	waveInUnprepareHeader(handle, this, sizeof(WAVEHDR));	}

		int16_t data[sample_size];
	};
}

int main()
{
	int sample_rate = 44100;

	auto device = dicto::open([&] (HWAVEIN handle, WAVEHDR &data) {
		auto &b = static_cast<dicto::buffer<> &>(data);

		b.release(handle);

		dicto::audio_header output_header = { 0 };
		
		output_header.magic = dicto::audio_header::magic_value;
		output_header.sample_rate = sample_rate;
		output_header.samples = b.n_samples;
		output_header.channels = 1;
		fwrite(&output_header, sizeof(output_header), 1, stdout);
		fwrite(b.data, 2, b.n_samples, stdout);
		b.add(handle);
	}, sample_rate);
	dicto::buffer<> buffers[2];

	buffers[0].add(static_cast<HWAVEIN>(device.get()));
	buffers[1].add(static_cast<HWAVEIN>(device.get()));
	waveInStart(static_cast<HWAVEIN>(device.get()));

	string input;

	while (getline(cin, input) && input != ":q")
	{	}
}
