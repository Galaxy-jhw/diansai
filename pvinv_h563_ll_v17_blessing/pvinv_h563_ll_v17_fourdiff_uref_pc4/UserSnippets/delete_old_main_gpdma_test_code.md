# Must delete old GPDMA test code in main.c

Delete any loop code that repeatedly reconfigures GPDMA channels, for example:

```c
LL_DMA_DisableChannel(GPDMA1,LL_DMA_CHANNEL_1);
LL_DMA_SetSrcAddress(GPDMA1,LL_DMA_CHANNEL_1,(uint32_t)TXTEST);
LL_DMA_SetBlkDataLength(GPDMA1,LL_DMA_CHANNEL_1,10);
LL_DMA_EnableChannel(GPDMA1,LL_DMA_CHANNEL_1);
```

Reason: ADC1/ADC2 need stable GPDMA circular transfers. Reconfiguring GPDMA in while(1) can break UART DMA and/or ADC DMA.

Final while(1) should only do low-speed tasks: display, serial print, key scan, state monitoring.
