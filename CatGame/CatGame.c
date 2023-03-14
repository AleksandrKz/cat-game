/*
 * CatGame.c
 *
 * Created: 22.10.2018 20:43:52
 *  Author: Aleks
 */ 

#define motorSpeed 12 // скорость серв (чем меньше значение, тем быстрее).
#define stepX 130 //если новые координаты по оси Х дальше на stepX шагов от старых, то двигаем лазер медленее.
#define stepY 130 //если новые координаты по оси Y дальше на stepY шагов от старых, то двигаем лазер медленее.
#define auto 1 // 1-стартовать автоматически, 0-не стартовать

//стартовые координаты для автоматической работы 650...2400
#define xMIN 1500
#define xMAX 2350
#define yMIN 700
#define yMAX 2350

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

int8_t buffer[12] = {0,};
volatile uint8_t count = 0, i = 0, flag = 0, automatik, a = 0, b = 0;
volatile uint16_t x,y,tmp,tmp_x,tmp_y,xc,yc,xa,xb,ya,yb;

void LowMotion(void); // замедляем движение сервоприводов
void delay_ms(uint16_t);

//UART
#define baudrate 9600L
#define bauddivider (F_CPU/(16*baudrate)-1)
#define HI(x) ((x)>>8)
#define LO(x) ((x) & 0xFF)

#define FRAMING_ERROR (1<<FE)
#define PARITY_ERROR (1<<UPE)
#define DATA_OVERRUN (1<<DOR)

ISR(USART_RX_vect)
{
	int8_t status, data;
	status = UCSRA;
	data = UDR;
	if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN)) == 0) //проверка на целостность данных
	{
		buffer[count] = data;
		count++;
		if(data == '_') // если приняли координаты, выставляем флаг для парсинга
		{
			flag = 1;
		}
		else if (data == 'L') // включаем/выключаем лазер
		{
			PORTB ^= _BV(PB2);
			count--;
		}
		else if (data == 'R') // выставляем флаг автоматического режима работы
		{
			automatik ^= 1;
			count--;
		}
		else if (data == 'A')  // сохраняем координаты начала игрового поля х у (ближняя точка к х=0(700) у=0(700)) 
		{
			xa = x;
			ya = y;
			count--;
		}
		else if (data == 'B') // сохраняем координаты конца игрового поля х у (ближняя точка к х=мах(2400) у=мах(2400))
		{
			xb = x;
			yb = y;
			count--;
		}
		else
		{
			count--;
		}
		if(count > 11) // если буфер приема переполнен то обнуляем индекс и сам буфер
		{
			count = 0;
			//memset(buffer,0,12);
		}			
	}
	else
	{
		UCSRA = 0;
	}
}

int main(void)
{
	DDRB |= (1<<PB4)|(1<<PB3)|(1<<PB2);
	
	//PWM
	ICR1 = 20000;// Hz
	TCCR1A |= (1<<COM1A1)|(1<<COM1B1)|(1<<WGM11);
	TCCR1B |= (1<<WGM13)|(1<<WGM12)|(1<<CS11);
	OCR1A = 1500; //600-2400
	OCR1B = 1500;
	//------
	//UART
	UBRRH = HI(bauddivider);
	UBRRL = LO(bauddivider);
	UCSRA = 0;
	UCSRB |= (1<<RXCIE)|(1<<RXEN);
	UCSRC |= (1<<UCSZ1)|(1<<UCSZ0);
	//------
	sei();
	
	PORTB |= _BV(PB2); // вкл. лазер/
	//стартовые координаты для автоматической работы
	xa = xMIN;
	xb = xMAX;
	ya = yMIN;
	yb = yMAX;
	automatik = auto;// автоматичекая работа при подаче питания
	
    while(1)
    {
        if(flag) // парсим координаты и записываем в х и у, а так же в серву(ручной режим управления)
        {
			x=0;y=0;
			cli();
			i =	1;
			while(buffer[i] != 'Y')
			{
				x *= 10;
				x += buffer[i] - '0';
				i++;
			}
			i++;
			while(buffer[i] != '_')
			{
				y *= 10;
				y += buffer[i] - '0';
				i++;
			}
			count = 0;
			memset(buffer,0,12);
			flag = 0;
			sei();
			
			if(x >= 650 && x <= 2400)
				OCR1A = x;
			if(y >= 650 && y <= 2400)
				OCR1B = y;
        }
		
		while(automatik) // автоматический режим работы
		{
			srand(TCNT1);
			tmp_x =  xa+rand()%(xb-xa); //рандом от ха до хь
			tmp_y =  ya+rand()%(yb-ya);
			//tmp = tmp_x >= tmp_y ? tmp_x : tmp_y;
			if(OCR1A >= tmp_x)
				xc = OCR1A - tmp_x;
			else
				xc =  tmp_x - OCR1A;
				
			if(OCR1B >= tmp_y)
				yc = OCR1B - tmp_y;
			else
				yc =  tmp_y - OCR1B;
				
			if (xc > stepX || yc > stepY) //если новые координаты дальше stepX stepY  шагов от старых, то двигаем лазер медленее.
			{
				tmp = 1000;
				a = 1; b = 1; 
				while(a != 0 && b != 0)
					LowMotion();
			} 
			else // иначе двигаем лазер быстро.
			{
				tmp = 700;
				OCR1A = tmp_x;
				OCR1B = tmp_y;
			}
			tmp = 500+rand()%tmp;
			delay_ms(tmp); // период между новыми координатами от 500мс до 1500мс.
		}		
	}
}

void LowMotion()
{
	if(OCR1A > tmp_x)
		OCR1A -=1;
	else if(OCR1A < tmp_x)
		OCR1A +=1;
	else
		a=0;
		
	if(OCR1B > tmp_y)
		OCR1B -=1;
	else if(OCR1B < tmp_y)
		OCR1B +=1;
	else
		b=0;
		
	_delay_ms(motorSpeed); //скорость серв
}

void delay_ms(uint16_t milliseconds)
{
   while(milliseconds > 0)
   {
	   _delay_ms(1);
	   milliseconds--;
   }
}