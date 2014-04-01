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

#ifndef __TINY_EVAL_H__
#define __TINY_EVAL_H__

typedef struct tag_tiny_eval tiny_eval;
typedef struct tag_te_object te_object;

tiny_eval* te_init(void);
void te_release(tiny_eval *te);

void te_define(tiny_eval *te, const char *symbol, te_object *object);
te_object* te_eval(tiny_eval *te, const char *expression);

const char *te_error(tiny_eval *te);
void te_set_error(tiny_eval *te, const char *str);

#define TE_TYPE_NIL       0
#define TE_TYPE_PROCEDURE 1
#define TE_TYPE_USERDATA  2
#define TE_TYPE_INTEGER   3
#define TE_TYPE_NUMBER    4
#define TE_TYPE_STRING    5
#define TE_TYPE_BOOLEAN   6

typedef int te_type;

te_type te_object_type(te_object *object);
te_object* te_object_retain(te_object *object);
void te_object_release(te_object *object);

#define TE_PROC(name) te_object* name\
	(tiny_eval *te, void *user, te_object *operands[], int count)

typedef TE_PROC((*te_procedure));

te_object* te_make_nil(void);
te_object* te_make_procedure(te_procedure proc, void *user);
te_object* te_make_userdata(void *user);
te_object* te_make_integer(long value);
te_object* te_make_number(double number);
te_object* te_make_str(const char *str);
te_object* te_make_string(const char *str, const char *end);
te_object* te_make_boolean(int value);
te_object* te_make_true();
te_object* te_make_false();

te_object* te_call(tiny_eval *te, te_object *procedure, te_object *operands[], int count);
void* te_to_userdata(te_object *object);
long te_to_integer(te_object *object);
double te_to_number(te_object *object);
const char* te_to_string(te_object *object);
int te_to_boolean(te_object *object);

#endif
