#ifndef ERROR_H_INCLUDED
#define ERROR_H_INCLUDED

#ifndef COMMONS 
#include "commons.h"
#endif

void error_handler(const char *func, char *file, int line, char *msg);

#endif
