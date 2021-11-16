#include <iostream>
#include "include/frame_queue.h"
#include "include/webm_encoder.h"
#include "include/semaphore.h"

Semaphore mux;

void CallBack(void *arg)
{
	mux.Signal();
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cout << "Need input file" << std::endl;
		exit(-1);
	}
	WebmMediaEncoder *rgba_to_webm = new WebmMediaEncoder(); //新建编码器
	rgba_to_webm->set_dst_file("output.webm");				 //设置输出文件名
	rgba_to_webm->set_resolution(1280, 720);				 //设置分辨率
	rgba_to_webm->set_frame_rate(30);						 //设置视频帧率
	rgba_to_webm->setOnEncodeFinish(CallBack, nullptr);		 //设置编码完成事件回调函数
	rgba_to_webm->set_log_interval(2000);					 //设定编码时打印日志间隔
	rgba_to_webm->set_log_switch(true);						 //设定编码器log开关
	rgba_to_webm->Start();									 //启动编码器

	FrameQueue *frame_queue = new FrameQueue(20, 1280 * 720 * 4, rgba_to_webm);
	frame_queue->Start();

	FILE *fd = fopen(argv[1], "rb");
	int ret;
	uint8_t *buff = new uint8_t[1280 * 720 * 4];

	while (ret = fread(buff, 1, 1280 * 720 * 4, fd)) //每次进行一帧的读取
	{
		usleep(50 * (5 + rand() % 122));			  //模拟渲染器正在渲染一帧
		frame_queue->PushFrame((uint8_t *)buff, ret); //将一帧推送到帧队列
	}
	frame_queue->EndEncode();
	mux.Wait();

	return 0;
}