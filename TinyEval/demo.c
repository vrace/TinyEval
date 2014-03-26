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

//static char expression[] =
//"(cond ( ( > 7 0 ) \"Hmmm\" ) )";

//static char expression[] =
//"(define (abs x) (if (< x 0) (- x) x)) (abs 7)";

te_object* equals(tiny_eval *te, void *user, te_object *operands[], int count)
{
	return te_make_boolean((te_to_number(operands[0]) == te_to_number(operands[1])) ? 1 : 0);
}

te_object* lesser(tiny_eval *te, void *user, te_object *operands[], int count)
{
	return te_make_boolean((te_to_number(operands[0]) < te_to_number(operands[1])) ? 1 : 0);
}

te_object* greater(tiny_eval *te, void *user, te_object *operands[], int count)
{
	return te_make_boolean((te_to_number(operands[0]) > te_to_number(operands[1])) ? 1 : 0);
}

te_object* negative(tiny_eval *te, void *user, te_object *operands[], int count)
{
	return te_make_number(-te_to_number(operands[0]));
}

te_object* minus(tiny_eval *te, void *user, te_object *operands[], int count)
{
	if (count == 1)
		return negative(te, user, operands, count);

	return te_make_number(te_to_number(operands[0]) - te_to_number(operands[1]));
}

te_object* divides(tiny_eval *te, void *user, te_object *operands[], int count)
{
	return te_make_number(te_to_number(operands[0]) / te_to_number(operands[1]));
}

te_object* plus(tiny_eval *te, void *user, te_object *operands[], int count)
{
	int i;
	te_type type;
	double result = 0;

	for (i = 0; i < count; i++)
	{
		assert(operands && operands[i]);
		type = te_object_type(operands[i]);
		if (type == TE_TYPE_INTEGER || type == TE_TYPE_NUMBER)
		{
			result += te_to_number(operands[i]);
		}
		else
		{
			te_set_error(te, "plus: unexpected operand type");
			break;
		}
	}
	
	return te_error(te) ? NULL : te_make_number(result);
}

te_object* multiplies(tiny_eval *te, void *user, te_object *operands[], int count)
{
	double result = 0;
	int i;
	te_type type;

	for (i = 0; i < count; i++)
	{
		double operand = 0;

		assert(operands && operands[i]);
		type = te_object_type(operands[i]);
		if (type == TE_TYPE_INTEGER || type == TE_TYPE_NUMBER)
		{
			operand = te_to_number(operands[i]);
		}
		else
		{
			te_set_error(te, "multiplies: unexpected operand type");
			break;
		}

		if (i == 0)
		{
			result = operand;
		}
		else
		{
			result = result * operand;
		}
	}
	
	return te_error(te) ? NULL : te_make_number(result);
}

int main(void)
{
	tiny_eval *te;
	te_object *result;

	te = te_init();
	te_define(te, "+", te_make_procedure(plus, NULL));
	te_define(te, "*", te_make_procedure(multiplies, NULL));
	te_define(te, "A", te_make_integer(5));
	te_define(te, "<", te_make_procedure(lesser, NULL));
	te_define(te, "=", te_make_procedure(equals, NULL));
	te_define(te, ">", te_make_procedure(greater, NULL));
	te_define(te, "-", te_make_procedure(minus, NULL));
	te_define(te, "/", te_make_procedure(divides, NULL));

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
		default:
			printf("unknown type\n");
			break;
		}
	}

	te_object_release(result);
	te_release(te);

	return 0;
}
