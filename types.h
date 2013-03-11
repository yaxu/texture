#define T_STRING 's'
#define T_INT 'i'
#define T_REAL 'r'
#define T_FUNCTION 'f'
#define T_WILDCARD '*'
#define T_LIST_START '['
#define T_LIST_END ']'
#define T_NOLIST 'x'
#define T_PARAM 'p'
#define T_OSC 'o'
#define T_NOTHING '~'

#define NUM_OF_TYPES 13

#define MAXLEN 255
#define MAXNAMES 2048
#define MAXDEPTH 32
#define MAXEDGES MAXNAMES
#define MAX_CONSTRAINTS NUM_OF_TYPES

#include <clutter/clutter.h>

typedef struct var t_var;
typedef struct function t_function;

typedef struct {
  int depth;
  t_var *vars[MAXDEPTH];
} t_edge;

struct function {
  t_var *input;
  t_var *output;
};

struct {
  t_var *vars[MAXLEN];
  int var_n;
} t_list;

// represent wildcard as a separate t_var, link others to it, then
// change it

typedef  t_var *t_wildcard[MAX_CONSTRAINTS + 1];

struct var {
  ClutterText *text;
  t_var *output_from;
  t_var *input_to;
  t_var *applied_as;
  char typetag;
  int assigned;

  float x, y;
  float width, height;
  float applied_x, applied_y;
  float applied_width, applied_height;
  t_var *list_next;
  
  t_wildcard wildcard;
  t_wildcard wildcard_test;
  int wildcard_curry_id;
  
  union {
    int i;
    float r;
    char s[MAXLEN + 1];
    t_function f;
    t_var *list_type;
  };
};

typedef struct {
  char name[MAXLEN + 1];
  t_var *var;
} t_name;

typedef struct {
  int n;
  t_name names[MAXNAMES];
} t_namespace;


extern t_var *i();
extern t_var *r();
extern t_var *s();
extern t_var *p();
extern t_var *p2();
extern t_var *o();
extern t_var *o2();
extern t_var *n();
extern t_var *l(t_var *type);
extern t_var *l_end();
extern t_var *w(t_var *a, ...);
extern t_var *f(t_var *input, t_var *output);
extern t_namespace *get_namespace(void);
extern t_name *find_name(t_namespace *namespace, char *str);
extern char *show_type(t_var *t);
extern char *show_name(t_name *name);
extern t_var *applied_type(t_var *v);
extern int type_eq(t_var *a, t_var *b, int curry_id);
extern void reset_mempool();
extern t_var *next_mempool();
//extern ClutterColor *type_colour(t_var *var);
