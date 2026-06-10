#include "error.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"

// 电、网、传感器故障
ErrorFault_t sg_err_status = {0};

/*
*********************************************************************************************************
*	函 数 名: Error_Set
*	功能说明: 标记错误发生
*	形    参: 
* @param item_idx: 错误索引
*	返 回 值: 无
*********************************************************************************************************
*/
void Error_Set(uint32_t item_idx)
{
	// 检查错误是否已存在
	for (uint8_t i = 0; i < sg_err_status.fault_count; i++)
	{
		if (sg_err_status.fault_index[i] == item_idx)
		{
			return; // 错误已存在，无需重复添加
		}
	}
	
	// 添加新错误
	if (sg_err_status.fault_count < 32)
	{
		sg_err_status.fault_index[sg_err_status.fault_count] = item_idx;
		sg_err_status.fault_count++;
	}
}

/*
*********************************************************************************************************
*	函 数 名: Error_Clear
*	功能说明: 清除指定错误
*	形    参: 
* @param item_idx: 错误索引
*	返 回 值: 无
*********************************************************************************************************
*/
void Error_Clear(uint32_t item_idx)
{
	// 查找并清除错误
	for (uint8_t i = 0; i < sg_err_status.fault_count; i++)
	{
		if (sg_err_status.fault_index[i] == item_idx)
		{
			// 移除错误（将最后一个错误移到当前位置）
			sg_err_status.fault_index[i] = sg_err_status.fault_index[sg_err_status.fault_count - 1];
			sg_err_status.fault_count--;
			break;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: Error_Check
*	功能说明: 查询错误是否发生
*	形    参: 
* @param group: 错误组
* @param item_idx: 组内错误索引
*	返 回 值: 1-发生，0-未发生
*********************************************************************************************************
*/
uint8_t Error_Check(uint32_t item_idx)
{
	// 查找错误是否存在
	for (uint8_t i = 0; i < sg_err_status.fault_count; i++)
	{
		if (sg_err_status.fault_index[i] == item_idx)
		{
			return 1; // 错误存在
		}
	}
	
	return 0; // 错误不存在
}

/*
*********************************************************************************************************
*	函 数 名: Error_GetAllCodes
*	功能说明: 遍历所有错误，返回错误码数组
*	形    参: 
 * @param codes: 输出错误码数组
*	返 回 值: 错误数量；-1=参数无效
*********************************************************************************************************
*/
int8_t Error_GetAllCodes(uint32_t* codes)
{
	if (codes == NULL) return -1;
	
	for (uint8_t i = 0; i < sg_err_status.fault_count; i++) 
	{
		codes[i] = sg_err_status.fault_index[i];
	}

	return (sg_err_status.fault_count > 127) ? 127 : (int8_t)sg_err_status.fault_count;
}

/*
*********************************************************************************************************
*	函 数 名: Error_Get_Codesbuf
*	功能说明: 获取所有错误码拼接字符串
*	形    参: 
* @param codes: 错误码数组首地址
* @param max_len: codes 缓冲区的最大长度（包含 '\0'）
*	返 回 值: -1-失败；其他-成功读取的个数
*********************************************************************************************************
*/
int8_t Error_Get_Codesbuf(uint8_t* codes, uint16_t max_len)
{
	if (codes == NULL || max_len == 0) return -1;
	
	uint16_t err_count = sg_err_status.fault_count;
	uint8_t buf[16] = {0};
	uint16_t current_len = 0;

	// 生成格式：ERR=数量,故障码1,故障码2;
	// 写入前缀和数量
	snprintf((char*)buf, sizeof(buf), "ERR=%u", (unsigned)err_count);
	strcat((char*)codes, (char*)buf);
	current_len += strlen((char*)buf);
	
	// 写入故障码
	if (err_count > 0)
	{
		for (uint8_t i = 0; i < err_count; i++) 
		{
			if (current_len + 10 >= max_len)
			{
				goto finish; // 超过最大长度，提前结束拼接
			}

			// 将故障码转换为字符串并添加逗号分隔符
			snprintf((char*)buf, sizeof(buf), ",%08X", sg_err_status.fault_index[i]);
			strcat((char*)codes, (char*)buf);
			current_len += strlen((char*)buf);
		}
	}
	
finish:
	// 添加分号结尾
	if (current_len + 2 <= max_len)
	{
		strcat((char*)codes, ";");
	}
	
	return (err_count > 127) ? 127 : (int8_t)err_count;
}


/*
*********************************************************************************************************
*	函 数 名: Error_GetCount
*	功能说明: 查询错误数量
*	形    参: 
* @param group: 错误组
*	返 回 值: 发生错误数量
*********************************************************************************************************
*/
uint8_t Error_GetCount(void)
{
	return sg_err_status.fault_count;
}
