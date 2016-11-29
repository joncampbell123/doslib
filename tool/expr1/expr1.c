
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>

/* lowest to highest precedence */
enum {
    PFWHAT_VALUE=0,             /* 0   (value)   */
    PFWHAT_OR,                  /*     |         */
    PFWHAT_XOR,                 /*     ^         */
    PFWHAT_AND,                 /*     &         */
    PFWHAT_EQUCMP,              /*     ==        */
    PFWHAT_ADD,                 /* 5   +         */
    PFWHAT_SUB,                 /*     -         */
    PFWHAT_MUL,                 /*     *         */
    PFWHAT_DIV,                 /*     /         */
    PFWHAT_MOD,                 /*     %         */
    PFWHAT_NEG,                 /* 10  -(value)  */
    PFWHAT_LPARENS,             /*     (         */
    PFWHAT_RPARENS,             /*     )         */

    PFWHAT_MAX
};

/* precedence order array */
static const unsigned char what_prec[PFWHAT_MAX] = {
    0,                          /* 0  VALUE */
    1,                          /*    OR */
    2,                          /*    XOR */
    3,                          /*    AND */
    4,                          /*    EQUCMP */
    5,                          /* 5  ADD */
    5,                          /*    SUB */
    6,                          /*    MUL */
    6,                          /*    DIV */
    6,                          /*    MOD */
    7,                          /* 10 NEG */
    255,                        /*    LPAREMS */
    255                         /*    RPARENS */
};

static const char *what_str[PFWHAT_MAX] = {
    "",                         /* 0 */
    "|",
    "^",
    "&",
    "==",
    "+",                        /* 5 */
    "-",
    "*",
    "/",
    "%",
    "NEG",                      /* 10 */
    "(",
    ")"
};

struct postfix_t {
    unsigned char       what;
    signed long         value;
};

struct postfix_expr_t {
    struct postfix_t    pf[64];
    unsigned int        count;
};

int parse_expr_token(struct postfix_t * const tok,const char **ps,const unsigned char last_what) {
    const char *s = *ps;

    while (isspace(*s)) s++;

    if (isdigit(*s)) {
        if (last_what == PFWHAT_VALUE)
            return -1;

        tok->what = PFWHAT_VALUE;
        tok->value = strtol(s,(char**)(&s),0);
        *ps = s;
        return 0;
    }

    switch (*s) {
        case '-':
            /* if the previous token was a value, this is subtract.
             * else, it's negate */
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_SUB;
            else if (last_what == PFWHAT_NEG)
                return -1; /* we don't want no double negatives */
            else
                tok->what = PFWHAT_NEG;
            s++;
            break;
        case '+':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_ADD;
            else
                return -1;
            s++;
            break;
        case '*':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_MUL;
            else
                return -1;
            s++;
            break;
        case '/':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_DIV;
            else
                return -1;
            s++;
            break;
        case '%':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_MOD;
            else
                return -1;
            s++;
            break;
        case '|':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_OR;
            else
                return -1;
            s++;
            break;
        case '&':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_AND;
            else
                return -1;
            break;
        case '^':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS)
                tok->what = PFWHAT_XOR;
            else
                return -1;
            break;
        case '=':
            if (last_what == PFWHAT_VALUE || last_what == PFWHAT_RPARENS) {
                s++;
                if (*s == '=') {
                    tok->what = PFWHAT_EQUCMP;
                    s++;
                }
                else {
                    return -1;
                }
            }
            else {
                return -1;
            }
            break;
        case '(':
            tok->what = PFWHAT_LPARENS;
            s++;
            break;
        case ')':
            tok->what = PFWHAT_RPARENS;
            s++;
            break;
        default:
            fprintf(stderr,"Unexpected %c\n",*s);
            return -1;
    };

    *ps = s;
    return 0;
}

int postfix_expr_add(struct postfix_expr_t * const expr,const struct postfix_t * const tok) {
    if (expr->count >= 64)
        return -1;

    expr->pf[expr->count++] = *tok;
    return 0;
}

