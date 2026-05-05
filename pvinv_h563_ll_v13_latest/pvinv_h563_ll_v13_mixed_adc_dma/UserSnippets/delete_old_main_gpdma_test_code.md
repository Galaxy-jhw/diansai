# 必须删除旧main.c中的GPDMA测试代码

你的旧main.c在while(1)中反复操作GPDMA1 Channel1，这会干扰真实ADC DMA和LPUART DMA。最终工程中必须删除。

## 删除这一段

```c
// LL_mDelay(1);

LL_DMA_DisableChannel(GPDMA1,LL_DMA_CHANNEL_1);
LL_DMA_SetSrcAddress(GPDMA1,LL_DMA_CHANNEL_1,(uint32_t)TXTEST);
LL_DMA_SetBlkDataLength(GPDMA1,LL_DMA_CHANNEL_1,10);
LL_DMA_EnableChannel(GPDMA1,LL_DMA_CHANNEL_1);

LL_mDelay(0);
DMASADD=LL_DMA_GetSrcAddress(GPDMA1,LL_DMA_CHANNEL_0);
DMADADD=LL_DMA_GetDestAddress(GPDMA1,LL_DMA_CHANNEL_0);
DMAL=LL_DMA_GetBlkDataLength(GPDMA1,LL_DMA_CHANNEL_0);
DMAEN=LL_DMA_IsEnabledChannel(GPDMA1,LL_DMA_CHANNEL_0);

// LL_SPI_Enable(SPI1);
// LL_SPI_TransmitData8(SPI1,0x55);
// LL_SPI_StartMasterTransfer(SPI1);
// LL_mDelay(1);
```

## 替换成这一段

```c
const PVINV_Handle_t *p = PVINV_LL_GetHandle();
(void)p;

/* 低速任务放这里：OLED/串口/按键/状态查看。
 * 不要在while(1)中改GPDMA寄存器。
 */
```

## 原因

- ADC1/ADC2的DMA需要稳定、连续、循环地把ADC结果搬到`g_pvinv_adc1_raw[]`和`g_pvinv_adc2_raw[]`。
- 旧测试代码在主循环里反复Disable/Enable GPDMA通道，会破坏DMA连续性。
- 你的LPUART工程已经使用了GPDMA1 Channel0/1，ADC DMA应使用CubeMX另分配的新通道，不应在主循环里抢占Channel0/1。
