# Example:
# (from the parent directory, with valgrind)
# make clean all UNDERSCORE=y && make test TEST_HARNESS="valgrind --leak-check=full -v"
# (from this directory, with gdb)
# make TEST_HARNESS="gdb --args"
all:
	$(TEST_HARNESS) ../buildsys test.lua --clean
	sha256sum -c result.txt
