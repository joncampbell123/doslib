
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <hw/cpu/cpu.h>

#ifdef __cplusplus
}
#endif

int main() {
    printf("Hello world\n");

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	return 0;
}

