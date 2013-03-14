/*
    Text - visual language based on Haskell
    Copyright (C) 2011 Alex McLean
    http://yaxu.org/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include "types.h"

#define POOLSIZE 8000

static t_var pool[POOLSIZE];
int pool_n = 0;
int pool_min = 0;

extern void reset_mempool() {
  pool_n = pool_min;
}

extern void save_mempool() {
  pool_min = pool_n;
}

extern t_var *next_mempool() {
  t_var *result;
  if (pool_n >= POOLSIZE) {
    // TODO realloc
    fprintf(stderr, "-- poolsize too small!");
    exit(1);
  }

  result = &pool[pool_n++];
  memset(result, 0, sizeof(t_var));
  result->applied_as = result;
  
  return(result);
}

t_wildcard *get_wildcard(t_var *var, int curry_id) {
  t_wildcard *result;
  if (curry_id <= 0) {
    result = &var->wildcard;
  }
  else {
    if (curry_id != var->wildcard_curry_id) {
      var->wildcard_test[0] = NULL;
      for (int i = 0; var->wildcard[i] != NULL; ++i) {
        var->wildcard_test[i] = var->wildcard[i];
        var->wildcard_test[i + 1] = NULL;
      }
    }
    result = &var->wildcard_test;
  }
  return(result);
}

extern t_var *i() {
  t_var *t = next_mempool();
  t->typetag = T_INT;
  return(t);
}

extern t_var *r() {
  t_var *t = next_mempool();
  t->typetag = T_REAL;
  return(t);
}

extern t_var *s() {
  t_var *t = next_mempool();
  t->typetag = T_STRING;
  return(t);
}
/*
extern t_var *p() {
  t_var *t = next_mempool();
  t->typetag = T_PARAM;
  return(t);
}
*/
extern t_var *m() {
  t_var *t = next_mempool();
  t->typetag = T_OSC;
  return(t);
}

extern t_var *os() {
  t_var *t = next_mempool();
  t->typetag = T_OSCSTREAM;
  return(t);
}

extern t_var *n() {
  t_var *t = next_mempool();
  t->typetag = T_NOTHING;
  return(t);
}

extern t_var *l(t_var *type) {
  t_var *t = next_mempool();
  t->typetag = T_LIST;
  t->list_type = type;
  return(t);
}

extern t_var *l_end() {
  t_var *t = next_mempool();
  t->typetag = T_LIST_END;
  return(t);
}

extern t_var *w(t_var *a, ...) {
  va_list ap;
  t_var *t = next_mempool();
  t->typetag = T_WILDCARD;
  
  if (a != NULL) {
    int n = 0;

    t->wildcard[n++] = a;
    
    va_start(ap, a);
    while ((a = va_arg(ap, t_var *)) != NULL) {
      t->wildcard[n++] = a;
    }
    va_end(ap);
    t->wildcard[n] = NULL;
  }
  else {
    t->wildcard[0] = NULL;
  }
  
  return(t);
}

extern t_var *f(t_var *input, t_var *output) {
  t_var *t = next_mempool();
  t->typetag = T_FUNCTION;
  t->f.input = input;
  t->f.output = output;
  t->assigned = 0;

  input->input_to = t;
  output->output_from = t;

  return(t);
}

/**/

