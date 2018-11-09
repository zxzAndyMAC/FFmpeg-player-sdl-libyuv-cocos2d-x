#ifndef FFMPEG_FQ_H
#define FFMPEG_FQ_H

#include "structs.h"

namespace FFMPEG {
	class CFrameQueue
	{
	public:
		CFrameQueue();
		~CFrameQueue();

		int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
		void frame_queue_destory(FrameQueue *f);
		void frame_queue_signal(FrameQueue *f);//帧队列信号
		Frame *frame_queue_peek(FrameQueue *f);//查找/定位帧
		Frame *frame_queue_peek_next(FrameQueue *f);//查找/定位下一帧
		Frame *frame_queue_peek_last(FrameQueue *f);//查找最后一帧
		Frame *frame_queue_peek_writable(FrameQueue *f);//查找可写帧
		Frame *frame_queue_peek_readable(FrameQueue *f);//查找可读帧
		void frame_queue_push(FrameQueue *f);//帧入队
		void frame_queue_next(FrameQueue *f);//下一个
		int frame_queue_nb_remaining(FrameQueue *f);//队列剩余帧
		int64_t frame_queue_last_pos(FrameQueue *f);//帧队列最后位置
	private:
		void frame_queue_unref_item(Frame *vp);//销毁frame
	
	};
}

#endif
