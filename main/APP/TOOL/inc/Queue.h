#ifndef _Queue_H
#define _Queue_H

#include "sys.h"

#define QUEUE_BUF_SIZE	4096

//循环队列存储结构  
 typedef struct queue  
{  
	u8 buf[QUEUE_BUF_SIZE];//存储队列元素  CanRxMsgTypeDef
	int len;//队列大小
	int front;//队列头指针  
	int rear;//队列尾指针  
	int count;//队列元素个数  
}queue_s;  
/**/


void Init_Queue(queue_s *queue);

int Judge_Queue_Empty(queue_s *queue);

int Judge_Queue_Full(queue_s *queue);

int Enqueue_One_Byte(queue_s *queue, u8 data);

int Dequeue_One_Byte(queue_s *queue);

u8 Get_Data_Size_Upto_Symbol(queue_s *queue,u16 symbol) ;

void Clear_Queue(queue_s *queue );

int Get_Queue_Count(queue_s *queue);

int Judge_Dequeue_Count_Enough(queue_s *queue,int size);

int Judge_Enqueue_Count_Enough(queue_s *queue,int size);

int Enqueue_Bytes_From_Buffer(u8 *buf, queue_s *queue,int size) ;

int Dequeue_Bytes_To_Buffer(queue_s *queue,u8 *buf,int size);

int Read_Bytes_To_Buffer(queue_s *queue,u8 *buf,int size);

int Dequeue_Bytes(queue_s *queue,int size);

#endif

