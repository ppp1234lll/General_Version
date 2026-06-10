#include <queue.h>
#include <string.h>

/************************************************* 
函数功能:初始化循环队列
传入参数:queue循环队列
传出参数:无
返回值:无
*************************************************/  
void Init_Queue(queue_s *queue)
{
	memset(queue->buf,0,QUEUE_BUF_SIZE);
	queue->len = QUEUE_BUF_SIZE;
	queue->front = 0;  
	queue->rear  = 0;  
	queue->count = 0;  
}  

 /*
判断队列为空和满  
1、使用计数器count,队列为空和满时，front都等于rear  
2、少用一个元素的空间，约定队列满时：(rear+1)%len=front,为空时front=rear  
rear指向队尾元素的下一个位置，始终为空；队列的长度为(rear-front+len)%len  
*/  

/************************************************* 
函数功能:判断队列为空
传入参数:queue循环队列
传出参数:无
返回值:ST_TRUE队列为空
			ST_FALSE队列不为空
*************************************************/  
int Judge_Queue_Empty(queue_s *queue)  
{  
	if(queue->count == 0)  
		return 0;  
	else  
		return -1;  
}  
  
/************************************************* 
函数功能:判断队列已满
传入参数:queue循环队列
传出参数:无
返回值:ST_TRUE队列已满
			ST_FALSE队列未满
*************************************************/  
int Judge_Queue_Full(queue_s *queue)  
{  
	if(queue->count == queue->len)  
		return 0;  
	else  
		return -1;  
}  
  
/************************************************* 
函数功能:入队一个字节
传入参数:queue循环队列
				data要入队的数据
传出参数:无
返回值:ST_ERROR入队失败
			ST_EOK入队成功
*************************************************/  
int Enqueue_One_Byte(queue_s *queue, u8 data)  
{  
	//验证队列是否已满  
	if(0 == Judge_Queue_Full(queue))  
	{  
		//printf("The queue is full\r\n");
		return -1;  
	}  
	//入队  
	queue->buf[queue->rear] = data;  
	//对尾指针后移  
	queue->rear = (queue->rear + 1) % (queue->len);  
	//更新队列长度  
	queue->count++;  
	return 0;  
}  
  
/************************************************* 
函数功能:出队一个字节
传入参数:queue循环队列
传出参数:无
返回值:data出队的数据
*************************************************/  
int Dequeue_One_Byte(queue_s *queue)  
{
	u8 data;
	//判断队列是否为空  
	if(0 == Judge_Queue_Empty(queue))  
	{  
		//printf("The queue is empty!\r\n");  
		return -1;  
	}  
	//保存返回值  
	data = queue->buf[queue->front];  

	//更新队头指针  
	queue->front = (queue->front + 1) % (queue->len);  
	//更新队列长度  
	queue->count--;  

	return data;  
}  

/************************************************* 
函数功能:判断第一个数据到下一个指定
				数据之间的数据个数
				(包涵下个指定数据)
传入参数:queue循环队列
				symbol指定的数据
传出参数:无
返回值:数据个数
*************************************************/  
u8 Get_Data_Size_Upto_Symbol(queue_s *queue,u16 symbol)  
{
	u16 data;
	u8 size = 2;
	int adrH = 0,adrL = 0;
	int i;

	adrH = queue->front;
	adrL = (queue->front + 1) % (queue->len); 
	data = (queue->buf[adrH] <<8)|(queue->buf[adrL]);  
	if(data == symbol)
	{
		return size;
	}
	else
	{
		for(i = 0;i < queue->count;i++)
		{
			size += 1;
			adrH = (adrH + 1) % (queue->len); 
			adrL = (adrL + 1) % (queue->len); 
			data = (queue->buf[adrH] <<8)|(queue->buf[adrL]);  
			if(data == symbol)
			{
				return size;
			}
		}
		return 0;
	}
	
}  

 
/************************************************* 
函数功能:清空队列
传入参数:queue循环队列
传出参数:无
返回值:无
*************************************************/  
void Clear_Queue(queue_s *queue )  
{  
	queue->front = 0;
	queue->rear = 0;  
	queue->count = 0;  
}  
  
