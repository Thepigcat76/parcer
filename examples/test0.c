const static int x, *y, **z[], test();

typedef const int const i32;

static int test_func() {
  const static int t[] = {[0] = 200, [1] = 300,};
  return 99;
}

void test_func0(void (*set_data)(int)) {
  int (*const volatile (*const fn_ptr_ptr))(int);
  (*fn_ptr_ptr)(0);
  set_data(100);
}
