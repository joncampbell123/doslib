
void hello() {
}

int hello2(int a,int b) {
    return a+b;
}

__asm__ (".global startup\nstartup: l.j hello");