/************************************************* 
函数功能:获取队列中数据长度
传入参数:queue循环队列
传出参数:无
返回值:queue->count队列中数据长度
*************************************************/  
int Get_Queue_Count(queue_s *queue)  
{  
	//printf("Get_Queue_Count %d\r\n",queue->count);

	return queue->count;  
}  

/************************************************* 
函数功能:判断队列是否有足够元素可出队
传入参数:queue循环队列
				size期望出队数量
传出参数:无
返回值:ST_TRUE队列中有足够数量元素可出队
			ST_FALSE队列可出队数量不够
*************************************************/  
int Judge_Dequeue_Count_Enough(queue_s *queue,int size)  
{  
	if(queue->count >= size)  
		return 0;  
	else  
		return -1;  
}  
  
/************************************************* 
函数功能:判断队列是否有足够空间可入队
传入参数:queue循环队列
				size期望入队数量
传出参数:无
返回值:ST_TRUE队列中有足够空间可入队
			ST_FALSE队列空间不够
*************************************************/  
int Judge_Enqueue_Count_Enough(queue_s *queue,int size)  
{  
	if((queue->len - queue->count) >= size)  
		return 0;  
	else  
		return -1;  
}  


/************************************************* 
函数功能:入队多个字节
传入参数:buf要入队的数据
				queue循环队列
				size期望入队数量
传出参数:无
返回值:ST_ERROR入队失败
			ST_EOK入队成功
*************************************************/  
int Enqueue_Bytes_From_Buffer(u8 *buf, queue_s *queue,int size)  
{
	int i;

	if(0 == Judge_Enqueue_Count_Enough(queue,size))
	{
		for(i = 0;i < size;i++)
		{
			Enqueue_One_Byte(queue,buf[i]);
		}
		return 0;  
	}
	else
	{
//		printf("The queue have not enough space!\r\n");
		return -1;  
	}
}  

/************************************************* 
函数功能:出队多个字节
传入参数:queue循环队列
				buf接收出队元素
				size期望出队数量
传出参数:无
返回值:ST_ERROR出队失败
			ST_EOK出队成功
*************************************************/  
int Dequeue_Bytes_To_Buffer(queue_s *queue,u8 *buf,int size)  
{
	int i;

	if(0 == Judge_Dequeue_Count_Enough(queue,size))
	{
		for(i = 0;i < size;i++)
		{
			buf[i] = Dequeue_One_Byte(queue);
		}
		return 0;  
	}
	else
	{
//		printf("The queue have not enough count data!\r\n");
		return -1;  
	}
}  

/************************************************* 
函数功能:读取多个字节但不出队
传入参数:queue循环队列
				buf接收出队元素
				size期望出队数量
传出参数:无
返回值:ST_ERROR出队失败
			ST_EOK出队成功
*************************************************/  
int Read_Bytes_To_Buffer(queue_s *queue,u8 *buf,int size)  
{
	int i;
	int front;

	if(0 == Judge_Dequeue_Count_Enough(queue,size))
	{
		front = queue->front;
		for(i = 0;i < size;i++)
		{
			buf[i] = queue->buf[front];  
			front = (front + 1) % (queue->len);  
		}
		return 0;  
	}
	else
	{
//		printf("The queue have not enough count data!\r\n");
		return -1;  
	}
}  

/************************************************* 
函数功能:出队多个字节
传入参数:queue循环队列
				size期望出队数量
传出参数:无
返回值:ST_ERROR出队失败
			ST_EOK出队成功
*************************************************/  
int Dequeue_Bytes(queue_s *queue,int size)  
{
	int i;

	if(0 == Judge_Dequeue_Count_Enough(queue,size))
	{
		for(i = 0;i < size;i++)
		{
			Dequeue_One_Byte(queue);
		}
		return 0;  
	}
	else
	{
//		printf("The queue have not enough count data!\r\n");
		return -1;  
	}
}  




