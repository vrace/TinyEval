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
(display \"The square-root of 3 is \")\
(display (square-root 3))\
(newline)";

//static char expression[] = "(display ((lambda (x) (* 2 x)) 5)) (newline)";

int main(void)
{
	tiny_eval *te;
	te_object *result;

	te = te_init();
	result = te_eval(te, expression);

	te_object_release(result);
	te_release(te);

	return 0;
}
