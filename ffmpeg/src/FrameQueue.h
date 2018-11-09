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
		void frame_queue_signal(FrameQueue *f);//֡�����ź�
		Frame *frame_queue_peek(FrameQueue *f);//����/��λ֡
		Frame *frame_queue_peek_next(FrameQueue *f);//����/��λ��һ֡
		Frame *frame_queue_peek_last(FrameQueue *f);//�������һ֡
		Frame *frame_queue_peek_writable(FrameQueue *f);//���ҿ�д֡
		Frame *frame_queue_peek_readable(FrameQueue *f);//���ҿɶ�֡
		void frame_queue_push(FrameQueue *f);//֡���
		void frame_queue_next(FrameQueue *f);//��һ��
		int frame_queue_nb_remaining(FrameQueue *f);//����ʣ��֡
		int64_t frame_queue_last_pos(FrameQueue *f);//֡�������λ��
	private:
		void frame_queue_unref_item(Frame *vp);//����frame
	
	};
}

#endif
