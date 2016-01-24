#define FUSE_USE_VERSION 30

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <fuse.h>
#include <ctype.h>
#include <math.h>

/* Helper functions */
/* pathType takes a string representing the path and the index of the function
 * we are comparing then returns a value describing the path as defined by the
 * enum.
 * mf_invalid: invalid file
 * mf_dir: directory
 * mf_file: valid file 
 * mf_doc: documentation file */
enum PathResults {mf_invalid, mf_dir, mf_doc, mf_file};
enum PathResults pathType(const char *path, int i);

/* isNum takes string s and returns 1 if s is a properly-formatted number:
 * Can start with a '+' or '-' or '.' or number
 * Can have one or zero decimal points
 * Can have only numeric characters otherwise
 * Returns 0 if these conditions are not met */
int isNum(const char *s);

/* printint takes a value and adds it as a new line to results_buf */
void printint(unsigned int val);

/* Math functions */
/* Prototypes for operations */
void bi_factor(double a, double b);
void bi_fib(double a, double b);
void bi_add(double a, double b);
void bi_sub(double a, double b);
void bi_mul(double a, double b);
void bi_div(double a, double b);
void bi_exp(double a, double b);

/* Results can't have more than these many characters */
#define tempbufsize 30

/* Results can't have more than these many lines */
#define maxlines 120

#define maxbufsize (tempbufsize * maxlines)

/* Buffer for results */
char results_buf[maxbufsize];

/* Maximum factor value */
const unsigned int maxfactor = 1000000000;

/* Last used function & arguments.
 * If they are different, run function again.
 * If they are all equal, then just reread from results. */
void (*last_f)(double, double);
double last_a;
double last_b;

/* Error strings */
static const char overflow_error[] = "Error: Overflow\n";
static const char divzero_error[] = "Error: Divide by zero\n";
static const char factor_nonint_error[] = "Error: can only factor on integers\n";
static const char factor_toolarge_error[] = "Error: can only factor up to %i\n";
static const char fib_error[] = "Error: can only count fibonacci for positive integers\n";

/* Struct for functions */
static const char docdir[] = "/doc";
const int max_builtins = 7;
struct bi_command
{
	char *path;
	int nops;
	void (*f)(double, double);
	char *doc;
} builtins[] =
{
	{"/factor", 1, bi_factor, "Prime factorization.\nThe file factor/a contains all the prime factors of a.\n"},
	{"/fib", 1, bi_fib, "Fibonacci Sequence.\nThe file fib/a contains the fibonacci sequence from 1 to a.\n"},
	{"/add", 2, bi_add, "Addition.\nThe file add/a/b contains a+b.\n"},
	{"/sub", 2, bi_sub, "Subtraction.\nThe file sub/a/b contains a-b.\n"},
	{"/mul", 2, bi_mul, "Multiplication.\nThe file mul/a/b contains a*b.\n"},
	{"/div", 2, bi_div, "Division.\nThe file div/a/b contains a/b.\n"},
	{"/exp", 2, bi_exp, "Exponent\nThe file exp/a/b contains a^b.\n"}
};

// FUSE function implementations.
static int mathfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	if(strcmp(path, "/") == 0)
	{
		/* root directory */
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else
	{
		/* search builtins table */
		double a = 0, b = 0;
		char *num2;
		int i;
		for(i = 0; i < max_builtins && strncmp(path, builtins[i].path, strlen(builtins[i].path)) != 0; i++);
		if(i < max_builtins)
		{
			enum PathResults result = pathType(path, i);
			switch(result)
			{
				case mf_dir:
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					break;
				case mf_doc:
					stbuf->st_mode = S_IFREG | 0444;
					stbuf->st_nlink = 1;
					stbuf->st_size = strlen(builtins[i].doc);
					break;
				case mf_file:
					stbuf->st_mode = S_IFREG | 0444;
					stbuf->st_nlink = 1;
					a = strtod(path + strlen(builtins[i].path) + 1, &num2);
					if(builtins[i].nops == 2)
						b = strtod(num2 + 1, 0);
					if((last_f != builtins[i].f) || a != last_a || b != last_b)
					{
						/* Function or arguments have changed, run new function */
						builtins[i].f(a, b);
						last_f = builtins[i].f;
						last_a = a;
						last_b = b;
					}
					stbuf->st_size = strlen(results_buf);
					break;
				default:
					res = -ENOENT;
			}
		}
		else
		{
			res = -ENOENT;
		}
	}
	
	return res;
}

