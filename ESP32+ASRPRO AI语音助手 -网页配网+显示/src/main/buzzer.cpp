#include "buzzer.h"

#define buzzer 33 // 蜂鸣器控制引脚

void buzzer_on(void)
{

    digitalWrite(buzzer, HIGH);
}

void buzzer_off(void)
{

    digitalWrite(buzzer, LOW);
}

void buzzer_init(void)
{

    pinMode(buzzer, OUTPUT);
    buzzer_on_off();
}

void buzzer_on_off(void)
{

    buzzer_on();
    delay(200);
    buzzer_off();
}
