#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "te.h"

static char expression[] = "(+ 1 (* 2 3) 4 A)";

te_object* plus(tiny_eval *te, void *user, te_object *operands[], int count)
{
	int i;
	te_type type;
	double result = 0;

	for (i = 0; i < count; i++)
	{
		assert(operands && operands[i]);
		type = te_object_type(operands[i]);
		if (type == TE_TYPE_INTEGER)
		{
			result += te_to_integer(operands[i]);
		}
		else if (type == TE_TYPE_NUMBER)
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
		if (type == TE_TYPE_INTEGER)
		{
			operand = te_to_integer(operands[i]);
		}
		else if (type == TE_TYPE_NUMBER)
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
	te_define(te, "+", te_make_procedure(te, plus, NULL));
	te_define(te, "*", te_make_procedure(te, multiplies, NULL));
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
			printf("%d\n", te_to_integer(result));
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
