#include "mock_hal.h"
#include "board_diagnostics.h"
#include "i2c_scanner.h"
#include <assert.h>
#include <stdio.h>
GPIO_TypeDef port_a,port_b,port_c;
I2C_HandleTypeDef hi2c1;
static uint32_t ms, outputs, input_pins;
static unsigned sensor_calls,lcd_calls,toggles;
uint32_t HAL_GetTick(void){return ms;}
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g){assert(p==&port_b);if(g->Mode==GPIO_MODE_INPUT){outputs&=~g->Pin;input_pins=g->Pin;}else {
 outputs|=g->Pin;
 p->ODR = (p->ODR | (p->BSRR & 0xFFFF)) & ~(p->BSRR >> 16);
 p->IDR = p->ODR;
 }}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *p,uint16_t n){(void)p;return n==BTN_1_Pin?GPIO_PIN_RESET:GPIO_PIN_SET;}
void HAL_GPIO_TogglePin(GPIO_TypeDef *p,uint16_t n){assert(p==&port_a&&n==LD2_Pin);toggles++;}
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *p){(void)p;return HAL_I2C_ERROR_AF;}
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*p,uint16_t a,uint32_t n,uint32_t t){assert(p==&hi2c1&&n==1&&t==2);return a==0x70||a==0x4E?HAL_OK:HAL_ERROR;}
void dht20_init(void){}
void lcd_init(void){}
void dht20_service(void){assert(g_i2c_scan.complete);sensor_calls++;}
void lcd_service(void){assert(g_i2c_scan.complete);lcd_calls++;}
int main(void){
 board_diagnostics_init();assert(outputs==0&&input_pins==(TL1_A_Pin|TL1_B_Pin|TL2_A_Pin|TL2_B_Pin));
 for(int i=0;i<112;i++){ms+=10;board_diagnostics_service();}assert(g_i2c_scan.complete&&g_i2c_scan.device_count==2);assert(sensor_calls==0&&lcd_calls==0);
 ms+=200;board_diagnostics_service();assert(sensor_calls==1&&lcd_calls==1&&toggles>=1);assert(g_board_diag.raw_buttons[0]==0&&g_board_diag.raw_buttons[1]==1);
 for(unsigned road=1;road<=2;road++){for(unsigned code=0;code<4;code++){
  g_board_diag.road=road;g_board_diag.code=code;g_board_diag.command=2;board_diagnostics_service();
  unsigned a=road==1?TL1_A_Pin:TL2_A_Pin,b=road==1?TL1_B_Pin:TL2_B_Pin;
  unsigned set=((code&2)?a:0)|((code&1)?b:0);
  assert(outputs==(a|b)&&port_b.BSRR==(set|(((a|b)&~set)<<16)));assert(g_board_diag.command==0&&g_board_diag.result==0);
 }}
 unsigned count=g_board_diag.applied_count;
 g_board_diag.road=1;g_board_diag.code=0;g_board_diag.command=2;board_diagnostics_service();
 assert(g_board_diag.applied_count==count+1&&g_board_diag.pin_code==0&&g_board_diag.result==0);
 for(unsigned code=1;code<4;code++){
  g_board_diag.code=code;board_diagnostics_service();
  assert(g_board_diag.applied_code==code&&g_board_diag.pin_code==code&&g_board_diag.result==0);
 }
 count=g_board_diag.applied_count;board_diagnostics_service();assert(g_board_diag.applied_count==count);
 unsigned old_outputs=outputs;g_board_diag.road=3;g_board_diag.command=2;board_diagnostics_service();assert(g_board_diag.result==1&&outputs==old_outputs);
 g_board_diag.command=3;board_diagnostics_service();assert(outputs==0);
 g_board_diag.command=1;board_diagnostics_service();assert(!g_i2c_scan.complete&&g_i2c_scan.next_address==9);
 puts("PASS diagnostics: input-only boot, scan exclusion, raw buttons, all requested A/B codes, command validation and output release");
}
