#include <Windows.h>
#include "vld.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "te.h"

static char expression[] = 
"\
(define square-root (lambda (x)\n\
    (define (abs-value x) (if (< x 0) (- x) x))\n\
    (define square (lambda (x) (* x x)))\n\
    (define average (lambda (a b) (/ (+ a b) 2)))\n\
    (define improve (lambda (guess) (average guess (/ x guess))))\n\
    (define (good-enough guess) (< (abs-value (- x (square guess))) 0.0001))\n\
    (define try (lambda (guess) (cond ((good-enough guess) guess)\n\
                                      (else (try (improve guess))))))\n\
    (try 1)))\n\
(square-root 3)";

int main(void)
{
	tiny_eval *te;
	te_object *result;

	te = te_init();
	te_define(te, "A", te_make_integer(5));

	result = te_eval(te, expression);

	if (te_error(te))
	{
		printf("Error: %s\n", te_error(te));
	}
	else
	{
		printf("%s = ", expression);

		switch (te_object_type(result))
		{
		case TE_TYPE_NIL:
			printf("nil value\n");
			break;
		case TE_TYPE_PROCEDURE:
			printf("procedure\n");
			break;
		case TE_TYPE_USERDATA:
			printf("user data\n");
			break;
		case TE_TYPE_INTEGER:
			printf("%ld\n", te_to_integer(result));
			break;
		case TE_TYPE_NUMBER:
			printf("%g\n", te_to_number(result));
			break;
		case TE_TYPE_STRING:
			printf("%s\n", te_to_string(result));
			break;
		case TE_TYPE_BOOLEAN:
			printf("%s\n", te_to_boolean(result) ? "#t" : "#f");
			break;
		default:
			printf("unknown type\n");
			break;
		}
	}

	te_object_release(result);
	te_release(te);

	return 0;
}
