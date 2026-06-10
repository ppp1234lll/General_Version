#include "bsp_adc.h"
#include "bsp.h"

// 定义存储ADC转换结果的数组
// DMA会自动将数据搬运到此数组
uint16_t adc_value[2] = {0};

/*!
 * \brief 配置ADC使用的GPIO引脚
 *        PA6 -> ADC Channel 6
 *        PC0 -> ADC Channel 10
 */
void adc_gpio_config(void)
{
    // 使能GPIO时钟
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    
    // 配置为模拟输入模式
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0);
}

/*!
 * \brief 配置DMA，用于自动搬运ADC多通道数据
 *        使用DMA0通道0（ADC0专用通道）
 */
void adc_dma_config(void)
{
    dma_single_data_parameter_struct dma_init_struct;  // 修正：使用正确的结构体类型
    
    rcu_periph_clock_enable(RCU_DMA1);  // ADC0使用DMA1
    
    dma_deinit(DMA1, DMA_CH0);
    
    dma_single_data_para_struct_init(&dma_init_struct);  // 初始化结构体
    
    // 外设地址：ADC0的数据寄存器地址
    dma_init_struct.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));
    // 内存地址：指向定义的数组
    dma_init_struct.memory0_addr = (uint32_t)adc_value;
    // 方向：外设 -> 内存
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    // 传输个数：2个通道
    dma_init_struct.number = 2;
    // 外设地址固定
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    // 内存地址递增
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    // 数据宽度：16位（ADC数据是16位的）
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    // 循环模式：使能
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    // 优先级：高
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    
    dma_single_data_mode_init(DMA1, DMA_CH0, &dma_init_struct);  // 单数据模式初始化
    
    // 选择外设请求：ADC0对应的是 DMA_SUBPERI0（需根据实际库确定）
    dma_channel_subperipheral_select(DMA1, DMA_CH0, DMA_SUBPERI0);
    
    dma_channel_enable(DMA1, DMA_CH0);
}

/*!
 * \brief ADC整体配置
 *        - 使用ADC0
 *        - 规则组多通道（Channel 6 和 Channel 10）
 *        - 扫描+连续模式
 *        - 软件触发
 */
void adc_config(void)
{
    // 1. 复位ADC并开启时钟
    adc_deinit();
    rcu_periph_clock_enable(RCU_ADC0);
    
    // 2. 配置ADC时钟（建议HCLK/4或/8，使ADC时钟不高于30MHz）
    //    假设系统HCLK=200MHz，8分频=25MHz，在安全范围内
    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);
    
    // 3. 配置GPIO
    adc_gpio_config();
    
    // 4. 配置DMA（必须在ADC之前初始化）
    adc_dma_config();
    
    // 5. ADC工作模式配置
    //    独立模式（不使用双ADC同步）
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    //    数据右对齐
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    //    使能扫描模式（多通道必须开启）
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    //    使能连续转换模式（持续不断采样）
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    
    // 6. 配置规则组转换序列长度 = 2
    adc_channel_length_config(ADC0, ADC_ROUTINE_CHANNEL, 2);
    
    // 7. 配置规则组转换顺序:
    //    第0个转换: ADC通道6 (PA6), 采样时间 55.5 周期
    adc_routine_channel_config(ADC0, 0, ADC_CHANNEL_6, ADC_SAMPLETIME_144);
    //    第1个转换: ADC通道10 (PC0), 采样时间 55.5 周期
    adc_routine_channel_config(ADC0, 1, ADC_CHANNEL_10, ADC_SAMPLETIME_144);
    
    // 8. 触发源配置
    //    关闭外部触发，使用软件触发
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, DISABLE);
    //    或者如果需要定时器触发，可以使用下面的代码（取消注释）
    //    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T0_CH0);
    //    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);
    
    // 9. 使能ADC的DMA请求
    adc_dma_mode_enable(ADC0);
    
    // 10. 使能ADC并进行校准
    adc_enable(ADC0);
    delay_ms(1);               // 等待稳定
    adc_calibration_enable(ADC0);
    
    // 11. 软件触发启动转换
    adc_software_trigger_enable(ADC0, ADC_ROUTINE_CHANNEL);
}

/*!
 * \brief 根据通道序号获取实际电压（mV）
 * \param channel_index: 0 -> PA6, 1 -> PC0
 * \return 电压值，单位mV
 */
float get_voltage_v(uint8_t channel_index)
{
    uint16_t adc_value_raw = 0;
    uint32_t voltage_v = 0;
    
    if(channel_index < 2) {
        adc_value_raw = adc_value[channel_index];
        // 假设参考电压为 3300mV (3.3V)
        // 公式: Voltage = (ADC_Value * Vref) / 4095
        voltage_v = adc_value_raw * 3.3 * 1000 / 4095;
    }
    
    return voltage_v;
}

int adc_test(void)
{
    // 系统时钟配置...
    
    // 初始化ADC
 //   adc_config();
    
    while(1) {
        // 延迟100ms读取一次（不需要轮询标志位，DMA已自动更新数组）
        delay_1ms(1000);
        
        // 获取PA6引脚的电压
        uint32_t vol_pa6 = get_voltage_v(0);
        // 获取PC0引脚的电压
        uint32_t vol_pc0 = get_voltage_v(1);
			
			printf("%d V,%d V\r\n",vol_pa6,vol_pc0);
        
        // 使用串口打印或进行其他处理...
    }
}
