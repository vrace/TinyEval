/*

WANE's Tiny Evaluator

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held liable
for any damages arising from the use of this software. 

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software. 

3. This notice may not be removed or altered from any source
distribution.

wane <newsheep@gmail.com>

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "te.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

struct tag_te_environment
{
	struct tag_te_environment *link;
	struct tag_te_symbol *symbol;
	int symbol_count;
	int symbol_cap;
};

struct tag_tiny_eval
{
	char *error;
	struct tag_te_environment global;
	struct tag_te_environment *env;
};

struct tag_te_object
{
	int ref;
	te_type type;
	union
	{
		struct tag_te_proc_data *procedure;
		void *userdata;
		long int_value;
		double num_value;
		char *str_value;
	}
	data;
};

struct tag_te_proc_data
{
	te_procedure proc;
	void *user;
};

struct tag_te_symbol
{
	char *name;
	te_object *object;
};

struct tag_te_lambda_data
{
	char **binding;
	int binding_count;
	char *combination;
	struct tag_te_environment env;
};

typedef struct tag_te_symbol te_symbol;
typedef struct tag_te_environment te_environment;
typedef struct tag_te_proc_data te_proc_data;
typedef struct tag_te_lambda_data te_lambda_data;

static te_object* apply(tiny_eval *te, const char *op, te_object *operands[], int count);
static te_object* eval(tiny_eval *te, const char **exp);
static te_object* te_lambda_proc(tiny_eval *te, void *user, te_object *operands[], int count);

char* te_str_extract(const char *begin, const char *end)
{
	char *str;
	size_t length;

	assert(begin);
	assert(end);
	assert(end >= begin);

	length = end - begin;
	str = malloc(length + 1);
	assert(str);

	memcpy(str, begin, length);
	str[length] = '\0';

	return str;
}

char* te_str_copy(const char *str)
{
	assert(str);
	return te_str_extract(str, str + strlen(str));
}

int te_is_space(char ch)
{
	switch (ch)
	{
	case ' ':
	case '\t':
	case '\r':
	case '\n':
		return 1;

	default:
		break;
	}
	
	return 0;
}

const char* te_close_string(const char *p)
{
	int skip = 0;
	int done = 0;
	assert(*p == '"');

	for (p++; *p && !done; p++)
	{
		if (!skip)
		{
			if (*p == '"')
				done = 1;
			else if (*p == '\\')
				skip = 1;
		}
		else
		{
			skip = 0;
		}
	}

	return p;
}

const char* te_close_bracket(const char *p)
{
	int count = 1;
	assert(*p == '(');

	for (p++; *p && count != 0; p++)
	{
		if (*p == '(')
			count++;
		else if (*p == ')')
			count--;
		else if (*p == '"')
			p = te_close_string(p) - 1;
	}

	return p;
}

const char* te_token_begin(const char *p)
{
	assert(p);

	for (; *p && te_is_space(*p); p++);

	return p;
}

const char* te_token_end(const char *p)
{
	assert(p);
	assert(!te_is_space(*p));
	assert(*p != ')');

	if (*p == '(')
	{
		p = te_close_bracket(p);
	}
	else if (*p == '"')
	{
		p = te_close_string(p);
	}
	else
	{
		for (; *p && !te_is_space(*p) && *p != ')'; p++);
	}

	return p;
}

te_type te_object_type(te_object *object)
{
	return object ? object->type : TE_TYPE_NIL;
}

te_lambda_data* te_lambda_init(tiny_eval *te)
{
	te_lambda_data *lambda;

	lambda = malloc(sizeof(te_lambda_data));
	assert(lambda);

	lambda->binding = NULL;
	lambda->binding_count = 0;
	lambda->combination = NULL;
	lambda->env.link = te->env;
	lambda->env.symbol = NULL;
	lambda->env.symbol_cap = 0;
	lambda->env.symbol_count = 0;

	return lambda;
}

void te_lambda_release(te_lambda_data *lambda)
{
	int i;

	assert(lambda);

	for (i = 0; i < lambda->binding_count; free(lambda->binding[i++]));

	if (lambda->binding)
		free(lambda->binding);

	if (lambda->combination)
		free(lambda->combination);

	for (i = 0; i < lambda->env.symbol_count; i++)
	{
		assert(lambda->env.symbol[i].name);
		free(lambda->env.symbol[i].name);
		te_object_release(lambda->env.symbol[i].object);
	}

	if (lambda->env.symbol)
		free(lambda->env.symbol);

	free(lambda);
}

void te_lambda_bindings(te_lambda_data *lambda, const char **exp)
{
	const char *start;
	const char *p;
	int binding_cap = 0;

	assert(exp);
	assert(*exp);
	assert(lambda);
	assert(lambda->binding == NULL);

	start = *exp;

	while (*start && *start != ')')
	{
		start = te_token_begin(start);
		p = te_token_end(start);

		if (lambda->binding_count <= binding_cap)
		{
			binding_cap += 8;
			lambda->binding = realloc(lambda->binding, binding_cap);
			assert(lambda->binding);
		}

		lambda->binding[lambda->binding_count++] = te_str_extract(start, p);
		start = te_token_begin(p);
	}

	*exp = start;
}

te_object* te_object_retain(te_object *object)
{
	if (object)
		object->ref++;
	return object;
}

void te_object_release(te_object *object)
{
	if (object)
	{
		if (--object->ref <= 0)
		{
			te_type type = te_object_type(object);
			
			if (type == TE_TYPE_PROCEDURE)
			{
				assert(object->data.procedure);

				if (object->data.procedure->proc == te_lambda_proc)
					te_lambda_release(object->data.procedure->user);

				free(object->data.procedure);
			}
			else if (type == TE_TYPE_STRING)
			{
				assert(object->data.str_value);
				free(object->data.str_value);
			}

			free(object);
		}
	}
}

te_object* te_make_nil(void)
{
	te_object *out;

	out = malloc(sizeof(te_object));
	assert(out);

	out->ref = 1;
	out->type = TE_TYPE_NIL;

	return out;
}

te_object* te_make_procedure(te_procedure proc, void *user)
{
	te_object *out;

	assert(proc);

	out = malloc(sizeof(te_object));
	assert(out);

	out->ref = 1;
	out->type = TE_TYPE_PROCEDURE;
	out->data.procedure = malloc(sizeof(te_proc_data));
	assert(out->data.procedure);
	out->data.procedure->proc = proc;
	out->data.procedure->user = user;

	return out;
}

te_object* te_make_userdata(void *user)
{
	te_object *out;

	out = malloc(sizeof(te_object));
	assert(out);

	out->ref = 1;
	out->type = TE_TYPE_USERDATA;
	out->data.userdata = user;

	return out;
}

te_object* te_make_integer(long value)
{
	te_object *out;

	out = malloc(sizeof(te_object));
	assert(out);

	out->ref = 1;
	out->type = TE_TYPE_INTEGER;
	out->data.int_value = value;

	return out;
}

te_object* te_make_number(double number)
{
	te_object *out;

	out = malloc(sizeof(te_object));
	assert(out);

	out->ref = 1;
	out->type = TE_TYPE_NUMBER;
	out->data.num_value = number;

	return out;
}

te_object* te_make_string(const char *str, const char *end)
{
	size_t length;
	te_object *out;

	assert(str);
	assert(end);

	out = malloc(sizeof(te_object));
	assert(out);

	length = end - str;
	out->ref = 1;
	out->type = TE_TYPE_STRING;
	out->data.str_value = te_str_extract(str, end);

	return out;
}

te_object* te_call(tiny_eval *te, te_object *procedure, te_object *operands[], int count)
{
	te_object *result = NULL;

	assert(procedure);

	if (te_object_type(procedure) == TE_TYPE_PROCEDURE)
	{
		result = procedure->data.procedure->proc(
			te,
			procedure->data.procedure->user,
			operands, count);
	}

	return result;
}

void* te_to_userdata(te_object *object)
{
	void *user = NULL;

	assert(object);

	if (te_object_type(object) == TE_TYPE_USERDATA)
	{
		user = object->data.userdata;
	}

	return user;
}

long te_to_integer(te_object *object)
{
	long value = 0;

	assert(object);

	if (te_object_type(object) == TE_TYPE_INTEGER)
	{
		value = object->data.int_value;
	}

	return value;
}

double te_to_number(te_object *object)
{
	te_type type;
	double number = 0;

	assert(object);
	type = te_object_type(object);

	if (type == TE_TYPE_NUMBER)
	{
		number = object->data.num_value;
	}
	else if (type == TE_TYPE_INTEGER)
	{
		number = object->data.int_value;
	}

	return number;
}

const char* te_to_string(te_object *object)
{
	const char *str = NULL;

	assert(object);

	if (te_object_type(object) == TE_TYPE_STRING)
	{
		str = object->data.str_value;
	}

	return str;
}

tiny_eval* te_init(void)
{
	tiny_eval *te;

	te = malloc(sizeof(tiny_eval));
	assert(te);

	te->error = NULL;
	te->global.link = NULL;
	te->global.symbol = NULL;
	te->global.symbol_cap = 0;
	te->global.symbol_count = 0;
	te->env = &te->global;

	return te;
}

void te_release(tiny_eval *te)
{
	int i;
	assert(te);

	if (te->error)
		free(te->error);

	for (i = 0; i < te->global.symbol_count; i++)
	{
		assert(te->global.symbol[i].name);

		free(te->global.symbol[i].name);
		te_object_release(te->global.symbol[i].object);
	}

	if (te->global.symbol)
		free(te->global.symbol);

	free(te);
}

te_symbol* te_symbol_find(tiny_eval *te, const char *name)
{
	int i;
	te_symbol *out;
	te_environment *env;

	assert(te);
	assert(name);

	out = NULL;
	env = te->env;

	while (env && !out)
	{
		for (i = env->symbol_count - 1; i >= 0; i--)
		{
			assert(env->symbol[i].name);
			if (strcasecmp(name, env->symbol[i].name) == 0)
			{
				out = &env->symbol[i];
				break;
			}
		}

		env = env->link;
	}

	return out;
}

void te_symbol_init(te_symbol *s, const char *name, te_object *object)
{
	assert(s);
	assert(name);

	s->name = te_str_copy(name);
	s->object = object;
}

void te_define(tiny_eval *te, const char *symbol, te_object *object)
{
	te_environment *env;

	assert(te);
	assert(symbol);

	env = &te->global;
	if (env->symbol_count >= env->symbol_cap)
	{
		env->symbol_cap += 100;
		env->symbol = realloc(env->symbol, sizeof(te_symbol) * env->symbol_cap);
		assert(env->symbol);
	}

	te_symbol_init(&env->symbol[env->symbol_count++], symbol, object);
}

void te_define_local(tiny_eval *te, const char *symbol, te_object *object)
{
	te_environment *env;

	assert(te);
	assert(te->env);
	assert(symbol);

	env = te->env;
	if (env->symbol_count >= env->symbol_cap)
	{
		env->symbol_cap += 100;
		env->symbol = realloc(env->symbol, sizeof(te_symbol) * env->symbol_cap);
		assert(env->symbol);
	}

	te_symbol_init(&env->symbol[env->symbol_count++], symbol, object);
}

te_object* te_eval(tiny_eval *te, const char *expression)
{
	te_object *result = NULL;
	const char **exp;

	assert(te);

	exp = &expression;
	te_set_error(te, NULL);

	while (!te_error(te) && *(*exp))
	{
		te_object_release(result);
		result = eval(te, exp);
		*exp = te_token_begin(*exp);
	}

	return result;
}

const char *te_error(tiny_eval *te)
{
	assert(te);
	return te->error;
}

void te_set_error(tiny_eval *te, const char *str)
{
	assert(te);

	if (te->error)
	{
		free(te->error);
		te->error = NULL;
	}

	if (str)
	{
		te->error = te_str_copy(str);
	}
}

te_object* apply(tiny_eval *te, const char *op, te_object *operands[], int count)
{
	te_object *result;
	te_symbol *s;
 
	assert(te);
	assert(op);

	result = NULL;
	s = te_symbol_find(te, op);

	if (s)
	{
		if (te_object_type(s->object) == TE_TYPE_PROCEDURE)
		{
			result = te_call(te, s->object, operands, count);
		}
		else
		{
			te_set_error(te, "apply: operator is not a procedure");
		}
	}
	else
	{
		te_set_error(te, "apply: unbound procedure");
	}

	return result;
}

te_object* te_define_symbol(tiny_eval *te, const char *exp, const char *end)
{
	const char *start;
	const char *p;
	char *symbol = NULL;
	char *combination = NULL;
	te_object *result = NULL;

	start = te_token_begin(te_token_end(te_token_begin(exp + 1)));
	p = te_token_end(start);

	if (*start == '(')
	{
		te_lambda_data *lambda;
		int binding_cap = 0;

		start = te_token_begin(start + 1);
		p = te_token_end(start);
		symbol = te_str_extract(start, p);

		start = te_token_begin(p);
		lambda = te_lambda_init(te);
		te_lambda_bindings(lambda, &start);

		if (start < end && *start == ')')
		{
			start = te_token_begin(++start);
			lambda->combination = te_str_extract(start, end - 1);
			result = te_make_procedure(te_lambda_proc, lambda);
			te_define(te, symbol, te_object_retain(result));
		}
		else
		{
			te_set_error(te, "define: unexpected end of procedure definition");
			
			if (lambda)
				te_lambda_release(lambda);
		}
	}
	else
	{
		symbol = te_str_extract(start, p);

		start = te_token_begin(p);
		p = te_token_end(start);

		combination = te_str_extract(start, p);

		p = te_token_begin(p);
		if (*p != ')' || (p + 1) != end)
		{
			te_set_error(te, "define: unexpected end of expression");
		}
		else
		{
			result = te_eval(te, combination);

			if (!te_error(te))
			{
				te_define_local(te, symbol, te_object_retain(result));
			}
		}
	}

	if (symbol)
		free(symbol);

	if (combination)
		free(combination);

	return result;
}

te_object* te_make_lambda(tiny_eval *te, const char *exp, const char *end)
{
	const char *start;
	const char *p;
	te_object *result = NULL;

	start = te_token_begin(te_token_end(te_token_begin(exp + 1)));
	p = te_token_end(start);

	if (*start == '(' && *(p - 1) == ')')
	{
		te_lambda_data *lambda;

		start = te_token_begin(++start);
		lambda = te_lambda_init(te);
		te_lambda_bindings(lambda, &start);

		if (start < end && *start == ')')
		{
			start = te_token_begin(++start);
			lambda->combination = te_str_extract(start, end - 1);
			result = te_make_procedure(te_lambda_proc, lambda);
		}
		else
		{
			te_set_error(te, "lambda: unexpected end of definition");

			if (lambda)
				te_lambda_release(lambda);
		}
	}
	else
	{
		te_set_error(te, "lambda: invalid expression");
	}

	return result;
}

te_object* te_conditional(tiny_eval *te, const char *exp, const char *end)
{
	const char *start;
	const char *p;
	te_object *result = NULL;

	start = te_token_begin(te_token_end(te_token_begin(exp + 1)));
	p = start;

	while (p < end - 1 && !te_error(te) && !result)
	{
		const char *block = start;

		if (*start == '(')
		{
			start = te_token_begin(++start);
			p = te_token_end(start);
			if (strncasecmp(start, "else", p - start) == 0)
			{
				result = te_make_integer(1);
				start = p;
			}
			else
			{
				result = eval(te, &start);
			}

			if (start != p)
			{
				te_set_error(te, "cond: can't eval condition");
			}
			else if (!te_error(te))
			{
				int cond = 0;

				if (te_object_type(result) == TE_TYPE_INTEGER)
					cond = te_to_integer(result);

				te_object_release(result);
				result = NULL;

				if (cond)
				{
					start = te_token_begin(p);
					p = te_token_end(block);

					if (start >= p)
					{
						te_set_error(te, "cond: unexpected end of expression");
					}
					else
					{
						char *combination = te_str_extract(start, p - 1);
						result = te_eval(te, combination);
						free(combination);
					}

					start = te_token_begin(p);
				}
				else
				{
					p = te_token_begin(te_token_end(block));
					start = p;
				}
			}
		}
		else
		{
			te_set_error(te, "cond: unexpected conditional expression");
		}
	}

	return result;
}

te_object* te_conditional_if(tiny_eval *te, const char *exp, const char *end)
{
	const char *start;
	const char *p;
	te_object *result = NULL;

	start = te_token_begin(te_token_end(te_token_begin(exp + 1)));
	p = start;

	result = eval(te, &p);

	if (!te_error(te))
	{
		if (te_object_type(result) == TE_TYPE_INTEGER)
		{
			long cond = te_to_integer(result);
			te_object_release(result);
			result = NULL;

			if (!cond)
				p = te_token_end(te_token_begin(p));

			p = te_token_begin(p);
			if (p >= end || !*p || *p == ')')
			{
				te_set_error(te, "if: unexpected end of expression");
			}
			else
			{
				result = eval(te, &p);
			}
		}
		else
		{
			te_object_release(result);
			result = NULL;
			te_set_error(te, "if: unexpected conditional result");
		}
	}

	return result;
}

te_object* eval(tiny_eval *te, const char **exp)
{
	te_object *result = NULL;
	const char *p;
	char *field = NULL;

	*exp = te_token_begin(*exp);
	p = *exp;

	if (*p == '(')
	{
		const char *start;

		start = te_token_begin(++p);
		p = te_token_end(start);

		field = te_str_extract(start, p);

		if (strcasecmp(field, "define") == 0)
		{
			p = te_token_end(*exp);
			result = te_define_symbol(te, *exp, p);
			*exp = p;
		}
		else if (strcasecmp(field, "lambda") == 0)
		{
			p = te_token_end(*exp);
			result = te_make_lambda(te, *exp, p);
			*exp = p;
		}
		else if (strcasecmp(field, "cond") == 0)
		{
			p = te_token_end(*exp);
			result = te_conditional(te, *exp, p);
			*exp = p;
		}
		else if (strcasecmp(field, "if") == 0)
		{
			p = te_token_end(*exp);
			result = te_conditional_if(te, *exp, p);
			*exp = p;
		}
		else
		{
			te_object **operands = NULL;
			int operand_cap = 0;
			int operand_count = 0;

			while (*p && *p != ')' && !te_error(te))
			{
				te_object *operand = eval(te, &p);
				p = te_token_begin(p);

				if (te_error(te))
					break;

				if (operand_count >= operand_cap)
				{
					operand_cap += 32;
					operands = realloc(operands, sizeof(te_object*) * operand_cap);
					assert(operands);
				}

				operands[operand_count++] = operand;
			}

			if (*p != ')')
			{
				if (!te_error(te))
					te_set_error(te, "eval: unexpected end of expression");

				*exp = p;
			}
			else
			{
				if (!te_error(te))
					result = apply(te, field, operands, operand_count);

				*exp = ++p;
			}

			for (operand_cap = 0; operand_cap < operand_count; te_object_release(operands[operand_cap++]));
			free(operands);
		}
	}
	else if (*p == '"')
	{
		*exp = p;
		p = te_token_end(*exp);

		if (*(p - 1) != '"')
		{
			te_set_error(te, "eval: unexpected end of string");
		}
		else
		{
			result = te_make_string(*exp + 1, p - 1);
		}

		*exp = p;
	}
	else if (*p == ')')
	{
		te_set_error(te, "eval: unexpected close parenthesis");
		p++;
	}
	else
	{
		te_symbol *s;

		*exp = p;
		p = te_token_end(p);

		field = te_str_extract(*exp, p);

		if (strchr(field, '.'))
		{
			double num;
			char *ep;

			num = strtod(field, &ep);
			if (!*ep)
			{
				result = te_make_number(num);
			}
			else
			{
				s = te_symbol_find(te, field);

				if (!s || !s->object)
				{
					te_set_error(te, "eval: unbound symbol");
				}
				else
				{
					result = te_object_retain(s->object);
				}
			}
		}
		else
		{
			long value;
			char *ep;

			value = strtol(field, &ep, 10);
			if (!*ep)
			{
				result = te_make_integer(value);
			}
			else
			{
				s = te_symbol_find(te, field);

				if (!s || !s->object)
				{
					te_set_error(te, "eval: unbound symbol");
				}
				else
				{
					result = te_object_retain(s->object);
				}
			}
		}

		*exp = p;
	}

	if (field)
		free(field);

	return result;
}

te_object* te_lambda_proc(tiny_eval *te, void *user, te_object *operands[], int count)
{
	int i;
	te_lambda_data *lambda;
	te_object *result = NULL;

	assert(te);
	assert(user);

	lambda = user;

	if (lambda->binding_count == count)
	{
		te_environment *prev = te->env;
		te->env = &lambda->env;

		for (i = 0; i < count; i++)
			te_define_local(te, lambda->binding[i], te_object_retain(operands[i]));

		result = te_eval(te, lambda->combination);

		te->env = prev;
	}
	else
	{
		te_set_error(te, "lambda: mismatch operand count");
	}

	return result;
}
