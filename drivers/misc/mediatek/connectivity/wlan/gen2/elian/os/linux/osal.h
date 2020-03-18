
#ifndef __MSC_OSAL_H__
#define __MSC_OSAL_H__

#include <linux/string.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>

#include "gl_typedef.h"

#define ELIAN_DBG_ERROR      1
#define ELIAN_DBG_WARN       2
#define ELIAN_DBG_DEBUG      3

extern unsigned int ElianDebugLevel;

#define DBGPRINT(Level, Fmt)		\
do {								\
	if(Level <= ElianDebugLevel)	\
	printk Fmt;						\
} while (0)

#define MSC_DBG printk

#define MAX_THREAD_NAME_LEN 16

#if 0
typedef void (*P_TIMEOUT_HANDLER)(unsigned long);
typedef int (*thread_func)(unsigned long);

typedef struct _osal_timer
{
	struct timer_list timer;
	P_TIMEOUT_HANDLER timeoutHandler;
	unsigned long data;
}osal_timer, *p_osal_timer;

typedef struct _osal_thread
{
	struct task_struct *thread;
	void *thread_func;
	void *data;
	char name[MAX_THREAD_NAME_LEN];
}osal_thread, *p_osal_thread;

extern void* osal_memset(void *buf, int i, unsigned int len);
extern void* osal_memcpy(void *dst, const void *src, unsigned int len);
extern int osal_memcmp(const void *buf1, const void *buf2, unsigned int len);
extern int osal_timer_modify(p_osal_timer ptimer, unsigned ms);
extern int osal_timer_stop(p_osal_timer ptimer);
extern int osal_timer_start(p_osal_timer ptimer, unsigned ms);
extern int osal_timer_create(p_osal_timer ptimer);
extern int osal_thread_create(p_osal_thread thread);
extern int osal_thread_run (p_osal_thread thread);
extern int osal_thread_stop (p_osal_thread thread);
extern int osal_thread_should_stop (p_osal_thread thread);
extern int osal_msleep(unsigned int ms);
extern unsigned int  osal_strlen(const char *str);
extern int osal_strcmp(const char *dst, const char *src);
extern int osal_strncmp(const char *dst, const char *src, unsigned int len);
extern char * osal_strcpy(char *dst, const char *src);
extern long int osal_strtol(const char *str, char **c, int adecimal);
extern char *osal_strstr(const char *haystack, const char *needle);
#endif


typedef void (*P_TIMEOUT_HANDLER) (unsigned long);

typedef struct _OSAL_TIMER_ {
	struct timer_list timer;
	P_TIMEOUT_HANDLER timeoutHandler;
	unsigned long timeroutHandlerData;
} OSAL_TIMER, *P_OSAL_TIMER;

typedef struct _OSAL_THREAD_ {
	struct task_struct *pThread;
	PVOID pThreadFunc;
	PVOID pThreadData;
	char threadName[MAX_THREAD_NAME_LEN];
} OSAL_THREAD, *P_OSAL_THREAD;

PVOID osal_memset(PVOID buf, int i, unsigned int len);
PVOID osal_memcpy(PVOID dst, const PVOID src, unsigned int len);
int osal_memcmp(const PVOID buf1, const PVOID buf2, unsigned int len);
int osal_timer_modify(P_OSAL_TIMER, unsigned int);
int osal_timer_stop(P_OSAL_TIMER);
int osal_timer_start(P_OSAL_TIMER, unsigned int);
int osal_timer_create(P_OSAL_TIMER);
int osal_thread_create(P_OSAL_THREAD);
int osal_thread_run(P_OSAL_THREAD);
int osal_thread_stop(P_OSAL_THREAD);
int osal_thread_should_stop(P_OSAL_THREAD);
int osal_sleep_ms(unsigned int ms);
unsigned int osal_strlen(const char *str);
int osal_strcmp(const char *dst, const char *src);
int osal_strncmp(const char *dst, const char *src, unsigned int len);
char *osal_strcpy(char *dst, const char *src);
int osal_strtol(const char *str, unsigned int adecimal, long *res);
char *osal_strstr(char *str1, const char *str2);

extern int osal_lock_init(spinlock_t *l);
extern int osal_lock(spinlock_t *l);
extern int osal_unlock(spinlock_t *l);
#endif
