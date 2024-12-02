#include "SPEAK_SET.h"
#include <Arduino.h>

#define Speak_set 13  // 喇叭选择控制引脚

void Spk_set_ASR (void)
{

    digitalWrite(Speak_set, HIGH);
}

void Spk_set_98357 (void)
{

    digitalWrite(Speak_set,LOW);
}

void Spk_init (void)
{

    pinMode(Speak_set, OUTPUT);
    Spk_set_ASR();
}