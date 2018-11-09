
#if defined(_FF_DEBUG_)
#include "vld.h"
#endif
#include "MediaState.h"
#include <unordered_map>
#include <queue>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace FFMPEG {
	
	struct PlayStruct
	{
		PlayStruct(MediaState* ms, const char *filename) {
			_ms = ms;
			_filename = filename;
			_buffer = nullptr;
			_size = 0;
		}

		PlayStruct(MediaState* ms, char *buffer, unsigned int size) {
			_ms = ms;
			_size = size;
			_buffer = buffer;
			_filename = nullptr;
		}

		MediaState  *_ms;
		const char  *_filename;
		char	    *_buffer;
		unsigned int _size;
	};

	class FFManager : public IFFMpeg
	{
	public:
		FFManager():_id(0), _firstPlay(true), _time_offset(500)
		{
		}

		~FFManager()
		{
			while(!_queue.empty())
			{
				PlayStruct* ps = _queue.front();
				_queue.pop();
				delete ps;
			}

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


		virtual bool play(FFHandle id, const char *filename) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			if (_firstPlay)
			{
				ms->play(filename);
				_firstPlay = false;
				setTime();
			}
			else
				_queue.push(new PlayStruct(ms, filename));
			return true;
		}

		virtual bool play(FFHandle id, char* buffer, unsigned int size) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return false;
			}
			if (_firstPlay)
			{
				ms->play(buffer, size);
				_firstPlay = false;
				setTime();
			}
			else
				_queue.push(new PlayStruct(ms, buffer, size));
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


		virtual FFMPEG_EVENT event_loop(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return FF_ERROR;
			}
			FFMPEG_EVENT ee;
			if (ms->getEvent() == FF_HOLD)
			{
				if (checkTimePassed())
				{
					PlayStruct* ps = _queue.front();
					if (ps->_filename != nullptr)
					{
						ps->_ms->play(ps->_filename);
					}
					else
						ps->_ms->play(ps->_buffer, ps->_size);
					_queue.pop();
					delete ps;
				}
				return ms->getEvent();
			}
			else
				ee = ms->event_loop();
// 			if (ee == FF_OVER)//²¥·Å½áÊø
// 			{
// 				_mediaPlayers.erase(id);
// 				delete ms;
// 			}
			return ee;
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

		virtual uint8_t *getData(FFHandle id) override
		{
			MediaState* ms = findMediaPlayer(id);
			if (ms == nullptr)
			{
				return nullptr;
			}
			uint8_t * data = ms->getData();
			return data;
		}

		FFHandle createMediaPlayer()
		{
			MediaState* ms = new MediaState(_id);
			_mediaPlayers.insert(std::pair<FFHandle, MediaState*>(_id, ms));
			return _id++;
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

#ifdef _WIN32
		int gettimeofday(struct timeval * val, struct timezone *)
		{
			if (val)
			{
				LARGE_INTEGER liTime, liFreq;
				QueryPerformanceFrequency(&liFreq);
				QueryPerformanceCounter(&liTime);
				val->tv_sec = (long)(liTime.QuadPart / liFreq.QuadPart);
				val->tv_usec = (long)(liTime.QuadPart * 1000000.0 / liFreq.QuadPart - val->tv_sec * 1000000.0);
			}
			return 0;
		}
#endif

		void setTime()
		{
			struct timeval current;
			gettimeofday(&current, NULL);
			_cur_time = (long long)current.tv_sec * 1000 + current.tv_usec / 1000;
		}

		bool checkTimePassed()
		{
			struct timeval current;
			gettimeofday(&current, NULL);
			long long _now = (long long)current.tv_sec * 1000 + current.tv_usec / 1000;
			long long _time_used = _now - _cur_time;
			if (_time_used >= _time_offset)
			{
				_cur_time = _now;
				return true;
			}			 
			return false;
		}

private:
		const long long                             _time_offset;
		long long									_cur_time;
		std::unordered_map<FFHandle, MediaState*>	_mediaPlayers;
		FFHandle									_id;
		std::queue<PlayStruct*>						_queue;
		bool										_firstPlay;
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
		MediaState::release();
	}

	FFHandle SimpleFFmpeg::newMediaPlayer()
	{
		if (s_ms == nullptr)
			SimpleFFmpeg::createInstance();
		return s_ms->createMediaPlayer();
	}
}