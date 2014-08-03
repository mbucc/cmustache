struct test {
	char		*jsondata;
	char		*template;
	char		*expected;
	char		*description;
};

typedef vec_t(struct test *) test_vec_t;

test_vec_t	get_tests(const char *spec_file);