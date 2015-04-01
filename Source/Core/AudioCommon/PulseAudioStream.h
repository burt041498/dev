// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
#include <pulse/pulseaudio.h>
#endif

#include <atomic>

#include "AudioCommon/SoundStream.h"
#include "Common/CommonTypes.h"
#include "Common/Thread.h"

class PulseAudio final : public SoundStream
{
#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
public:
	PulseAudio(CMixer *mixer);

	virtual bool Start() override;
	virtual void Stop() override;

	static bool isValid() { return true; }

	virtual void Update() override;

	void StateCallback(pa_context *c);
	void WriteCallback(pa_stream *s, size_t length);
	void UnderflowCallback(pa_stream *s);

private:
	virtual void SoundLoop() override;

	bool PulseInit();
	void PulseShutdown();

	// wrapper callback functions, last parameter _must_ be PulseAudio*
	static void StateCallback(pa_context *c, void *userdata);
	static void WriteCallback(pa_stream *s, size_t length, void *userdata);
	static void UnderflowCallback(pa_stream *s, void *userdata);

	std::thread m_thread;
	std::atomic<bool> m_run_thread;

	bool m_stereo; // stereo, else surround
	int m_bytespersample;
	int m_channels;

	int m_pa_error;
	int m_pa_connected;
	pa_mainloop *m_pa_ml;
	pa_mainloop_api *m_pa_mlapi;
	pa_context *m_pa_ctx;
	pa_stream *m_pa_s;
	pa_buffer_attr m_pa_ba;
#else
public:
	PulseAudio(CMixer *mixer) : SoundStream(mixer) {}
#endif
};
