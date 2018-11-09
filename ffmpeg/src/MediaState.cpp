
#if defined(_FF_DEBUG_)
#include "vld.h"
#endif
#include "MediaState.h"
#include <libyuv.h>

namespace FFMPEG {

	int MediaState::read_buffer(void *opaque, uint8_t *buf, int buf_size)
	{
		MediaState *ms = (MediaState*)opaque;
		buf_size = FFMIN(buf_size, ms->is->filebuffersize);
		if (!buf_size)
			return AVERROR_EOF;

		memcpy(buf, ms->is->filebuffer, buf_size);
		ms->is->filebuffer += buf_size;
		ms->is->filebuffersize -= buf_size;

		return buf_size;
	}

	MediaState::MediaState(unsigned int id) :
		decoder_reorder_pts(-1),
		_packetQueue(nullptr),
		_frameQueue(nullptr),
		framedrop(-1),
		fast(0),
		lowres(0),
		codec_opts(nullptr),
		format_opts(nullptr),
		find_stream_info(1),
		seek_by_bytes(-1),
		show_status(false),
		start_time(AV_NOPTS_VALUE),
		audio_disable(fast),
		video_disable(false),
		subtitle_disable(true),
		infinite_buffer(-1),
		duration(AV_NOPTS_VALUE),
		loop(1),
		rdftspeed(0.02),
		startup_volume(100),
		remaining_time(0.0),
		av_sync_type(AV_SYNC_AUDIO_MASTER),
		show_mode(VideoState::ShowMode::SHOW_MODE_NONE),
		displayFrame(nullptr),
		_bgra(nullptr),
		_rgba(nullptr),
		audio_callback_time(0),
		is(nullptr),
		_event(FF_HOLD)
	{
		_uniqueID = id;
		vs_mutex = SDL_CreateMutex();
		_packetQueue = new CPacketQueue(this);
		_frameQueue = new CFrameQueue();
	}

	MediaState::~MediaState()
	{
		do_exit();
		SDL_DestroyMutex(vs_mutex);

		if (_packetQueue != nullptr)
		{
			delete _packetQueue;
		}

		if (_frameQueue != nullptr)
		{
			delete _frameQueue;
		}
	}

	void MediaState::release()
	{
		avformat_network_deinit();
		SDL_Quit();
	}

	void MediaState::play(const char *filename)
	{
		if (!init())
		{
			av_log(NULL, AV_LOG_FATAL, "Failed to initialize VideoState!\n");
			return;
		}

		if (!stream_open(filename, nullptr)) {
			av_log(NULL, AV_LOG_FATAL, "Failed to open stream!\n");
		}

		_event = FF_NONE;
	}

	void MediaState::play(char* buffer, const unsigned int size)
	{
		if (!init())
		{
			av_log(NULL, AV_LOG_FATAL, "Failed to initialize VideoState!\n");
			return;
		}

		if (!stream_open(buffer, size)) {
			av_log(NULL, AV_LOG_FATAL, "Failed to open stream!\n");
		}

		_event = FF_NONE;
	}

	bool MediaState::init()
	{
#if TARGET_OS_IOS
		SDL_SetMainReady();
#endif

		int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
		if (!SDL_WasInit(flags))
		{
			av_log_set_flags(AV_LOG_SKIP_REPEATED);

			avcodec_register_all();
			av_register_all();
			avformat_network_init();

			if (SDL_Init(flags)) {
				av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
				av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
				return false;
			}

			av_init_packet(&flush_pkt);
			flush_pkt.data = (uint8_t *)&flush_pkt;
		}
		return true;
	}

