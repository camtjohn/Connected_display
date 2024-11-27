#ifndef ADC_TOOLS
#define ADC_TOOLS

//ADC1 Channels
#define ADC_CH              ADC_CHANNEL_6       //Pin34
#define ADC_ATTEN           ADC_ATTEN_DB_11

void adc_init(void);
int read_adc(void);
void adc_teardown(void);

#endif