static int mathfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") == 0)
	{
		/* Root directory */
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		/* Fill in directory contents from table */
		int i;
		for(i = 0; i < max_builtins; i++)
		{
			//printf("Filling %s\n", builtins[i].path + 1);
			filler(buf, builtins[i].path + 1, NULL, 0);
		}
	}
	else
	{
		/* search builtins table */
		int i;
		for(i = 0; i < max_builtins && strcmp(path, builtins[i].path) != 0; i++);
		if(i < max_builtins)
		{
			/* Fill in doc file */
			filler(buf, docdir + 1, NULL, 0);
		}
		else
		{
			return -ENOENT;
		}
	}

	return 0;
}		

static int mathfs_open(const char *path, struct fuse_file_info *fi)
{
	int i;
	for(i = 0; i < max_builtins && strncmp(path, builtins[i].path, strlen(builtins[i].path)) != 0; i++);
	if(i < max_builtins)
	{
		enum PathResults result = pathType(path, i);
		switch(result)
		{
			case mf_file:
				if ((fi->flags & 3) != O_RDONLY)
					return -EACCES;
				else
					return 0;
				break;
			case mf_doc:
				if ((fi->flags & 3) != O_RDONLY)
					return -EACCES;
				else
					return 0;
				break;
			default:
				return -ENOENT;
		}
	}
	else
		return -ENOENT;
}

static int mathfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	int i;
	for(i = 0; i < max_builtins && strncmp(path, builtins[i].path, strlen(builtins[i].path)) != 0; i++);
	if(i < max_builtins)
	{
		size_t len;
		double a = 0, b = 0;
		char *num2;
		enum PathResults result = pathType(path, i);
		switch(result)
		{
			case mf_doc:
				len = strlen(builtins[i].doc);
				if (offset < len)
				{
					if (offset + size > len)
						size = len - offset;
					memcpy(buf, builtins[i].doc + offset, size);
				}
				else
					size = 0;
				break;
			case mf_file:
				a = strtod(path + strlen(builtins[i].path) + 1, &num2);
				if(builtins[i].nops == 2)
				{
					b = strtod(num2 + 1, 0);
				}
				if((last_f != builtins[i].f) || a != last_a || b != last_b)
				{
					/* Function or arguments have changed, run new function */
					builtins[i].f(a, b);
					last_f = builtins[i].f;
					last_a = a;
					last_b = b;
				}
				len = strlen(results_buf);

				if (offset < len)
				{
					if (offset + size > len)
						size = len - offset;
					memcpy(buf, results_buf + offset, size);
				}
				else
					size = 0;

				break;
			default:
				return -ENOENT;
		}
		return size;
	}
	else
		return -ENOENT;
}

static struct fuse_operations mathfs_oper = {
	.getattr = mathfs_getattr,
	.readdir = mathfs_readdir,
	.open = mathfs_open,
	.read = mathfs_read,
};

int main(int argc, char **argv)
{
	return fuse_main(argc, argv, &mathfs_oper, NULL);
}

void bi_factor(double a, double b)
{
	double intpart;
	double fractpart = modf(a, &intpart);
	if(fractpart != 0)
	{
		/* Non-integer number entered */
		strcpy(results_buf, factor_nonint_error);
	}
	else if(intpart > maxfactor)
	{
		/* Too large number entered */
		snprintf(results_buf, tempbufsize * maxlines, factor_toolarge_error, maxfactor);
	}
	else
	{
		/* Clear results buffer */
		memset(results_buf, 0, 1);

		/* Find factors iteratively. */
		/* Stupid algorithm: just keep incrementing divisor by 1 until it divides evenly */
		/* Stupid because it will divide by nonprimes */
		double i = intpart;
		double div = 2;
		double temp = i / div;
		fractpart = modf(temp, &intpart);
		while(div < i)
		{
			if(fractpart != 0)
			{
				div++;
			}
			else
			{
				printint(div);
				i /= div;
				div = 2;
			}
			temp = i / div;
			fractpart = modf(temp, &intpart);
		}
		printint(i);
	}

}

