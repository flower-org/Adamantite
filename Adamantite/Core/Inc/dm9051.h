#ifndef __DM9051_H
#define __DM9051_H

#include "main.h"

void DM9051_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);

#endif /* __DM9051_H */
