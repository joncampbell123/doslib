
void func() {
	int vals = { 0, 1, 2, 3, 4, 9, 10, 19, 100, 255, 256, 1000, 0x0, 0x1, 0x3, 0x7, 0xF, 0x10, 0x1F, 0xFF, 0xFFF, 01, 06, 07, 010, 'a', 'c', '\t', '\n', '\\', '\?', '\"', '\'', '\x01', '\xff', '\0', '\007', L'\xffff', u'\xffff', U'\xffff', 1u, 1l, 1ll, 1ull, 1llu, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFll, -1, -127, -128, -129, -255, -256, -257, -0x7FFF, -0x8000, -0x8001, -0x7FFFFFFF, -0x80000000, -0x80000001, +1, +5, +127, +128, +129, +130, ~0, ~1, ~0u, !0, !1, !-1 };
	const char *strs = { "Hello", "World""How", "Are" "You" };
	const char *strs = { u8"Hello", u8"World""How", u8"Are" "You" };
	const short *strs = { u"Hello", u"World""How", u"Are" "You" };
	const long *strs = { U"Hello", U"World""How", U"Are" "You" };
	const long *strs = { L"Hello", L"World""How", L"Are" "You" };
}

