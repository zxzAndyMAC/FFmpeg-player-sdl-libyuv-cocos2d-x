
#if defined(_FF_DEBUG_)
#include "vld.h"
#endif
#include "MediaState.h"
#include <unordered_map>
#include <queue>
#include <time.h>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace FFMPEG {
	class FFManager : public IFFMpeg
	{
	public:
		FFManager():_id(0)
		{
		}

		~FFManager()
		{
			std::unordered_map<FFHandle, MediaState*>::iterator iter = _mediaPlayers.begin();
			for (iter;iter!=_mediaPlayers.end();iter++)
			{
				delete iter->second;
			}
			_mediaPlayers.clear();
		}

		virtual bool pause(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->pause();
			return true;
		} 

		virtual bool setFramedrop(FFHandle id, int fd) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->setFramedrop(fd);
			return true;
		}

		virtual bool resume(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->resume();
			return true;
		}

		virtual bool mute(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->mute();
			return true;
		}

		virtual bool unmute(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->unmute();
			return true;
		}


		virtual FFHandle create( const char *filename) override
		{
			FFHandle id = createMediaPlayer();

			MediaState* ms = findMediaPlayer(id);

			if (ms == nullptr)
			{
				return -1;
			}
			ms->create(filename);

			return id;
		}

		virtual FFHandle create( char* buffer, unsigned int size) override
		{
			FFHandle id = createMediaPlayer();

			MediaState* ms = findMediaPlayer(id);

			if (ms == nullptr)
			{
				return -1;
			}

			ms->create(buffer, size);

			return id;
		}

		virtual bool play( FFHandle id, int loopCnt = 1 )
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->play( loopCnt );

			return true;
		}

		virtual unsigned int getWidth(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return -1;
			}
			unsigned int t = ms->getWidth();
			return t;
		}

		virtual unsigned int getHeight(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return -1;
			}
			unsigned int t = ms->getHeight();
			return t;
		}


		virtual int getDuration(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return -1;
			}
			int t = ms->getDuration();
			return t;
		}

		virtual bool setSeek(FFHandle id, double seconds) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			ms->setSeek(seconds);
			return false;
		}

		virtual double get_master_clock(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return -1;
			}
			double t = ms->get_master_clock();
			return t;
		}


		virtual void event_loop() override
		{
			for ( auto it = _mediaPlayers.begin(); it != _mediaPlayers.end(); ++it )
			{
				it->second->event_loop();
			}
		}

		virtual FFMPEG_EVENT getEvent(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return FF_ERROR;
			}

			return ms->getEvent();
		}

		virtual bool release(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			_mediaPlayers.erase(id);
			delete ms;
			return true;
		}


		virtual uint8_t *getData( FFHandle id, unsigned int& width, unsigned int& height ) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return nullptr;
			}

			if ( ms->getEvent() == FF_PLAY )
			{
				uint8_t * data = ms->getData();

				width  = ms->getWidth();
				
				height = ms->getHeight();

				return data;
			}
			
			return nullptr;
		}

		virtual uint8_t* getThumbnail( FFHandle id, unsigned int& width, unsigned int& height ) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return nullptr;
			}

			uint8_t* data = ms->getThumbnail();
			
			width  = ms->getWidth();
			
			height = ms->getHeight();

			return data;
		}

		FFHandle createMediaPlayer()
		{
			MediaState* ms = new MediaState(++_id);
			_mediaPlayers.insert(std::pair<FFHandle, MediaState*>(_id, ms));
			return _id;
		}
	private:

		MediaState* findMediaPlayer(FFHandle id)
		{
			std::unordered_map<FFHandle, MediaState*>::iterator got = _mediaPlayers.find(id);
			if (got == _mediaPlayers.end())
			{
				return nullptr;
			}
			else
			{
				return got->second;
			}
		}

private:
		std::unordered_map<FFHandle, MediaState*>	_mediaPlayers;
		FFHandle									_id;
	};

	///////////////////////////////////////////////////////////////////////////////////
	static FFManager* s_ms = nullptr;

	IFFMpeg* SimpleFFmpeg::createInstance()
	{
		if (s_ms == nullptr)
		{
			s_ms = new FFManager();
		}
		return s_ms;
	}

	IFFMpeg* SimpleFFmpeg::getInstance()
	{
		return s_ms;
	}

	void SimpleFFmpeg::release()
	{
		if (s_ms != nullptr)
		{
			delete s_ms;
			s_ms = nullptr;
		}

		avformat_network_deinit();

		SDL_Quit();
	}
}