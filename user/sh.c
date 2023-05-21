#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()\""

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
	static int in_quot = 0;
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		in_quot = 0;
		return 0;
	}

	if (strchr(SYMBOLS, *s)) {
		if (*s == '\"') {
			in_quot = !in_quot;
		}
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	*p1 = s;
	while (*s && ((in_quot && !strchr("\"", *s)) || !strchr(WHITESPACE SYMBOLS, *s))) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe, int *leftenv) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit();
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit();
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			/* Exercise 6.5: Your code here. (1/3) */
			fd = open(t, O_RDONLY);
			dup(fd, 0);
			close(fd);

//			user_panic("< redirection not implemented");

			break;
		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// Open 't' for writing, dup it onto fd 1, and then close the original fd.
			/* Exercise 6.5: Your code here. (2/3) */
			fd = open(t, O_WRONLY);
			dup(fd, 1);
			close(fd);

//			user_panic("> redirection not implemented");

			break;
		case '|':;
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			int p[2];
			/* Exercise 6.5: Your code here. (3/3) */
			pipe(p);
			*rightpipe = fork();
			if (*rightpipe == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe, leftenv);
			}  else if (*rightpipe > 0) {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}

//			user_panic("| not implemented");

			break;
		case ';':
			debugf("clg: use ;\n");
			*leftenv = fork();
			if (*leftenv == 0) {
				return argc;
			} else if (*leftenv > 0) {
				return parsecmd(argv, rightpipe, leftenv);
			}
			break;
		case '&':
			debugf("clg: use &\n");
			int child = fork();
			if (child == 0) {
				return argc;
			} else if (child > 0) {
				return parsecmd(argv, rightpipe, leftenv);
			}
			break;
		case '\"':
			debugf("clg: use \"\n");
			gettoken(0, &t);
			argv[argc++] = t;
			if (gettoken(0, &t) == 0) {
				return argc;
			}
			break;
		}
	}

	return argc;
}

void runcmd(char *s) {
	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	int leftenv = 0;
	int argc = parsecmd(argv, &rightpipe, &leftenv);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	if (leftenv) {
		wait(leftenv);
	}
	
	debugf("clg: spawn %s...\n", argv[0]);
	int child = spawn(argv[0], argv);
	close_all();
	if (child >= 0) {
		wait(child);
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit();
}

int insert_char(char *buf, int i, char ch) {
	int len = strlen(buf);
	if (len >= MAXPATHLEN) {
		return -1;
	}
	for (int j = len; j > i; j--) {
		buf[j] = buf[j - 1];
	}
	buf[i] = ch;

	len++;
	for (int j = i + 1; j < len; j++) {
		printf("%c", buf[j]);
	}
	for (int j = i + 1; j < len; j++) {
		printf("\b");
	}
	
	return 0;
}

void remove_char(char *buf, int i) {
	if (i < 0) {
		return;
	}
	for (int j = i; buf[j]; j++) {
		buf[j] = buf[j + 1];
	}

	printf("\b");
	for (int j = i; buf[j]; j++) {
		printf("%c", buf[j]);
	}
	printf(" \b");
	for (int j = i; buf[j]; j++) {
		printf("\b");
	}
}

void readline(char *buf, u_int n) {
	memset(buf, 0, 1024);
	int r;
	int cursor = 0;
	int ch;
	while (1) {	
		if ((r = read(0, &ch, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit();
		}
		if (ch == '\b' || ch == 0x7f) {
			if (cursor > 0) {
				remove_char(buf, cursor - 1);
				cursor--;
			}
		} else if (ch == '\r' || ch == '\n') {
			return;
		} else if (ch == 0x1b) {
			read(0, &ch, 1); // read [
			read(0, &ch, 1);
			if (ch == 'A')
				debugf("clg: get up key\n");
			else if (ch == 'B')
				debugf("clg: get down key\n");
			else if (ch == 'C') {
				if (cursor < strlen(buf)) {
					cursor++;
				} else {
					printf("\b");
				}
			}
			else if (ch == 'D') {
				if (cursor > 0) {
					cursor--;
				} else {
					printf(" ");
				}
			}
		} else {
			insert_char(buf, cursor, ch);
			cursor++;
		}

	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	debugf("usage: sh [-dix] [command-file]\n");
	exit();
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	debugf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	debugf("::                                                         ::\n");
	debugf("::                     MOS Shell 2023                      ::\n");
	debugf("::                                                         ::\n");
	debugf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[1], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[1], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			runcmd(buf);
			exit();
		} else {
			wait(r);
		}
	}
	return 0;
}
