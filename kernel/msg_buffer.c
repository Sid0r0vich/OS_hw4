#include "msg_buffer.h"
#include "defs.h"
#include "stat.h"
#include "printf.h"
#include <stdarg.h>

msgbuff msgbuf;

static int flag = 0;

void msgbufinit() {
	acquire(&msgbuf.lock);
	msgbuf.begin = msgbuf.end = 0;
	release(&msgbuf.lock);
}

void putc(char c) {
	msgbuf.buf[msgbuf.end++] = c;
	msgbuf.end %= MSGBUFLEN;
	if (msgbuf.end < 0) msgbuf.end += MSGBUFLEN;
	if (msgbuf.end == (msgbuf.begin + 1) % MSGBUFLEN && flag)
		msgbuf.begin = msgbuf.end;
	else ++flag;
}

void msgadd(char* msg) {
	char* c = msg;

	acquire(&msgbuf.lock);
	do {
		putc(*c);
	} while(*(c++));
	int lstc = msgbuf.end - 1;
	if (lstc < 0) lstc += MSGBUFLEN;
	msgbuf.buf[lstc] = '\n';
	release(&msgbuf.lock);
}

static void
bprintint(int xx, int base, int sign) {
  	char buf[16];
  	int i;
  	uint x;

  	if(sign && (sign = xx < 0))
    	x = -xx;
  	else
    	x = xx;

  	i = 0;
  	do {
    	buf[i++] = digits[x % base];
  	} while((x /= base) != 0);

  	if(sign)
    	buf[i++] = '-';

  	while(--i >= 0)
    	putc(buf[i]);
}

static void
bprintptr(uint64 x) {
  	int i;
  	putc('0');
 	putc('x');
  	for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
		putc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

void pr_msg(const char* fmt, ...) {
  	if (fmt == 0)
    	panic("null fmt");
    	
    va_list args;
	
	char fmtticks[ strlen(fmt) + 7];
	char* tmp = "[%d] ";
	safestrcpy(fmtticks, tmp, 6);
	safestrcpy(fmtticks + 5, fmt, strlen(fmt) + 1);
	int len = strlen(fmtticks);
	fmtticks[len] = '\n';
	fmtticks[len + 1] = '\0';
	//fmt	printf("dmesg! beg: %d\n", msgbuf.begin);

    acquire(&tickslock);
	int time = ticks;
	release(&tickslock);

	int i, c, locking;
	int targ = 1;
  	char *s;

  	locking = pr.locking;
  	if(locking)
    	acquire(&pr.lock);
  	acquire(&msgbuf.lock);
  	
  	va_start(args, fmt);
  	for(i = 0; (c = fmtticks[i] & 0xff) != 0; i++) {
    	if(c != '%') {
      		putc(c);
      		consputc(c);
      		continue;
    	}
	    c = fmtticks[++i] & 0xff;
	    if(c == 0)
	      	break;
	    switch(c) {
		    case 'd':
		    	int argd;
		    	if (targ) {
		    		targ = 0;
		    		argd = time;
		    	} else argd = va_arg(args, int);
		    	printint(argd, 10, 1);
		      	bprintint(argd, 10, 1);
		      	break;
		    case 'x':
		    	int argx = va_arg(args, int);
		    	printint(argx, 16, 1);
		      	bprintint(argx, 16, 1);
		      	break;
		    case 'p':
		    	uint64 argp = va_arg(args, uint64);
		    	printptr(argp);
		      	bprintptr(argp);
		      	break;
		    case 's':
		    	char* argstr = va_arg(args, char*);
		      	if((s = argstr) == 0)
		        	s = "(null)";
		      	for(; *s; s++) {
		        	putc(*s);
		        	consputc(*s);
		        }
		      	break;
		    case '%':
		      	putc('%');
		      	consputc('%');
		      	break;
		    default:
		      	putc('%');
		      	consputc('%');
		      	putc(c);
		      	consputc(c);
		      	break;
		}
 	}
 	va_end(args);
 	
  	release(&msgbuf.lock);
  	if(locking)
    	release(&pr.lock);
}

uint64 sys_dmesg(void) {
	uint64 buf;
	argaddr(0, &buf);

	struct proc *p = myproc();
	acquire(&msgbuf.lock);
	if (copyout(p->pagetable, buf, msgbuf.buf + msgbuf.begin, MSGBUFLEN - msgbuf.begin) < 0)
		return -1;
	if (copyout(p->pagetable, buf + MSGBUFLEN - msgbuf.begin, msgbuf.buf, msgbuf.begin) < 0) 
		return -1;
	release(&msgbuf.lock);

	// printf("char %d: %d\n", msgbuf.begin, msgbuf.buf[msgbuf.begin]);
	// for (int i = msgbuf.begin + 1; i != msgbuf.end; i = (i + 1) % MSGBUFLEN) {
		// printf("char %d: %d\n", i, msgbuf.buf[i]);
	// }

	return 0;
}
