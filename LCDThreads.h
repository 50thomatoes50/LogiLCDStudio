/*******************************************
A Docile Sloth 2016 (adocilesloth@gmail.com)
*******************************************/
#ifndef LCDTHREADS_H
#define LCDTHREADS_H

#include "LogitechLCDLib.h"
#include "obs.h"
#include "DataFunctions.h"

//#include <string>
#include <sstream>
#include <atomic>
#include <iomanip>
#include <ctime>
#include <thread>

void Mono(std::atomic<bool>&);
void Colour(std::atomic<bool>&);
void Dual(std::atomic<bool>&);

#define do_log(level, format, ...) \
	blog(level, "[LogiLCD] " format, ##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define dbg(format, ...)  do_log(LOG_DEBUG,    format, ##__VA_ARGS__)

#endif //LCDTHREADS_H