	void MediaState::decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond)
	{
		memset(d, 0, sizeof(Decoder));
		d->avctx = avctx;
		d->queue = queue;
		d->empty_queue_cond = empty_queue_cond;
		d->start_pts = AV_NOPTS_VALUE;
		d->pkt_serial = -1;
	}

	int MediaState::decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub)
	{
		int ret = AVERROR(EAGAIN);

		for (;;) {
			AVPacket pkt;

			if (d->queue->serial == d->pkt_serial) {
				do {
					if (d->queue->abort_request)
						return -1;

					switch (d->avctx->codec_type) {
					case AVMEDIA_TYPE_VIDEO:
						ret = avcodec_receive_frame(d->avctx, frame);
						if (ret >= 0) {
							if (decoder_reorder_pts == -1) {
								frame->pts = frame->best_effort_timestamp;
							}
							else if (!decoder_reorder_pts) {
								frame->pts = frame->pkt_dts;
							}
						}
						break;
					case AVMEDIA_TYPE_AUDIO:
						ret = avcodec_receive_frame(d->avctx, frame);
						if (ret >= 0) {
							AVRational tb { 1, frame->sample_rate };
							if (frame->pts != AV_NOPTS_VALUE)
								frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
							else if (d->next_pts != AV_NOPTS_VALUE)
								frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
							if (frame->pts != AV_NOPTS_VALUE) {
								d->next_pts = frame->pts + frame->nb_samples;
								d->next_pts_tb = tb;
							}
						}
						break;
					default:
						break;
					}
					if (ret == AVERROR_EOF) {
						d->finished = d->pkt_serial;
						avcodec_flush_buffers(d->avctx);
						return 0;
					}
					if (ret >= 0)
						return 1;
				} while (ret != AVERROR(EAGAIN));
			}

			do {
				if (d->queue->nb_packets == 0)
					SDL_CondSignal(d->empty_queue_cond);
				if (d->packet_pending) {
					av_packet_move_ref(&pkt, &d->pkt);
					d->packet_pending = 0;
				}
				else {
					if (_packetQueue->packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial) < 0)
						return -1;
				}
			} while (d->queue->serial != d->pkt_serial);

			if (pkt.data == flush_pkt.data) {
				avcodec_flush_buffers(d->avctx);
				d->finished = 0;
				d->next_pts = d->start_pts;
				d->next_pts_tb = d->start_pts_tb;
			}
			else {
				if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
					int got_frame = 0;
					ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, &pkt);
					if (ret < 0) {
						ret = AVERROR(EAGAIN);
					}
					else {
						if (got_frame && !pkt.data) {
							d->packet_pending = 1;
							av_packet_move_ref(&d->pkt, &pkt);
						}
						ret = got_frame ? 0 : (pkt.data ? AVERROR(EAGAIN) : AVERROR_EOF);
					}
				}
				else {
					if (avcodec_send_packet(d->avctx, &pkt) == AVERROR(EAGAIN)) {
						av_log(d->avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
						d->packet_pending = 1;
						av_packet_move_ref(&d->pkt, &pkt);
					}
				}
				av_packet_unref(&pkt);
			}
		}
	}

	void MediaState::decoder_destroy(Decoder *d)
	{
		av_packet_unref(&d->pkt);
		avcodec_free_context(&d->avctx);
	}

	void MediaState::decoder_abort(Decoder *d, FrameQueue *fq)
	{
		_packetQueue->packet_queue_abort(d->queue);
		_frameQueue->frame_queue_signal(fq);
		SDL_WaitThread(d->decoder_tid, NULL);
		d->decoder_tid = NULL;
		_packetQueue->packet_queue_flush(d->queue);
	}

	void MediaState::stream_component_close(int stream_index)
	{
		AVFormatContext *ic = is->ic;
		AVCodecParameters *codecpar;

		if (stream_index < 0 || stream_index >= ic->nb_streams)
			return;
		codecpar = ic->streams[stream_index]->codecpar;

		switch (codecpar->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			decoder_abort(&is->auddec, &is->sampq);
			SDL_CloseAudioDevice(audio_dev);
			decoder_destroy(&is->auddec);
			swr_free(&is->swr_ctx);
			av_freep(&is->audio_buf1);
			is->audio_buf1_size = 0;
			is->audio_buf = NULL;

			if (is->rdft) {
				av_rdft_end(is->rdft);
				av_freep(&is->rdft_data);
				is->rdft = NULL;
				is->rdft_bits = 0;
			}
			break;
		case AVMEDIA_TYPE_VIDEO:
			decoder_abort(&is->viddec, &is->pictq);
			decoder_destroy(&is->viddec);
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			decoder_abort(&is->subdec, &is->subpq);
			decoder_destroy(&is->subdec);
			break;
		default:
			break;
		}

		ic->streams[stream_index]->discard = AVDISCARD_ALL;
		switch (codecpar->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			is->audio_st = NULL;
			is->audio_stream = -1;
			break;
		case AVMEDIA_TYPE_VIDEO:
			is->video_st = NULL;
			is->video_stream = -1;
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			is->subtitle_st = NULL;
			is->subtitle_stream = -1;
			break;
		default:
			break;
		}
	}

	void MediaState::stream_close()
	{
		/* XXX: use a special url_shutdown call to abort parse cleanly */
		is->abort_request = 1;
		SDL_WaitThread(is->read_tid, NULL);

		/* close each stream */
		if (is->audio_stream >= 0)
			stream_component_close(is->audio_stream);
		if (is->video_stream >= 0)
			stream_component_close(is->video_stream);
		if (is->subtitle_stream >= 0)
			stream_component_close(is->subtitle_stream);

		avformat_close_input(&is->ic);

		_packetQueue->packet_queue_destroy(&is->videoq);
		_packetQueue->packet_queue_destroy(&is->audioq);
		_packetQueue->packet_queue_destroy(&is->subtitleq);

		/* free all pictures */
		_frameQueue->frame_queue_destory(&is->pictq);
		_frameQueue->frame_queue_destory(&is->sampq);
		_frameQueue->frame_queue_destory(&is->subpq);
		SDL_DestroyCond(is->continue_read_thread);
		sws_freeContext(is->img_convert_ctx);
		sws_freeContext(is->sub_convert_ctx);
		if (is->filename != nullptr)
			av_free(is->filename);

		if (displayFrame!=nullptr)
		{
			av_free(displayFrame->data[0]);
			av_frame_free(&displayFrame);
		}

		if (_bgra != nullptr)
		{
			free(_bgra);
			free(_rgba);
			_bgra = nullptr;
			_rgba = nullptr;
		}
//andy
// 		if (is->vid_texture)
// 			SDL_DestroyTexture(is->vid_texture);
// 		if (is->sub_texture)
// 			SDL_DestroyTexture(is->sub_texture);
		av_free(is);
		is = nullptr;
	}

	void MediaState::do_exit()
	{
		if (is) {
			stream_close();
			av_log(NULL, AV_LOG_QUIET, "%s", "");
		}
	}

	double MediaState::get_clock(Clock *c)
	{
		if (*c->queue_serial != c->serial)
			return NAN;
		if (c->paused) {
			return c->pts;
		}
		else {
			double time = av_gettime_relative() / 1000000.0;
			return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
		}
	}

	void MediaState::set_clock_at(Clock *c, double pts, int serial, double time)
	{
		c->pts = pts;
		c->last_updated = time;
		c->pts_drift = c->pts - time;
		c->serial = serial;
	}

	void MediaState::set_clock(Clock *c, double pts, int serial)
	{
		double time = av_gettime_relative() / 1000000.0;
		set_clock_at(c, pts, serial, time);
	}

	void MediaState::set_clock_speed(Clock *c, double speed)
	{
		set_clock(c, get_clock(c), c->serial);
		c->speed = speed;
	}

	void MediaState::init_clock(Clock *c, int *queue_serial)
	{
		c->speed = 1.0;
		c->paused = 0;
		c->queue_serial = queue_serial;
		set_clock(c, NAN, -1);
	}

	void MediaState::sync_clock_to_slave(Clock *c, Clock *slave)
	{
		double clock = get_clock(c);
		double slave_clock = get_clock(slave);
		if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
			set_clock(c, slave_clock, slave->serial);
	}

	int MediaState::get_master_sync_type() {
		if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
			if (is->video_st)
				return AV_SYNC_VIDEO_MASTER;
			else
				return AV_SYNC_AUDIO_MASTER;
		}
		else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
			if (is->audio_st)
				return AV_SYNC_AUDIO_MASTER;
			else
				return AV_SYNC_EXTERNAL_CLOCK;
		}
		else {
			return AV_SYNC_EXTERNAL_CLOCK;
		}
	}

	double MediaState::get_master_clock()
	{
		double val;

		switch (get_master_sync_type()) {
		case AV_SYNC_VIDEO_MASTER:
			val = get_clock(&is->vidclk);
			break;
		case AV_SYNC_AUDIO_MASTER:
			val = get_clock(&is->audclk);
			break;
		default:
			val = get_clock(&is->extclk);
			break;
		}
		return val;
	}

	void MediaState::check_external_clock_speed() 
	{
		if ((is->video_stream >= 0 && is->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) ||
			(is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES)) {
			set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
		}
		else if ((is->video_stream < 0 || is->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
			(is->audio_stream < 0 || is->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES)) {
			set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
		}
		else {
			double speed = is->extclk.speed;
			if (speed != 1.0)
				set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
		}
	}

	void MediaState::setSeek(double seconds)
	{
		SDL_Event event;

		event.type = FF_SEEK_EVENT_ID(_uniqueID);
		double* a = new double;
		*a = seconds;
		event.user.data1 = a;
		SDL_PushEvent(&event);
	}

	void MediaState::stream_seek(int64_t pos, int64_t rel, int seek_by_bytes)
	{
		if (!is->seek_req) {
			is->seek_pos = pos;
			is->seek_rel = rel;
			is->seek_flags &= ~AVSEEK_FLAG_BYTE;
			if (seek_by_bytes)
				is->seek_flags |= AVSEEK_FLAG_BYTE;
			is->seek_req = 1;
			SDL_CondSignal(is->continue_read_thread);
		}
	}

	void MediaState::stream_toggle_pause()
	{
		if (is->paused) {
			is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
			if (is->read_pause_return != AVERROR(ENOSYS)) {
				is->vidclk.paused = 0;
			}
			set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
		}
		set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
		is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = !is->paused;
	}

	void MediaState::toggle_pause()
	{
		stream_toggle_pause();
		is->step = 0;
	}

	void MediaState::toggle_mute()
	{
		is->muted = !is->muted;
	}

	void MediaState::update_volume(int sign, double step)
	{
		double volume_level = is->audio_volume ? (20 * log(is->audio_volume / (double)SDL_MIX_MAXVOLUME) / log(10)) : -1000.0;
		int new_volume = lrint(SDL_MIX_MAXVOLUME * pow(10.0, (volume_level + sign * step) / 20.0));
		is->audio_volume = av_clip(is->audio_volume == new_volume ? (is->audio_volume + sign) : new_volume, 0, SDL_MIX_MAXVOLUME);
	}

	void MediaState::step_to_next_frame()
	{
		/* if the stream is paused unpause it, then step */
		if (is->paused)
			stream_toggle_pause();
		is->step = 1;
	}

	double MediaState::compute_target_delay(double delay)
	{
		double sync_threshold, diff = 0;

		// 如果不是以视频做为同步基准，则计算延时
		if (get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {
			/* if video is slave, we try to correct big delays by
			duplicating or deleting a frame */
			// 计算时间差
			diff = get_clock(&is->vidclk) - get_master_clock();

			/* skip or repeat frame. We take into account the
			delay to compute the threshold. I still don't know
			if it is the best guess */
			// 计算同步预支
			sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
			if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
				// 滞后
				if (diff <= -sync_threshold)
					delay = FFMAX(0, delay + diff);
				else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)// 超前
					delay = delay + diff;
				else if (diff >= sync_threshold)// 超过了理论阈值
					delay = 2 * delay;
			}
		}

		av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n",
			delay, -diff);

		return delay;
	}

	double MediaState::vp_duration(Frame *vp, Frame *nextvp) {
		if (vp->serial == nextvp->serial) {
			double duration = nextvp->pts - vp->pts;
			if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
				return vp->duration;
			else
				return duration;
		}
		else {
			return 0.0;
		}
	}

	void MediaState::update_video_pts(double pts, int64_t pos, int serial) {
		/* update current video pts */
		set_clock(&is->vidclk, pts, serial);
		sync_clock_to_slave(&is->extclk, &is->vidclk);
	}

	int MediaState::queue_picture(AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
	{
		Frame *vp;

#if defined(DEBUG_SYNC)
		printf("frame_type=%c pts=%0.3f\n",
			av_get_picture_type_char(src_frame->pict_type), pts);
#endif

		if (!(vp = _frameQueue->frame_queue_peek_writable(&is->pictq)))
			return -1;

		vp->sar = src_frame->sample_aspect_ratio;
		vp->uploaded = 0;

		vp->width = src_frame->width;
		vp->height = src_frame->height;
		vp->format = src_frame->format;

		vp->pts = pts;
		vp->duration = duration;
		vp->pos = pos;
		vp->serial = serial;

		//andy
		//set_default_window_size(vp->width, vp->height, vp->sar);

		av_frame_move_ref(vp->frame, src_frame);
		_frameQueue->frame_queue_push(&is->pictq);
		return 0;
	}

	int MediaState::get_video_frame(AVFrame *frame)
	{
		int got_picture;

		if ((got_picture = decoder_decode_frame(&is->viddec, frame, NULL)) < 0)
			return -1;

		if (got_picture) {
			double dpts = NAN;

			if (frame->pts != AV_NOPTS_VALUE)
				dpts = av_q2d(is->video_st->time_base) * frame->pts;

			frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

			if (framedrop > 0 || (framedrop && get_master_sync_type() != AV_SYNC_VIDEO_MASTER)) {
				if (frame->pts != AV_NOPTS_VALUE) {
					double diff = dpts - get_master_clock();
					if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
						diff - is->frame_last_filter_delay < 0 &&
						is->viddec.pkt_serial == is->vidclk.serial &&
						is->videoq.nb_packets) {
						is->frame_drops_early++;
						av_frame_unref(frame);
						got_picture = 0;
					}
				}
			}
		}

		return got_picture;
	}

	int MediaState::synchronize_audio(int nb_samples)
	{
		int wanted_nb_samples = nb_samples;

		// 如果不是以音频同步，则尝试通过移除或增加采样来纠正时钟
		if (get_master_sync_type() != AV_SYNC_AUDIO_MASTER) {
			double diff, avg_diff;
			int min_nb_samples, max_nb_samples;

			// 获取音频时钟跟主时钟的差值
			diff = get_clock(&is->audclk) - get_master_clock();
			// 判断差值是否存在，并且在非同步阈值范围内
			if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
				// 计算新的差值
				is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
				// 记录差值的数量
				if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
					/* not enough measures to have a correct estimate */
					is->audio_diff_avg_count++;
				}
				else {
					/* estimate the A-V difference */
					// 估计音频和视频的时钟差值
					avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
					// 判断平均差值是否超过了音频差的阈值，如果超过，则计算新的采样值
					if (fabs(avg_diff) >= is->audio_diff_threshold) {
						wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
						min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
						max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
						wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
					}
					av_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
						diff, avg_diff, wanted_nb_samples - nb_samples,
						is->audio_clock, is->audio_diff_threshold);
				}
			}
			else {
				/* too big difference : may be initial PTS errors, so
				reset A-V filter */
				// 如果差值过大，重置防止pts出错
				is->audio_diff_avg_count = 0;
				is->audio_diff_cum = 0;
			}
		}

		return wanted_nb_samples;
	}

	int MediaState::audio_decode_frame()
	{
		int data_size, resampled_data_size;
		int64_t dec_channel_layout;
		av_unused double audio_clock0;
		int wanted_nb_samples;
		Frame *af;

		if (is->paused)
			return -1;

		do {
#if defined(_WIN32)
			while (_frameQueue->frame_queue_nb_remaining(&is->sampq) == 0) {
				if ((av_gettime_relative() - audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
					return -1;
				av_usleep(1000);
			}
#endif
			if (!(af = _frameQueue->frame_queue_peek_readable(&is->sampq)))
				return -1;
			_frameQueue->frame_queue_next(&is->sampq);
		} while (af->serial != is->audioq.serial);

		data_size = av_samples_get_buffer_size(NULL, af->frame->channels,
			af->frame->nb_samples,
			(AVSampleFormat)af->frame->format, 1);

		dec_channel_layout =
			(af->frame->channel_layout && af->frame->channels == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
			af->frame->channel_layout : av_get_default_channel_layout(af->frame->channels);
		wanted_nb_samples = synchronize_audio(af->frame->nb_samples);

		if (af->frame->format != is->audio_src.fmt ||
			dec_channel_layout != is->audio_src.channel_layout ||
			af->frame->sample_rate != is->audio_src.freq ||
			(wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx)) {
			swr_free(&is->swr_ctx);
			is->swr_ctx = swr_alloc_set_opts(NULL,
				is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
				dec_channel_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
				0, NULL);
			if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
				av_log(NULL, AV_LOG_ERROR,
					"Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
					af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), af->frame->channels,
					is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
				swr_free(&is->swr_ctx);
				return -1;
			}
			is->audio_src.channel_layout = dec_channel_layout;
			is->audio_src.channels = af->frame->channels;
			is->audio_src.freq = af->frame->sample_rate;
			is->audio_src.fmt = (AVSampleFormat)af->frame->format;
		}

		if (is->swr_ctx) {
			const uint8_t **in = (const uint8_t **)af->frame->extended_data;
			uint8_t **out = &is->audio_buf1;
			int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
			int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
			int len2;
			if (out_size < 0) {
				av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
				return -1;
			}
			if (wanted_nb_samples != af->frame->nb_samples) {
				if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
					wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
					av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
					return -1;
				}
			}
			av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
			if (!is->audio_buf1)
				return AVERROR(ENOMEM);
			len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
			if (len2 < 0) {
				av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
				return -1;
			}
			if (len2 == out_count) {
				av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
				if (swr_init(is->swr_ctx) < 0)
					swr_free(&is->swr_ctx);
			}
			is->audio_buf = is->audio_buf1;
			resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
		}
		else {
			is->audio_buf = af->frame->data[0];
			resampled_data_size = data_size;
		}

		audio_clock0 = is->audio_clock;
		/* update the audio clock with the pts */
		if (!isnan(af->pts))
			is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
		else
			is->audio_clock = NAN;
		is->audio_clock_serial = af->serial;
#ifdef DEBUG
		{
			static double last_clock;
			printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
				is->audio_clock - last_clock,
				is->audio_clock, audio_clock0);
			last_clock = is->audio_clock;
		}
#endif
		return resampled_data_size;
	}

	void MediaState::sdl_audio_callback(void *opaque, Uint8 *stream, int len)
	{
		MediaState *ms = (MediaState*)opaque;
		VideoState *is = ms->is;
		int audio_size, len1;

		ms->audio_callback_time = av_gettime_relative();

		while (len > 0) {
			if (is->audio_buf_index >= is->audio_buf_size) {
				audio_size = ms->audio_decode_frame();
				if (audio_size < 0) {
					/* if error, just output silence */
					is->audio_buf = NULL;
					is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
				}
				else {
					is->audio_buf_size = audio_size;
				}
				is->audio_buf_index = 0;
			}
			len1 = is->audio_buf_size - is->audio_buf_index;
			if (len1 > len)
				len1 = len;
			if (!is->muted && is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
				memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
			else {
				memset(stream, 0, len1);
				if (!is->muted && is->audio_buf)
					SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, AUDIO_S16SYS, len1, is->audio_volume);
			}
			len -= len1;
			stream += len1;
			is->audio_buf_index += len1;
		}
		is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
		/* Let's assume the audio driver that is used by SDL has two periods. */
		if (!isnan(is->audio_clock)) {
			ms->set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, ms->audio_callback_time / 1000000.0);
			ms->sync_clock_to_slave(&is->extclk, &is->audclk);
		}
	}

	int MediaState::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
	{
		SDL_AudioSpec wanted_spec, spec;
		const char *env;
		static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
		static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
		int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

		env = SDL_getenv("SDL_AUDIO_CHANNELS");
		if (env) {
			wanted_nb_channels = atoi(env);
			wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
		}
		if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
			wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
			wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
		}
		wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
		wanted_spec.channels = wanted_nb_channels;
		wanted_spec.freq = wanted_sample_rate;
		if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
			av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
			return -1;
		}
		while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
			next_sample_rate_idx--;
		wanted_spec.format = AUDIO_S16SYS;
		wanted_spec.silence = 0;
		wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
		wanted_spec.callback = MediaState::sdl_audio_callback;
		wanted_spec.userdata = opaque;
		while (!(audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) 
		//while (SDL_OpenAudio(&wanted_spec, &spec) < 0)
		{
			av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
				wanted_spec.channels, wanted_spec.freq, SDL_GetError());
			wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
			if (!wanted_spec.channels) {
				wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
				wanted_spec.channels = wanted_nb_channels;
				if (!wanted_spec.freq) {
					av_log(NULL, AV_LOG_ERROR,
						"No more combinations to try, audio open failed\n");
					return -1;
				}
			}
			wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
		}
		if (spec.format != AUDIO_S16SYS) {
			av_log(NULL, AV_LOG_ERROR,
				"SDL advised audio format %d is not supported!\n", spec.format);
			return -1;
		}
		if (spec.channels != wanted_spec.channels) {
			wanted_channel_layout = av_get_default_channel_layout(spec.channels);
			if (!wanted_channel_layout) {
				av_log(NULL, AV_LOG_ERROR,
					"SDL advised channel count %d is not supported!\n", spec.channels);
				return -1;
			}
		}

		audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
		audio_hw_params->freq = spec.freq;
		audio_hw_params->channel_layout = wanted_channel_layout;
		audio_hw_params->channels = spec.channels;
		audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
		audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
		if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
			av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
			return -1;
		}
		return spec.size;
	}

	int MediaState::stream_component_open(int stream_index)
	{
		AVFormatContext *ic = is->ic;
		AVCodecContext *avctx;
		AVCodec *codec;
		const char *forced_codec_name = NULL;
		AVDictionary *opts = NULL;
		AVDictionaryEntry *t = NULL;
		int sample_rate, nb_channels;
		int64_t channel_layout;
		int ret = 0;
		int stream_lowres = lowres;
		uint8_t *buffer;
		int numBytes;

		if (stream_index < 0 || stream_index >= ic->nb_streams)
			return -1;

		// 创建解码上下文
		avctx = avcodec_alloc_context3(NULL);
		if (!avctx)
			return AVERROR(ENOMEM);

		// 复制解码器信息到解码上下文
		ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
		if (ret < 0)
			goto fail;
		avctx->pkt_timebase = ic->streams[stream_index]->time_base;
		// 查找解码器
		codec = avcodec_find_decoder(avctx->codec_id);
		// 判断解码器类型，设置流的索引并根据类型设置解码名称
// 		switch (avctx->codec_type) {
// 		case AVMEDIA_TYPE_AUDIO: is->last_audio_stream = stream_index; forced_codec_name = audio_codec_name; break;
// 		case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; forced_codec_name = subtitle_codec_name; break;
// 		case AVMEDIA_TYPE_VIDEO: is->last_video_stream = stream_index; forced_codec_name = video_codec_name; break;
// 		}
// 		if (forced_codec_name)
// 			codec = avcodec_find_decoder_by_name(forced_codec_name);
		if (!codec) {
			if (forced_codec_name) av_log(NULL, AV_LOG_WARNING,
				"No codec could be found with name '%s'\n", forced_codec_name);
			else                   av_log(NULL, AV_LOG_WARNING,
				"No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
			ret = AVERROR(EINVAL);
			goto fail;
		}
		// 设置解码器的Id
		avctx->codec_id = codec->id;
		// 判断是否需要重新设置lowres的值
		if (stream_lowres > codec->max_lowres) {
			av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
				codec->max_lowres);
			stream_lowres = codec->max_lowres;
		}
		avctx->lowres = stream_lowres;

		if (fast)
			avctx->flags2 |= AV_CODEC_FLAG2_FAST;

		opts = filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);
		if (!av_dict_get(opts, "threads", NULL, 0))
			av_dict_set(&opts, "threads", "auto", 0);
		if (stream_lowres)
			av_dict_set_int(&opts, "lowres", stream_lowres, 0);
		if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
			av_dict_set(&opts, "refcounted_frames", "1", 0);
		if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
			goto fail;
		}
		if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
			av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
			ret = AVERROR_OPTION_NOT_FOUND;
			goto fail;
		}

		is->eof = 0;
		ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
		switch (avctx->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			sample_rate = avctx->sample_rate;
			nb_channels = avctx->channels;
			channel_layout = avctx->channel_layout;

			/* prepare audio output */
			if ((ret = audio_open(this, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0)
				goto fail;
			is->audio_hw_buf_size = ret;
			is->audio_src = is->audio_tgt;
			is->audio_buf_size = 0;
			is->audio_buf_index = 0;

			/* init averaging filter */
			is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
			is->audio_diff_avg_count = 0;
			/* since we do not have a precise anough audio FIFO fullness,
			we correct audio sync only if larger than this threshold */
			is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

			is->audio_stream = stream_index;
			is->audio_st = ic->streams[stream_index];
			// 音频解码器初始化
			decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
			if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
				is->auddec.start_pts = is->audio_st->start_time;
				is->auddec.start_pts_tb = is->audio_st->time_base;
			}
			if ((ret = decoder_start(&is->auddec, audio_thread, this)) < 0)
				goto out;
			SDL_PauseAudioDevice(audio_dev, 0);
			break;
		case AVMEDIA_TYPE_VIDEO:

			displayFrame = av_frame_alloc();

			displayFrame->format = AV_PIX_FMT_YUV420P;
			displayFrame->width = avctx->width;
			displayFrame->height = avctx->height;

			numBytes = avpicture_get_size((AVPixelFormat)displayFrame->format, displayFrame->width, displayFrame->height);
			buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

			avpicture_fill((AVPicture*)displayFrame, buffer, (AVPixelFormat)displayFrame->format, displayFrame->width, displayFrame->height);

			is->video_stream = stream_index;
			is->video_st = ic->streams[stream_index];
			// 视频解码器初始化
			decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);
			if ((ret = decoder_start(&is->viddec, video_thread, this)) < 0)
				goto out;
			is->queue_attachments_req = 1;
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			is->subtitle_stream = stream_index;
			is->subtitle_st = ic->streams[stream_index];
			// 字幕解码器初始化
			decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread);
			if ((ret = decoder_start(&is->subdec, subtitle_thread, this)) < 0)
				goto out;
			break;
		default:
			break;
		}
		goto out;

	fail:
		avcodec_free_context(&avctx);
	out:
		av_dict_free(&opts);

		return ret;
	}

	/********************************************read tread*************************************************/
	int MediaState::decode_interrupt_cb(void *ctx)
	{
		VideoState *is = (VideoState*)ctx;
		return is->abort_request;
	}

	int MediaState::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
		return stream_id < 0 ||
			queue->abort_request ||
			(st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
			(queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0));
	}

	int MediaState::is_realtime(AVFormatContext *s)
	{
		if (!strcmp(s->iformat->name, "rtp")
			|| !strcmp(s->iformat->name, "rtsp")
			|| !strcmp(s->iformat->name, "sdp")
			)
			return 1;

		if (s->pb && (!strncmp(s->url, "rtp:", 4)
			|| !strncmp(s->url, "udp:", 4)
			)
			)
			return 1;
		return 0;
	}

	int MediaState::getDuration()
	{
		if (is->ic == nullptr) return -1;
		int64_t duration = is->ic->duration + (is->ic->duration <= INT64_MAX - 5000 ? 5000 : 0);
		int secs = duration / AV_TIME_BASE;
		return secs;
	}

	int MediaState::read_thread(void *arg)
	{
		MediaState *self = (MediaState *)arg;
		VideoState *is = self->is;
		AVFormatContext *ic = NULL;
		int err, i, ret;
		int st_index[AVMEDIA_TYPE_NB];
		AVPacket pkt1, *pkt = &pkt1;
		int64_t stream_start_time;
		int pkt_in_play_range = 0;
		AVDictionaryEntry *t;
		SDL_mutex *wait_mutex = SDL_CreateMutex();
		int scan_all_pmts_set = 0;
		int64_t pkt_ts;

		if (!wait_mutex) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		// 设置初始值
		memset(st_index, -1, sizeof(st_index));

		SDL_LockMutex(self->vs_mutex);

		is->last_video_stream = is->video_stream = -1;
		is->last_audio_stream = is->audio_stream = -1;
		is->last_subtitle_stream = is->subtitle_stream = -1;
		is->eof = 0;

		// 创建输入上下文
		ic = avformat_alloc_context();
		if (!ic) {
			av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		// 设置解码中断回调方法
		ic->interrupt_callback.callback = decode_interrupt_cb;
		// 设置中断回调参数
		ic->interrupt_callback.opaque = is;
		// 获取参数
		if (!av_dict_get(self->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
			av_dict_set(&self->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
			scan_all_pmts_set = 1;
		}
		// 打开文件
		if (is->filename != nullptr)
		{
			err = avformat_open_input(&ic, is->filename, is->iformat, &self->format_opts);
			if (err < 0) {
				print_error(is->filename, err);
				ret = -1;
				goto fail;
			}
		}
		else if (is->filebuffer != nullptr)
		{
			unsigned char *aviobuffer = (unsigned char *)av_malloc(32768);
			AVIOContext *avio = avio_alloc_context(aviobuffer, 32768, 0, self, read_buffer, NULL, NULL);
			ic->pb = avio;

			if (avformat_open_input(&ic, NULL, NULL, NULL) != 0)
				return false;
		}
		else
		{
			goto fail;
		}

		if (scan_all_pmts_set)
			av_dict_set(&self->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

		if ((t = av_dict_get(self->format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
			av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
			ret = AVERROR_OPTION_NOT_FOUND;
			goto fail;
		}
		is->ic = ic;

		av_format_inject_global_side_data(ic);

		if (self->find_stream_info) {
			AVDictionary **opts = self->setup_find_stream_info_opts(ic, self->codec_opts);
			int orig_nb_streams = ic->nb_streams;

			err = avformat_find_stream_info(ic, opts);

			for (i = 0; i < orig_nb_streams; i++)
				av_dict_free(&opts[i]);
			av_freep(&opts);

			if (err < 0) {
				av_log(NULL, AV_LOG_WARNING,
					"could not find codec parameters\n");
				ret = -1;
				goto fail;
			}
		}

		if (ic->pb)
			ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

		if (self->seek_by_bytes < 0)
			self->seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);

		is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

		/* if seeking requested, we execute it */
		if (self->start_time != AV_NOPTS_VALUE) {
			int64_t timestamp;

			timestamp = self->start_time;
			/* add the stream start time */
			if (ic->start_time != AV_NOPTS_VALUE)
				timestamp += ic->start_time;
			// 定位文件
			ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
			if (ret < 0) {
				av_log(NULL, AV_LOG_WARNING, "could not seek to position %0.3f\n",
					(double)timestamp / AV_TIME_BASE);
			}
		}

		is->realtime = self->is_realtime(ic);

		if (self->show_status && is->filename!=nullptr)
			av_dump_format(ic, 0, is->filename, 0);

		// 获取码流对应的索引
		for (i = 0; i < ic->nb_streams; i++) {
			AVStream *st = ic->streams[i];
			enum AVMediaType type = st->codecpar->codec_type;
			st->discard = AVDISCARD_ALL;
			if (type >= 0 && self->wanted_stream_spec[type] && st_index[type] == -1)
				if (avformat_match_stream_specifier(ic, st, self->wanted_stream_spec[type]) > 0)
					st_index[type] = i;
		}
		for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
			if (self->wanted_stream_spec[i] && st_index[i] == -1) {
				av_log(NULL, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n", self->wanted_stream_spec[i], av_get_media_type_string((AVMediaType)i));
				st_index[i] = INT_MAX;
			}
		}

		// 查找视频流
		if (!self->video_disable)
			st_index[AVMEDIA_TYPE_VIDEO] =
			av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
				st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

		// 查找音频流
		if (!self->audio_disable)
			st_index[AVMEDIA_TYPE_AUDIO] =
			av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
				st_index[AVMEDIA_TYPE_AUDIO],
				st_index[AVMEDIA_TYPE_VIDEO],
				NULL, 0);

		// 查找字幕流
		if (!self->video_disable && !self->subtitle_disable)
			st_index[AVMEDIA_TYPE_SUBTITLE] =
			av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
				st_index[AVMEDIA_TYPE_SUBTITLE],
				(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
					st_index[AVMEDIA_TYPE_AUDIO] :
					st_index[AVMEDIA_TYPE_VIDEO]),
				NULL, 0);

		// 设置显示模式
		is->show_mode = self->show_mode;

		// 判断视频流是否存在
		/*
		if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
			AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
			AVCodecParameters *codecpar = st->codecpar;
			AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
 			if (codecpar->width)
 				set_default_window_size(codecpar->width, codecpar->height, sar);
		}
		*/
		/* open the streams */
		if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
			self->stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
		}

		ret = -1;
		if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
			ret = self->stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
		}
		if (is->show_mode == VideoState::ShowMode::SHOW_MODE_NONE)
			is->show_mode = ret >= 0 ? VideoState::ShowMode::SHOW_MODE_VIDEO : VideoState::ShowMode::SHOW_MODE_RDFT;

		if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
			self->stream_component_open(st_index[AVMEDIA_TYPE_SUBTITLE]);
		}

		// 如果音频流和视频流都不存在，则退出
		if (is->video_stream < 0 && is->audio_stream < 0) {
			av_log(NULL, AV_LOG_FATAL, "Failed to open file or configure filtergraph\n");
			ret = -1;
			goto fail;
		}

		if (self->infinite_buffer < 0 && is->realtime)
			self->infinite_buffer = 1;

		SDL_UnlockMutex(self->vs_mutex);

		self->_duration = self->getDuration();

		for (;;) {
			if (is->abort_request)
				break;
			if (is->paused != is->last_paused) {
				is->last_paused = is->paused;
				if (is->paused)
					is->read_pause_return = av_read_pause(ic);
				else
					av_read_play(ic);
			}

			// 定位文件
			if (is->seek_req) {
				int64_t seek_target = is->seek_pos;
				int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
				int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;
				// FIXME the +-2 is due to rounding being not done in the correct direction in generation
				//      of the seek_pos/seek_rel variables

				ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR,
						"%s: error while seeking\n", is->ic->url);
				}
				else {
					// 音频入队
					if (is->audio_stream >= 0) {
						self->_packetQueue->packet_queue_flush(&is->audioq);
						self->_packetQueue->packet_queue_put(&is->audioq, &self->flush_pkt);
					}
					// 字幕入队
					if (is->subtitle_stream >= 0) {
						self->_packetQueue->packet_queue_flush(&is->subtitleq);
						self->_packetQueue->packet_queue_put(&is->subtitleq, &self->flush_pkt);
					}
					// 视频入队
					if (is->video_stream >= 0) {
						self->_packetQueue->packet_queue_flush(&is->videoq);
						self->_packetQueue->packet_queue_put(&is->videoq, &self->flush_pkt);
					}
					// 根据定位的标志设置时钟
					if (is->seek_flags & AVSEEK_FLAG_BYTE) {
						self->set_clock(&is->extclk, NAN, 0);
					}
					else {
						self->set_clock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
					}
				}
				is->seek_req = 0;
				is->queue_attachments_req = 1;
				is->eof = 0;
				if (is->paused)
					self->step_to_next_frame();
			}
			if (is->queue_attachments_req) {
				if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
					AVPacket copy = { 0 };
					if ((ret = av_packet_ref(&copy, &is->video_st->attached_pic)) < 0)
						goto fail;
					self->_packetQueue->packet_queue_put(&is->videoq, &copy);
					self->_packetQueue->packet_queue_put_nullpacket(&is->videoq, is->video_stream);
				}
				is->queue_attachments_req = 0;
			}

			/* if the queue are full, no need to read more */
			// 待解码数据写入队列失败，并且待解码数据还有足够的包时，等待待解码队列的数据消耗掉
			if (self->infinite_buffer < 1 &&
				(is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
					|| (self->stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq) &&
						self->stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq) &&
						self->stream_has_enough_packets(is->subtitle_st, is->subtitle_stream, &is->subtitleq)))) {
				/* wait 10 ms */
				SDL_LockMutex(wait_mutex);
				SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
				SDL_UnlockMutex(wait_mutex);
				continue;
			}
			if (!is->paused &&
				(!is->audio_st || (is->auddec.finished == is->audioq.serial && self->_frameQueue->frame_queue_nb_remaining(&is->sampq) == 0)) &&
				(!is->video_st || (is->viddec.finished == is->videoq.serial && self->_frameQueue->frame_queue_nb_remaining(&is->pictq) == 0))) {
				if (self->loop != 1 && (!self->loop || --self->loop)) {
					self->stream_seek(self->start_time != AV_NOPTS_VALUE ? self->start_time : 0, 0, 0);
				}
				else{
					//播放结束
					//ret = AVERROR_EOF;
					//goto fail;
				}
			}
			// 读取数据包
			ret = av_read_frame(ic, pkt);
			if (ret < 0) {
				// 读取结束或失败
				if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
					if (is->video_stream >= 0)
						self->_packetQueue->packet_queue_put_nullpacket(&is->videoq, is->video_stream);
					if (is->audio_stream >= 0)
						self->_packetQueue->packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
					if (is->subtitle_stream >= 0)
						self->_packetQueue->packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
					is->eof = 1;

					SDL_Event event;
					event.type = FF_OVER_EVENT_ID(self->_uniqueID);
					event.user.data1 = is;
					SDL_PushEvent(&event);
				}
				if (ic->pb && ic->pb->error)
					break;
				SDL_LockMutex(wait_mutex);
				SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
				SDL_UnlockMutex(wait_mutex);
				continue;
			}
			else {
				is->eof = 0;
			}
			/* check if packet is in play range specified by user, then queue, otherwise discard */
			stream_start_time = ic->streams[pkt->stream_index]->start_time;
			pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
			pkt_in_play_range = self->duration == AV_NOPTS_VALUE ||
				(pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
				av_q2d(ic->streams[pkt->stream_index]->time_base) -
				(double)(self->start_time != AV_NOPTS_VALUE ? self->start_time : 0) / 1000000
				<= ((double)self->duration / 1000000);
			// 将解复用得到的数据包添加到对应的待解码队列中
			if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
				self->_packetQueue->packet_queue_put(&is->audioq, pkt);
			}
			else if (pkt->stream_index == is->video_stream && pkt_in_play_range
				&& !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
				self->_packetQueue->packet_queue_put(&is->videoq, pkt);
			}
			else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
				self->_packetQueue->packet_queue_put(&is->subtitleq, pkt);
			}
			else {
				av_packet_unref(pkt);
			}
		}

		ret = 0;
	fail:
		if (ic && !is->ic)
			avformat_close_input(&ic);

		if (ret == AVERROR_EOF)
		{
			SDL_Event event;

			event.type = FF_OVER_EVENT_ID(self->_uniqueID);
			event.user.data1 = is;
			SDL_PushEvent(&event);
		}
		else if (ret != 0) {
			SDL_Event event;

			event.type = FF_QUIT_EVENT_ID(self->_uniqueID);
			event.user.data1 = is;
			SDL_PushEvent(&event);
		}
		
		SDL_DestroyMutex(wait_mutex);
		return 0;
	}

	void MediaState::pause()
	{
		if (is->paused)
		{
			return;
		}
		SDL_Event event;

		event.type = FF_PAUSE_EVENT_ID(_uniqueID);
		event.user.data1 = is;
		int ret = SDL_PushEvent(&event);
		if (ret!=1)
		{
			av_log(NULL, AV_LOG_WARNING, "push pause event failed\n");
		}
	}

	void MediaState::resume()
	{
		if (!is->paused)
		{
			return;
		}
		SDL_Event event;

		event.type = FF_PAUSE_EVENT_ID(_uniqueID);
		event.user.data1 = is;
		int ret = SDL_PushEvent(&event);
		if (ret != 1)
		{
			av_log(NULL, AV_LOG_WARNING, "push resume event failed\n");
		}
	}

	void MediaState::mute()
	{
		SDL_Event event;

		event.type = FF_MUTE_EVENT_ID(_uniqueID);
		event.user.data1 = is;
		int ret = SDL_PushEvent(&event);
		if (ret != 1)
		{
			av_log(NULL, AV_LOG_WARNING, "push mute event failed\n");
		}
	}

	void MediaState::unmute()
	{
		SDL_Event event;

		event.type = FF_MUTE_EVENT_ID(_uniqueID);
		event.user.data1 = is;
		int ret = SDL_PushEvent(&event);
		if (ret != 1)
		{
			av_log(NULL, AV_LOG_WARNING, "push unmute event failed\n");
		}

	}

	/*******************************************************************************************************************/
	VideoState *MediaState::stream_open(char *filebuffer, unsigned int filebuffersize)
	{
		is = (VideoState*)av_mallocz(sizeof(VideoState));
		if (!is)
			return NULL;
		is->filename = nullptr;
		is->filebuffer = filebuffer;
		is->filebuffersize = filebuffersize;
		is->iformat = nullptr;
		is->ytop = 0;
		is->xleft = 0;
		is->paused = false;
		/* start video display */
		if (_frameQueue->frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
			goto fail;
		if (_frameQueue->frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
			goto fail;
		if (_frameQueue->frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
			goto fail;

		if (_packetQueue->packet_queue_init(&is->videoq) < 0 ||
			_packetQueue->packet_queue_init(&is->audioq) < 0 ||
			_packetQueue->packet_queue_init(&is->subtitleq) < 0)
			goto fail;

		if (!(is->continue_read_thread = SDL_CreateCond())) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
			goto fail;
		}

		init_clock(&is->vidclk, &is->videoq.serial);
		init_clock(&is->audclk, &is->audioq.serial);
		init_clock(&is->extclk, &is->extclk.serial);
		is->audio_clock_serial = -1;
		if (startup_volume < 0)
			av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", startup_volume);
		if (startup_volume > 100)
			av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", startup_volume);
		startup_volume = av_clip(startup_volume, 0, 100);
		startup_volume = av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
		is->audio_volume = startup_volume;
		is->muted = false;
		is->av_sync_type = av_sync_type;
		is->read_tid = SDL_CreateThread(read_thread, "read_thread", this);
		if (!is->read_tid) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
		fail:
			stream_close();
			return NULL;
		}
		return is;
	}

	VideoState *MediaState::stream_open(const char *filename, AVInputFormat *iformat)
	{
		is = (VideoState*)av_mallocz(sizeof(VideoState));
		if (!is)
			return NULL;
		is->filename = av_strdup(filename);
		if (!is->filename)
			goto fail;
		is->filebuffer = nullptr;
		is->iformat = iformat;
		is->ytop = 0;
		is->xleft = 0;
		is->paused = false;
		/* start video display */
		if (_frameQueue->frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
			goto fail;
		if (_frameQueue->frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
			goto fail;
		if (_frameQueue->frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
			goto fail;

		if (_packetQueue->packet_queue_init(&is->videoq) < 0 ||
			_packetQueue->packet_queue_init(&is->audioq) < 0 ||
			_packetQueue->packet_queue_init(&is->subtitleq) < 0)
			goto fail;

		if (!(is->continue_read_thread = SDL_CreateCond())) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
			goto fail;
		}

		init_clock(&is->vidclk, &is->videoq.serial);
		init_clock(&is->audclk, &is->audioq.serial);
		init_clock(&is->extclk, &is->extclk.serial);
		is->audio_clock_serial = -1;
		if (startup_volume < 0)
			av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", startup_volume);
		if (startup_volume > 100)
			av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", startup_volume);
		startup_volume = av_clip(startup_volume, 0, 100);
		startup_volume = av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
		is->audio_volume = startup_volume;
		is->muted = false;
		is->av_sync_type = av_sync_type;
		is->read_tid = SDL_CreateThread(read_thread, "read_thread", this);
		if (!is->read_tid) {
			av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
		fail:
			stream_close();
			return NULL;
		}
		return is;
	}

	int MediaState::refresh_loop_wait_event(SDL_Event *event)
	{
		int  ret = SDL_WaitEventTimeout(event, 0);

		if (remaining_time > 0.0)
			av_usleep((int64_t)(remaining_time * 1000000.0));
		remaining_time = REFRESH_RATE;
		if (is->show_mode != VideoState::ShowMode::SHOW_MODE_NONE && (!is->paused || is->force_refresh))
			video_refresh(&remaining_time);
		

		return ret;
	}

	void MediaState::video_refresh(double *remaining_time)
	{
		double time;

		Frame *sp, *sp2;

		if (!is->paused && get_master_sync_type() == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
			check_external_clock_speed();

		if (is->show_mode != VideoState::ShowMode::SHOW_MODE_VIDEO && is->audio_st) {
			time = av_gettime_relative() / 1000000.0;
			if (is->force_refresh || is->last_vis_time + rdftspeed < time) {
				video_display();
				is->last_vis_time = time;
			}
			*remaining_time = FFMIN(*remaining_time, is->last_vis_time + rdftspeed - time);
		}

		if (is->video_st) {
		retry:
			if (_frameQueue->frame_queue_nb_remaining(&is->pictq) == 0) {
				// nothing to do, no picture to display in the queue
			}
			else {
				double last_duration, duration, delay;
				Frame *vp, *lastvp;

				/* dequeue the picture */
				lastvp = _frameQueue->frame_queue_peek_last(&is->pictq);
				vp = _frameQueue->frame_queue_peek(&is->pictq);

				if (vp->serial != is->videoq.serial) {
					_frameQueue->frame_queue_next(&is->pictq);
					goto retry;
				}

				if (lastvp->serial != vp->serial)
					is->frame_timer = av_gettime_relative() / 1000000.0;

				if (is->paused)
					goto display;

				/* compute nominal last_duration */
				last_duration = vp_duration(lastvp, vp);
				delay = compute_target_delay(last_duration);

				time = av_gettime_relative() / 1000000.0;
				if (time < is->frame_timer + delay) {
					*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
					goto display;
				}

				is->frame_timer += delay;
				if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
					is->frame_timer = time;

				SDL_LockMutex(is->pictq.mutex);
				if (!isnan(vp->pts))
					update_video_pts(vp->pts, vp->pos, vp->serial);
				SDL_UnlockMutex(is->pictq.mutex);

				if (_frameQueue->frame_queue_nb_remaining(&is->pictq) > 1) {
					Frame *nextvp = _frameQueue->frame_queue_peek_next(&is->pictq);
					duration = vp_duration(vp, nextvp);
					if (!is->step && (framedrop > 0 || (framedrop && get_master_sync_type() != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
						is->frame_drops_late++;
						_frameQueue->frame_queue_next(&is->pictq);
						goto retry;
					}
				}

				if (is->subtitle_st) {
					while (_frameQueue->frame_queue_nb_remaining(&is->subpq) > 0) {
						sp = _frameQueue->frame_queue_peek(&is->subpq);

						if (_frameQueue->frame_queue_nb_remaining(&is->subpq) > 1)
							sp2 = _frameQueue->frame_queue_peek_next(&is->subpq);
						else
							sp2 = NULL;

						if (sp->serial != is->subtitleq.serial
							|| (is->vidclk.pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
							|| (sp2 && is->vidclk.pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
						{
							/*
							if (sp->uploaded) {
								int i;
								for (i = 0; i < sp->sub.num_rects; i++) {
									AVSubtitleRect *sub_rect = sp->sub.rects[i];
									uint8_t *pixels;
									int pitch, j;
// 
// 									if (!SDL_LockTexture(is->sub_texture, (SDL_Rect *)sub_rect, (void **)&pixels, &pitch)) {
// 										for (j = 0; j < sub_rect->h; j++, pixels += pitch)
// 											memset(pixels, 0, sub_rect->w << 2);
// 										SDL_UnlockTexture(is->sub_texture);
// 									}
								}
							}*/
							_frameQueue->frame_queue_next(&is->subpq);
						}
						else {
							break;
						}
					}
				}

				_frameQueue->frame_queue_next(&is->pictq);
				is->force_refresh = 1;

				if (is->step && !is->paused)
					stream_toggle_pause();
			}
		display:
			/* display picture */
			if (is->force_refresh && is->show_mode == VideoState::ShowMode::SHOW_MODE_VIDEO && is->pictq.rindex_shown)
				video_display();
		}
		is->force_refresh = 0;
	}

	void MediaState::video_display()
	{
		if (is->video_st)
			video_image_display();
	}

	void MediaState::video_image_display()
	{
		Frame *vp; //视频帧
		Frame *sp = NULL;

		vp = _frameQueue->frame_queue_peek_last(&is->pictq);
		if (is->subtitle_st) {
			if (_frameQueue->frame_queue_nb_remaining(&is->subpq) > 0) {
				sp = _frameQueue->frame_queue_peek(&is->subpq);

				if (vp->pts >= sp->pts + ((float)sp->sub.start_display_time / 1000)) {
					if (!sp->uploaded) {
						uint8_t* pixels[4];
						int pitch[4];
						int i;
						if (!sp->width || !sp->height) {
							sp->width = vp->width;
							sp->height = vp->height;
						}
						
						for (i = 0; i < sp->sub.num_rects; i++) {
							AVSubtitleRect *sub_rect = sp->sub.rects[i];

							sub_rect->x = av_clip(sub_rect->x, 0, sp->width);
							sub_rect->y = av_clip(sub_rect->y, 0, sp->height);
							sub_rect->w = av_clip(sub_rect->w, 0, sp->width - sub_rect->x);
							sub_rect->h = av_clip(sub_rect->h, 0, sp->height - sub_rect->y);

							is->sub_convert_ctx = sws_getCachedContext(is->sub_convert_ctx,
								sub_rect->w, sub_rect->h, AV_PIX_FMT_PAL8,
								sub_rect->w, sub_rect->h, AV_PIX_FMT_BGRA,
								0, NULL, NULL, NULL);
							if (!is->sub_convert_ctx) {
								av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
								return;
							}
							sws_scale(is->sub_convert_ctx, (const uint8_t * const *)sub_rect->data, sub_rect->linesize,
								0, sub_rect->h, pixels, pitch);
						}
						sp->uploaded = 1;
					}
				}
				else
					sp = NULL;
			}
		}

		if (!vp->uploaded) {
			if (set_texture_buf(vp->frame, &is->img_convert_ctx) < 0)
				return;
			vp->uploaded = 1;
			vp->flip_v = vp->frame->linesize[0] < 0;
		}
	}

	//设置图像数据
	int MediaState::set_texture_buf(AVFrame *frame, struct SwsContext **img_convert_ctx)
	{
		*img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
			frame->width, frame->height, (AVPixelFormat)frame->format, displayFrame->width, displayFrame->height,
			(AVPixelFormat)displayFrame->format, SWS_BILINEAR, NULL, NULL, NULL);
		if (*img_convert_ctx != NULL)
		{
			sws_scale(*img_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize,
				0, frame->height, displayFrame->data, displayFrame->linesize);

			if (!is->width)
			{
				is->width = displayFrame->width;
				is->height = displayFrame->height;
			}

			if (_bgra == nullptr)
				_bgra = (uint8_t *)malloc(displayFrame->width * displayFrame->height * 4 * sizeof(uint8_t));

			memset(_bgra, 0, displayFrame->width * displayFrame->height * 4 * sizeof(uint8_t));

			libyuv::I420ToRGBA(displayFrame->data[0],
				displayFrame->linesize[0],
				displayFrame->data[1],
				displayFrame->linesize[1],
				displayFrame->data[2],
				displayFrame->linesize[2],
				_bgra,
				displayFrame->width * 4,
				displayFrame->width,
				displayFrame->height);

			if (_rgba == nullptr)
				_rgba = (uint8_t *)malloc(displayFrame->width * displayFrame->height * 4 * sizeof(uint8_t));

			memset(_rgba, 0, displayFrame->width * displayFrame->height * 4 * sizeof(uint8_t));

			libyuv::ARGBToBGRA(_bgra,
				displayFrame->width * 4,
				_rgba,
				displayFrame->width * 4,
				displayFrame->width,
				displayFrame->height);

			libyuv::ARGBToI420(_rgba,
				displayFrame->width * 4,
				displayFrame->data[0],
				displayFrame->linesize[0],
				displayFrame->data[1],
				displayFrame->linesize[1],
				displayFrame->data[2],
				displayFrame->linesize[2],
				displayFrame->width,
				displayFrame->height);
			return 0;
		}

		return -1;
	}

	uint8_t *MediaState::getData()
	{
		return _rgba;
	}

	unsigned int MediaState::getWidth()
	{
		return displayFrame->width;
	}

	unsigned int MediaState::getHeight()
	{
		return displayFrame->height;
	}

	/******************************************************************************************************************************************************************/
	int MediaState::decoder_start(Decoder *d, int(*fn)(void *), void *arg)
	{
		_packetQueue->packet_queue_start(d->queue);
		d->decoder_tid = SDL_CreateThread(fn, "decoder", arg);
		if (!d->decoder_tid) {
			av_log(NULL, AV_LOG_ERROR, "SDL_CreateThread(): %s\n", SDL_GetError());
			return AVERROR(ENOMEM);
		}
		return 0;
	}

	int MediaState::audio_thread(void *arg)
	{
		MediaState *self = (MediaState *)arg;
		VideoState *is = self->is;
		AVFrame *frame = av_frame_alloc();
		Frame *af;

		int got_frame = 0;
		AVRational tb;
		int ret = 0;

		if (!frame)
			return AVERROR(ENOMEM);

		do {
			// 解码音频帧帧
			if ((got_frame = self->decoder_decode_frame(&is->auddec, frame, NULL)) < 0)
				goto the_end;

			if (got_frame) {
				tb = { 1, frame->sample_rate };
				// 检查是否帧队列是否可写入，如果不可写入，则直接释放
				if (!(af = self->_frameQueue->frame_queue_peek_writable(&is->sampq)))
					goto the_end;
				// 设定帧的pts
				af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
				af->pos = frame->pkt_pos;
				af->serial = is->auddec.pkt_serial;
				af->duration = av_q2d({ frame->nb_samples, frame->sample_rate });
				// 将解码后的音频帧压入解码后的音频队列
				av_frame_move_ref(af->frame, frame);
				self->_frameQueue->frame_queue_push(&is->sampq);

			}
		} while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
	the_end:
		av_frame_free(&frame);
		return ret;
	}

	int MediaState::video_thread(void *arg)
	{
		MediaState *self = (MediaState *)arg;
		VideoState *is = self->is;
		AVFrame *frame = av_frame_alloc();
		double pts;
		double duration;
		int ret;
		AVRational tb = is->video_st->time_base;
		// 猜测视频帧率
		AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);


		if (!frame) {
			return AVERROR(ENOMEM);
		}

		for (;;) {
			// 获得视频解码帧，如果失败，则直接释放，如果没有视频帧，则继续等待
			ret = self->get_video_frame(frame);
			if (ret < 0)
				goto the_end;
			if (!ret)
				continue;

			duration = (frame_rate.num && frame_rate.den ? av_q2d({ frame_rate.den, frame_rate.num }) : 0);
			// 计算帧的pts、duration等
			pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
			// 放入到已解码队列
			ret = self->queue_picture(frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial);
			av_frame_unref(frame);

			if (ret < 0)
				goto the_end;
		}
	the_end:
		av_frame_free(&frame);
		return 0;
	}

	int MediaState::subtitle_thread(void *arg)
	{
		MediaState *self = (MediaState *)arg;
		VideoState *is = self->is;
		Frame *sp;
		int got_subtitle;
		double pts;

		for (;;) {
			// 查询队列是否可写
			if (!(sp = self->_frameQueue->frame_queue_peek_writable(&is->subpq)))
				return 0;
			// 解码字幕帧
			if ((got_subtitle = self->decoder_decode_frame(&is->subdec, NULL, &sp->sub)) < 0)
				break;

			pts = 0;
			// 如果存在字幕
			if (got_subtitle && sp->sub.format == 0) {
				if (sp->sub.pts != AV_NOPTS_VALUE)
					pts = sp->sub.pts / (double)AV_TIME_BASE;
				sp->pts = pts;
				sp->serial = is->subdec.pkt_serial;
				sp->width = is->subdec.avctx->width;
				sp->height = is->subdec.avctx->height;
				sp->uploaded = 0;
				// 将解码后的字幕帧压入解码后的字幕队列
				/* now we can update the picture count */
				self->_frameQueue->frame_queue_push(&is->subpq);
			}
			else if (got_subtitle) {
				// 释放字幕
				avsubtitle_free(&sp->sub);
			}
		}
		return 0;
	}

	AVDictionary **MediaState::setup_find_stream_info_opts(AVFormatContext *s,
		AVDictionary *codec_opts)
	{
		int i;
		AVDictionary **opts;

		if (!s->nb_streams)
			return NULL;
		opts = (AVDictionary **)av_mallocz_array(s->nb_streams, sizeof(*opts));
		if (!opts) {
			av_log(NULL, AV_LOG_ERROR,
				"Could not alloc memory for stream options.\n");
			return NULL;
		}
		for (i = 0; i < s->nb_streams; i++)
			opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codecpar->codec_id,
				s, s->streams[i], NULL);
		return opts;
	}

	AVDictionary *MediaState::filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
		AVFormatContext *s, AVStream *st, AVCodec *codec)
	{
		AVDictionary    *ret = NULL;
		AVDictionaryEntry *t = NULL;
		int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
			: AV_OPT_FLAG_DECODING_PARAM;
		char          prefix = 0;
		const AVClass    *cc = avcodec_get_class();

		if (!codec)
			codec = s->oformat ? avcodec_find_encoder(codec_id)
			: avcodec_find_decoder(codec_id);
		if (!codec)
			return NULL;

		switch (codec->type) {
		case AVMEDIA_TYPE_VIDEO:
			prefix = 'v';
			flags |= AV_OPT_FLAG_VIDEO_PARAM;
			break;
		case AVMEDIA_TYPE_AUDIO:
			prefix = 'a';
			flags |= AV_OPT_FLAG_AUDIO_PARAM;
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			prefix = 's';
			flags |= AV_OPT_FLAG_SUBTITLE_PARAM;
			break;
		default:
			break;
		}

		while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
			char *p = strchr(t->key, ':');

			/* check stream specification in opt name */
			if (p)
				switch (check_stream_specifier(s, st, p + 1)) {
				case  1: *p = 0; break;
				case  0:         continue;
				default:         return NULL;
				}

			if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
				(codec && codec->priv_class &&
					av_opt_find(&codec->priv_class, t->key, NULL, flags,
						AV_OPT_SEARCH_FAKE_OBJ)))
				av_dict_set(&ret, t->key, t->value, 0);
			else if (t->key[0] == prefix &&
				av_opt_find(&cc, t->key + 1, NULL, flags,
					AV_OPT_SEARCH_FAKE_OBJ))
				av_dict_set(&ret, t->key + 1, t->value, 0);

			if (p)
				*p = ':';
		}
		return ret;
	}

	int MediaState::check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
	{
		int ret = avformat_match_stream_specifier(s, st, spec);
		if (ret < 0)
			av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
		return ret;
	}

	/***********************************************************************************************************/

	FFMPEG_EVENT MediaState::event_loop()
	{
		if (_event == FF_HOLD)
		{
			return _event;
		}

		SDL_Event event;

		int ret = refresh_loop_wait_event(&event);
		
		if (ret != 1)
		{
			return FF_NONE;
		}

		if (!is->width) 
			return FF_NONE;

		

		Uint32 type = event.type - _uniqueID;

		switch (type) {
		case FF_PAUSE_EVENT:
			toggle_pause();
			_event = FF_PAUSE;
			break;
		case FF_MUTE_EVENT:
			toggle_mute();
			_event = FF_MUTE;
			break;
		case FF_OVER_EVENT:
			_event = FF_OVER;
			//do_exit();
			break;
		case SDL_QUIT:
		case FF_QUIT_EVENT:
			_event = FF_QUIT;
			do_exit();
			break;
		case FF_SEEK_EVENT:
		{
			double seconds = *((double*)event.user.data1);
// 			double dst = seconds > _duration ? _duration : seconds;
// 			double pos = get_master_clock();
// 			double seek = dst - pos;
// 			stream_seek((int64_t)(dst * AV_TIME_BASE), (int64_t)(seek * AV_TIME_BASE), 0);

			double pos = get_master_clock();
			pos += seconds;
			stream_seek((int64_t)(pos * AV_TIME_BASE), (int64_t)(seconds * AV_TIME_BASE), 0);

			delete (double*)event.user.data1;
			_event = FF_PLAY;
		}
			break;
		default:
			_event = FF_PLAY;
		}

		return _event;
	}
}