extern t_namespace *get_namespace(void) {
  static t_namespace namespace;
  static int first = 1;
  t_name *entry;
  t_var *a, *b;
  
  if (first) {
    first = 0;
    namespace.n = 0;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "~", MAXLEN);
    entry->var = n();

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "[", MAXLEN);
    a = w(NULL);
    entry->var = l(a);
    
    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "]", MAXLEN);
    entry->var = l_end();

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "+", MAXLEN);
    a = w(i(), r(), NULL);
    entry->var = f(a, f(a, a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "-", MAXLEN);
    a = w(i(), r(), NULL);
    entry->var = f(a, f(a, a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "subtract", MAXLEN);
    a = w(i(), r(), NULL);
    entry->var = f(a, f(a, a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "*", MAXLEN);
    a = w(i(), r(), NULL);
    entry->var = f(a, f(a, a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "/", MAXLEN);
    entry->var = f(r(), f(r(), r()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "div", MAXLEN);
    entry->var = f(i(), f(i(), i()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "floor", MAXLEN);
    entry->var = f(r(), i());
    
    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "fmap", MAXLEN);
    a = w(NULL);
    b = w(NULL);
    entry->var = f(f(a, b), f(l(a), l(b)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "<$>", MAXLEN);
    a = w(NULL);
    b = w(NULL);
    entry->var = f(f(a, b), f(l(a), l(b)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "<*>", MAXLEN);
    a = w(NULL);
    b = w(NULL);
    entry->var = f(l(f(a, b)), f(l(a), l(b)));
    
    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "|+|", MAXLEN);
    entry->var = f(l(m()), f(l(m()), l(m())));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "every", MAXLEN);
    a = w(NULL);
    entry->var = f(i(), f(f(l(a), l(a)), f(l(a), l(a))));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "sound", MAXLEN);
    entry->var = f(l(s()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "speed", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "begin", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "end", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "shape", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "vowel", MAXLEN);
    entry->var = f(l(s()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "pan", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "cutoff", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "resonance", MAXLEN);
    entry->var = f(l(r()), l(m()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "sampler", MAXLEN);
    entry->var = f(l(m()), m());

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "sample", MAXLEN);
    entry->var = f(s(), f(i(), s()));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "sinewave", MAXLEN);
    entry->var = l(r());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "sinewave1", MAXLEN);
    entry->var = l(r());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "triwave", MAXLEN);
    entry->var = l(r());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "triwave1", MAXLEN);
    entry->var = l(r());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "squarewave", MAXLEN);
    entry->var = l(r());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "squarewave1", MAXLEN);
    entry->var = l(r());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "<~", MAXLEN);
    a = w(NULL);
    entry->var = f(r(), f(l(a), l(a)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "~>", MAXLEN);
    a = w(NULL);
    entry->var = f(r(), f(l(a), l(a)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, ">+<", MAXLEN);
    a = w(NULL);
    entry->var = f(l(a), f(l(a), l(a)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "pure", MAXLEN);
    a = w(i(), r(), s(), NULL);
    entry->var = f(a, l(a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "silence", MAXLEN);
    entry->var = l(m());
    entry->var->list_function = 1;

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "palindrome", MAXLEN);
    a = w(NULL);
    entry->var = f(l(a), l(a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "brak", MAXLEN);
    a = w(NULL);
    entry->var = f(l(a), l(a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "slow", MAXLEN);
    a = w(NULL);
    entry->var = f(i(), f(l(a), l(a)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "density", MAXLEN);
    a = w(NULL);
    entry->var = f(i(), f(l(a), l(a)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "rev", MAXLEN);
    a = w(NULL);
    entry->var = f(l(a), l(a));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "striate", MAXLEN);
    entry->var = f(i(), f(l(m()), l(m())));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "smudge", MAXLEN);
    a = w(NULL);
    entry->var = f(l(r()), f(l(a), l(a)));

    entry = &namespace.names[namespace.n++];
    strncpy(entry->name, "samples", MAXLEN);
    entry->var = f(l(s()), f(l(i()), l(s())));

    save_mempool();
  }
  return(&namespace);
}

extern t_name *find_name(t_namespace *namespace, char *str) {
  int i;
  t_name *result = NULL;
  for (i = 0; i < namespace->n; ++i) {
    t_name *name = &namespace->names[i];
    
    if (strncmp(str, name->name, MAXLEN) == 0) {
      result = name;
      break;
    }
  }

  return(result);
}

extern char *show_type(t_var *t) {
  char tmp[256];
  if (t->applied_as) {
    t = t->applied_as;
  }

  static char str[256];
  switch (t->typetag) {
    case T_INT:
      return("int");
    case T_STRING:
      return("string");
    case T_REAL:
      return("real");
    case T_LIST:
      return("pattern start");
    case T_LIST_END:
      return("pattern end");
    case T_WILDCARD:
      return("wildcard");
    case T_FUNCTION:
      snprintf(tmp, 255, "(%s -> %s)", show_type(t->f.input), show_type(t->f.output));
      strncpy(str, tmp, 255);
      return(str);
  };
  return("error");
}

extern char *show_name(t_name *name) {
  static char str[256];
  snprintf(str, 255, "%s: %s", name->name, show_type(name->var));
  return(str);
}

/* visit http://chordpunch.com/ today */
extern t_var *applied_type(t_var *v) {
  while(v->typetag == T_FUNCTION && v->assigned) {
    v = v->f.output;
  }
  return(v);
}

/* Kiti le Step - Bam!!! E.P. out 1st September on all good digital
   music stores */
int wildcard_matches(t_var *a, t_var *b) {
  assert(a->typetag == T_WILDCARD);
  int result = 0;
  if (a->wildcard[0] == NULL) {
    result = 1;
  }
  else {
    for (int i = 0; a->wildcard[i] != NULL; ++i) {
      if (type_eq(a->wildcard[i], b, -1)) {
        result = 1;
        break;
      }
    }
  }
  
  return(result);
}

/**/

void resolve_wildcard(t_var *var, t_var *as, int curry_id) {
  t_wildcard *wildcard = get_wildcard(var, curry_id);
  (*wildcard)[0] = as;
  (*wildcard)[1] = NULL;
}

/**/

// Something weird clobbering a normal function into a wildcard in here if
// resolve = 1
extern int type_eq(t_var *a, t_var *b, int curry_id) {
  int result = 0;
  int resolve = curry_id >= 0;
  t_wildcard *wildcard_a, *wildcard_b;
  
  if (a->typetag == b->typetag) {
    switch(a->typetag) {
    case T_FUNCTION:
      if (type_eq(a->f.input, b->f.input, curry_id) 
          && type_eq(a->f.output, b->f.output, curry_id)) {
        result = 1;
      }
      break;
    case T_LIST:
      if (type_eq(a->list_type, b->list_type, curry_id)) {
        result = 1;
      }
      break;
    case T_WILDCARD:
      wildcard_a = get_wildcard(a, curry_id);
      wildcard_b = get_wildcard(b, curry_id);
      // two wildcards...
      if ((*wildcard_a)[0] == NULL || (*wildcard_b)[0] == NULL) {
        result = 1;
        if (resolve) {
          if ((*wildcard_a)[0] == NULL && (*wildcard_b)[0] == NULL) {
            // do nothing
          }
          else {
            t_wildcard *wildcard_x, *wildcard_y;
            if ((*wildcard_a)[0] == NULL) {
              wildcard_x = wildcard_b;
              wildcard_y = wildcard_a;
            }
            else {
              wildcard_x = wildcard_a;
              wildcard_y = wildcard_b;
            }
            
            (*wildcard_y)[0] = NULL;
            for (int i = 0; (*wildcard_x)[i] != NULL; ++i) {
              (*wildcard_y)[i] = (*wildcard_x)[i];
              (*wildcard_y)[i + 1] = NULL;
            }
          }
        }
      }
      else {
        for (int i = 0; ((*wildcard_a)[i] != NULL) && !result; ++i) {
          for (int j = 0; ((*wildcard_b)[j] != NULL) && !result; ++j) {
            if (type_eq((*wildcard_a)[i], (*wildcard_b)[j], -1)) {
              result = 1;
            }
          }
        }
        
        if (result == 1 && resolve) {
          t_wildcard tmp;
          int n = 0;
          for (int i = 0; (*wildcard_a)[i] != NULL; ++i) {
            int found = 0;
            for (int j = 0; (*wildcard_b)[j] != NULL; ++j) {
              if (type_eq((*wildcard_a)[i], (*wildcard_b)[j], -1)) {
                found = 1;
                break;
              }
            }
            if (found) {
              tmp[n++] = (*wildcard_a)[i];
            }
          }
          tmp[n++] = NULL;
          for (int i = 0; i < n; ++i) {
            (*wildcard_b)[i] = (*wildcard_a)[i] = tmp[i];
          }
        }
      }
      break;
    default:
      result = 1;
      break;
    }
  }
  else {
    if (a->typetag == T_WILDCARD) {
      result = wildcard_matches(a, b);
      if (result && resolve) {
        resolve_wildcard(a, b, curry_id);
      }
    }
    else {
      if (b->typetag == T_WILDCARD) {
        result = wildcard_matches(b, a);
        if (result && resolve) {
          resolve_wildcard(b, a, curry_id);
        }
      }
    }
  }
  
  return(result);
}

