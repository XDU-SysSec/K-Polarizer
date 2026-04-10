#include "kp_header.h"



void kp_pr_info(const char *fmt, ...)
{
#ifdef KP_DEBUG
    char string[0x100];
    va_list args;
    va_start(args, fmt);
    vsprintf(string, fmt, args);
    pr_info("%s", string); // can't use pr_info(string); here
    va_end(args);
#endif
}

void kp_pr_alert(const char *fmt, ...)
{
#ifdef KP_DEBUG
    char string[0x100];
    va_list args;
    va_start(args, fmt);
    vsprintf(string, fmt, args);
    pr_alert("%s", string);
    va_end(args);
    #endif
}

void kp_pr_err(const char *fmt, ...)
{
#ifdef KP_DEBUG
    char string[0x100];
    va_list args;
    va_start(args, fmt);
    vsprintf(string, fmt, args);
    pr_err("%s", string);
    va_end(args);
#endif
}