int parse_expr(struct postfix_expr_t * const expr,const char *s) {
    unsigned char last_what = PFWHAT_MAX;
    struct postfix_t op[64];
    unsigned int opstk = 0;
    struct postfix_t tok;

    expr->count = 0;

    while (*s != 0) {
        if (parse_expr_token(&tok,&s,last_what) < 0)
            return -1;

        if (tok.what == PFWHAT_VALUE) {
            if (postfix_expr_add(expr,&tok) < 0)
                return -1;
        }
        else if (tok.what == PFWHAT_LPARENS) {
            if (opstk >= 64)
                return -1;

            op[opstk++] = tok;
        }
        else if (tok.what == PFWHAT_RPARENS) {
            while (opstk > 0) {
                const struct postfix_t * const top = &op[opstk-1];

                if (top->what == PFWHAT_LPARENS) {
                    opstk--; /* discard */
                    break;
                }
                else if (postfix_expr_add(expr,top) < 0)
                    return -1;

                opstk--;
            }
        }
        else {
            while (opstk > 0) {
                const struct postfix_t * const top = &op[opstk-1];

                if (top->what == PFWHAT_LPARENS)
                    break;
                else if (what_prec[tok.what] <= what_prec[top->what]) {
                    if (postfix_expr_add(expr,top) < 0)
                        return -1;

                    opstk--;
                }
                else {
                    break;
                }
            }

            if (opstk >= 64)
                return -1;

            op[opstk++] = tok;
        }

#if 0
        fprintf(stderr,"what=\"%s\"",what_str[tok.what]);
        if (tok.what == PFWHAT_VALUE) fprintf(stderr," value=%lu",tok.value);
        fprintf(stderr,"\n");
#endif

        last_what = tok.what;
    }

    while (opstk > 0) {
        const struct postfix_t * const top = &op[opstk-1];

        if (top->what == PFWHAT_LPARENS || top->what == PFWHAT_RPARENS) {
            fprintf(stderr,"parse error: unbalanced parenthesis\n");
            return -1;
        }
        if (postfix_expr_add(expr,top) < 0)
            return -1;

        opstk--;
    }

    return 0;
}

void print_expr(const struct postfix_expr_t * const expr) {
    unsigned int i;

    for (i=0;i < expr->count;i++) {
        const struct postfix_t *p = &expr->pf[i];

        if (p->what == PFWHAT_VALUE)
            printf("%lu ",p->value);
        else
            printf("%s ",what_str[p->what]);
    }
}

unsigned long eval_expr(const struct postfix_expr_t * const expr) {
    signed long stk[64],A,B;
    unsigned int stp = 0;
    unsigned int i;

    for (i=0;i < expr->count;i++) {
        const struct postfix_t *p = &expr->pf[i];

        switch (p->what) {
            case PFWHAT_VALUE:
                if (stp >= 64)
                    goto stack_overflow;
                stk[stp++] = p->value;
                break;
            case PFWHAT_ADD:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B + A;
                break;
            case PFWHAT_SUB:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B - A;
                break;
             case PFWHAT_MUL:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B * A;
                break;
             case PFWHAT_DIV:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B / A;
                break;
             case PFWHAT_MOD:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B % A;
                break;
              case PFWHAT_OR:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B | A;
                break;
             case PFWHAT_XOR:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B ^ A;
                break;
             case PFWHAT_AND:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B & A;
                break;
             case PFWHAT_EQUCMP:
                if (stp < 2)
                    goto stack_underflow;
                A = stk[--stp];
                B = stk[--stp];
                stk[stp++] = B == A;
                break;
             case PFWHAT_NEG:
                if (stp < 1)
                    goto stack_underflow;
                stk[stp-1] = -stk[stp-1];
                break;
            default:
                fprintf(stderr,"Unknown pf token\n");
                return LONG_MAX;
        }
    }

    if (stp == 0) {
        fprintf(stderr,"eval no result\n");
        return LONG_MAX;
    }
    else if (stp > 1) {
        fprintf(stderr,"eval failed to clear stack\n");
        return LONG_MAX;
    }

    return stk[0];
stack_overflow:
    fprintf(stderr,"eval stack overflow\n");
    return LONG_MAX;
stack_underflow:
    fprintf(stderr,"eval stack underflow\n");
    return LONG_MAX;
}

int main(int argc,char **argv) {
    struct postfix_expr_t expr;
    signed long val;

    if (argc < 2) {
        fprintf(stderr,"Please enter an expression in argv[1]\n");
        return 1;
    }

    if (parse_expr(&expr,argv[1]) < 0) {
        fprintf(stderr,"Failure to parse\n");
        return 1;
    }

    print_expr(&expr);
    printf("\n");

    val = eval_expr(&expr);
    if (val == LONG_MAX) {
        fprintf(stderr,"Failure to eval\n");
        return 1;
    }
    printf("result = %ld\n",val);

    return 0;
}

