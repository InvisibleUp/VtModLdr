This is an admittedly weird testing setup, but it works?

INTRO:

This folder is full of unit tests. Each unit test is dead simple, as they
should be. They often call on "prototypes" in /src/include/test that
simplify things a lot.

Each test case returns 0 on success and 1 on failure. This is the OPPOSITE
of what a "x == y" equality gives you, so be careful.

STRUCTURE:

Each test case belongs to a certain numbered group. Each test case in a
group is executed. Only if all of them pass does the next group get executed.

This allows for the ability to have test cases that rely on other tests
having succeded. For instance, group 0 is a "sanity check" group that makes
sure basic functions such as "asprintf", "crc32File" and the program init
sequence are functioning.

After that we can test simple mod operations, then advanced mod operations,
etc. etc. without worrying about them failing due to a complete and total lack
of support.

USING:

Just use CTest!

Input:

	srmodldr.exe [testname]

	testname - Optional. Can refer to the name of a test or the number of a group.
        	   Executes whatever it can.

Output (for non-CTest users):

	[PASS/FAIL] testname (X.XXX s)

	[PASS]/[FAIL] - Indicates if test passed or failed
	testname - name of test
	(X.XXX s) - Number of seconds test took to execute


GROUPS:

00 - Sanity check. (Ensures all core testing functions are working)
01 - Nondependent (Ensures sanity of most other components
02... As groups up, dependency increases.