
int hello2(int a,int b);
int hello3(int a,int b,int c);
int hello3if(int a,int b,int c);

void hello() {
    int x;

    x = hello2(4,5);
    *((volatile int*)0xA0000) = x;

    x = hello3(1,2,3);
    *((volatile int*)0xA0004) = x;

    x = hello3if(1,-2,-3);
    *((volatile int*)0xA0008) = x;
}

int hello2(int a,int b) {
    return a+b;
}

int hello3if(int a,int b,int c) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (c == 3) c++;
    return a+b+c;
}

__asm__ (".global startup\nstartup: l.j hello\nl.nop"); /* NTS: l.j with nop for delay slot */

