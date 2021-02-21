
void hello() {
}

__asm__ (".global startup\nstartup: l.j hello");

