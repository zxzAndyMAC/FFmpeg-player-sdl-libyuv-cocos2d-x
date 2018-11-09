
#ifndef _SDLWAPPER_H_
#define _SDLWAPPER_H_

#include "SDL_mixer.h"
#include "SDLThreadPool.h"
#include <stdlib.h>
#include <string>
#include <functional>
#include <unordered_map> 

#include "ISDLAudio.h"

namespace SDL
{
	class SDLAudio : public ISDL
	{
	public:
		SDLAudio(int flags);
		~SDLAudio();

		virtual void releaseThreadPool() override;

		/*****************************sound相关********************************/

		virtual void allocateChannels(int nums) override;

		virtual bool loadSound(const std::string &filePath) override;
		virtual bool loadSound(const std::string &filename, void *data, unsigned int size = 0) override;

		virtual void loadSoundAsynchronous(const std::string& filePath, std::function<void(bool)> callback) override;
		virtual void loadSoundAsynchronous(const std::string &filename, void *data, unsigned int size, std::function<void(bool)> callback) override;

		virtual void playSound(const std::string &filename, void *data = nullptr, int channel = -1, bool loop = false) override;

		virtual AUDIOSTATE getSoundState(int channel = -1) override;

		virtual void pauseSound(int channel = -1) override;
		virtual void resumeSound(int channel = -1) override;
		virtual void stopSound(int channel = -1) override;

		virtual void unloadSound(const std::string &filename) override;
		virtual void unloadAllSound() override;

		virtual void setSoundVolume(int volume, int channel = -1) override;//0~128
		virtual void setSoundVolume(int volume, const std::string &filename) override;

		virtual int getSoundVolume() override
		{
			return _soundVolume;
		}

		virtual void channelFinishedCallback(channleCallback cb) override;

		/********************************************************************/

		/*****************************music相关********************************/
		virtual bool loadMusic(const std::string &filePath) override;
		virtual bool loadMusic(const void *data, unsigned int size) override;

		virtual void loadMusicAsynchronous(const std::string& filePath, std::function<void(bool)> callback) override;
		virtual void loadMusicAsynchronous(const void *data, unsigned int size, std::function<void(bool)> callback) override;

		virtual void playMusic(bool loop = true) override;
		virtual void playMusic(int times) override;

		virtual void setMusicVolume(int volume) override;

		virtual AUDIOSTATE getMusicState() override;

		virtual void rewineMusic() override;
		virtual void pauseMusic() override;
		virtual void resumeMusic() override;
		virtual void stopMusic() override;

		virtual void unloadMusic() override;

		virtual int getMusicVolume() override
		{
			return _musicVolume;
		}
	private:
		bool init();
		void uninit();
		void initthread();

		std::unordered_map<std::string, Mix_Chunk*> _SoundMap;

		Mix_Music* _Music;
		bool       _inited;
		int        _channelNums;
		int		   _flags;
		int		   _soundVolume, _musicVolume;
		SDLThreadPool* _thread_pool;
	};
}
#endif