void bi_fib(double a, double b)
{
	double intpart;
	double fractpart = modf(a, &intpart);
	if(fractpart != 0 || intpart < 0)
	{
		/* Invalid number entered */
		strcpy(results_buf, fib_error);
	}
	else
	{
		/* Initialize start of fibonacci sequence */
		strcpy(results_buf, "1\n");
		/* Calculate fibonacci sequence iteratively */
		unsigned int prev1 = 1, prev2 = 0, result = 0;
		int i;
		for(i = 1; i < intpart; i++)
		{
			result = prev1 + prev2;
			if(result < prev1)
			{
				/* integer overflow */
				strcat(results_buf, overflow_error);
				break;
			}
			else
			{
				printint(result);
				prev2 = prev1;
				prev1 = result;
			}
		}
	}
}

void bi_add(double a, double b)
{
	double result = a + b;
	snprintf(results_buf, tempbufsize * maxlines, "%g\n", result);
}

void bi_sub(double a, double b)
{
	double result = a - b;
	snprintf(results_buf, tempbufsize * maxlines, "%g\n", result);
}

void bi_mul(double a, double b)
{
	double result = a * b;
	snprintf(results_buf, tempbufsize * maxlines, "%g\n", result);
}

void bi_div(double a, double b)
{
	char temp[tempbufsize];
	if(b == 0)
	{
		strcpy(results_buf, divzero_error);
	}
	else
	{
		double result = a / b;
		snprintf(results_buf, tempbufsize * maxlines, "%g\n", result);
	}
}

void bi_exp(double a, double b)
{
	double result = pow(a, b);
	snprintf(results_buf, tempbufsize * maxlines, "%g\n", result);
}

void printint(unsigned int val)
{
	char temp[tempbufsize];
	int res = snprintf(temp, tempbufsize, "%u\n", val);
	if(res > 0 && res < tempbufsize)
	{
		/* string correctly written */
		strcat(results_buf, temp);
	}
	else
	{
		strcpy(results_buf, overflow_error);
	}
}

int isNum(const char *s)
{
	/* First character must be a '-' or '+' or '.' or digit */
	if(s[0] != '-' && s[0] != '+' && s[0] != '.' && !isdigit(s[0]))
	{
		return 0;
	}
	else
	{
		int dp = s[0] == '.';
		int i;
		for(i = 1; s[i] != 0; i++)
		{
			if(s[i] != '.' && !isdigit(s[i]))
				/* nonnumeric character */
				return 0;
			if(s[i] == '.')
				dp++;
		}
		if(dp <= 1)
			/* one or zero decimal points, valid number */
			return 1;
		else
			/* too many decmial points */
			return 0;
	}
}


enum PathResults pathType(const char *path, int i)
{
	if(strcmp(path, builtins[i].path) == 0)
	{
		/* This is the directory itself */
		return mf_dir;
	}
	else if(strcmp(path + strlen(builtins[i].path), docdir) == 0)
	{
		/* This is the doc file */
		return mf_doc;
	}
	else if(builtins[i].nops == 1)
	{
		/* One operand, this is a "file" */
		if(isNum(path + strlen(builtins[i].path) + 1))
		{
			return mf_file;
		}
		else
		{
			/* Non-numeric value entered, no such "file" */
			return mf_invalid;
		}
	}
	else if(strchr(path + strlen(builtins[i].path) + 1, '/') != 0)
	{
		/* This is the second operand, so it is a "file" */
		if(isNum(strchr(path + strlen(builtins[i].path) + 1, '/') + 1))
		{
			return mf_file;
		}
		else
		{
			/* Non-numeric value entered, no such "file" */
			return mf_invalid;
		}
	}
	else
	{
		/* This is the first operand, so it is a "subdirectory" */
		if(isNum(path + strlen(builtins[i].path) + 1))
		{
			return mf_dir;
		}
		else
		{
			/* Non-numeric value entered, no such "file" */
			return mf_invalid;
		}
	}
}
