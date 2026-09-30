#pragma once
#include <stdio.h>
#define ESP_LOGI(tag, format, ...) printf("%s: " format "\n", tag, ##__VA_ARGS__)
