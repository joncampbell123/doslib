
static void pvga1a_unlock(void) {
	outpw(0x3CE,0x050F); // register 0x0F = 0x05 to unlock extensions
}

