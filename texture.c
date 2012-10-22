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

#include <clutter/clutter.h>
#include <cairo.h>
#include <cairo-svg.h> 
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "types.h"
#include "assert.h"
#include "log.h"

//#define WIDTH 640
//#define HEIGHT 480
//#define WIDTH 1024
//#define HEIGHT 768
int WIDTH = 1024;
int HEIGHT = 768;

#define FONT_SIZE_CAIRO 17
#define FONT_SIZE_CLUTTER "14"
//#define FONT_SIZE_CAIRO 14
//#define FONT_SIZE_CLUTTER "11"

#define sqr(x) ((x)*(x))
#define MAXWORDS 1024
#define CTEXT clutter_text_get_text

//#define MONOCHROME 1

ClutterActor *stage = NULL;
ClutterActor *ctex = NULL;
ClutterActor *cursor = NULL;

ClutterText *words[MAXWORDS];
int words_n = 0;
ClutterText *editing = NULL;

gfloat mouse_x = 0;
gfloat mouse_y = 0;
gfloat mouse_x_press = 0;
gfloat mouse_y_press = 0;

ClutterColor white = {255, 255, 255, 255};  
ClutterColor black = {0, 0, 0, 255};  
ClutterColor grey = {200, 200, 200, 255};  
ClutterColor darkgrey = {100, 100, 100, 255};

// http://colorschemedesigner.com/#2L42vyiBRw0w0
ClutterColor c_int = {39,222,0,255};
ClutterColor c_real = {21,57,191,255};
ClutterColor c_string = {255,177,0,255};
ClutterColor c_param = {248,0,24,255};
ClutterColor c_osc = {186,47,60,255};
ClutterColor c_list = {252,113,126,255};

// solarized http://ethanschoonover.com/solarized

ClutterColor solar_base03 = {  0,  43,  54, 255};
ClutterColor solar_base02 = {  7,  54,  66, 255};
ClutterColor solar_base01 = { 88, 110, 117, 255};
ClutterColor solar_base00 = {101, 123, 131, 255};
ClutterColor solar_base0  = {131, 148, 150, 255};
ClutterColor solar_base1  = {147, 161, 161, 255};
ClutterColor solar_base2  = {238, 232, 213, 255};
ClutterColor solar_base3  = {253, 246, 227, 255};
ClutterColor solar_yellow = {181, 137,   0, 255};
ClutterColor solar_orange = {203,  75,  22, 255};
ClutterColor solar_red    = {220,  50,  47, 255};
ClutterColor solar_magenta= {211,  54, 130, 255};
ClutterColor solar_violet = {108, 113, 196, 255};
ClutterColor solar_blue   = { 38, 139, 210, 255};
ClutterColor solar_cyan   = { 42, 161, 152, 255};
ClutterColor solar_green  = {133, 153,   0, 255};

gboolean dark = TRUE;

gboolean text_drag = FALSE;
gboolean text_drag_tree = FALSE;
gboolean selected_words_n = 0;
ClutterText *selected_words[MAXWORDS];
//t curried[MAXWORDS];

int stage_drag = FALSE;
int stage_drag_x = -1;
int stage_drag_y = -1;

t_var curried[MAXWORDS];
int curried_n = 0;

t_edge edges[MAXWORDS];
int edge_n = 0;

t_namespace *global = NULL;
t_namespace local;

int save = 0;

typedef struct {
  float x;
  float y;
} t_coord;

#define MAX_COORDS 512
typedef struct {
  t_coord coords[MAX_COORDS];
  int n;
} t_coords;

typedef struct {
  unsigned int id;
  ClutterText *text;
} t_pair;

t_pair playback_map[MAXWORDS];
int playback_map_n = 0;

#define CURSOR_WIDTH 12
#define CURSOR_HEIGHT 12
#define SPACE_WIDTH 12.0
#define LINE_WIDTH 3
#define LINE_GAP 2
#define MAX_CHARS 2048


void break_word ();

/**/

void toggle_background() {
  ClutterColor *background;
  dark = !dark;

  background = dark ? &black : &white;
  clutter_stage_set_color (CLUTTER_STAGE(stage), background);
}

/**/

ClutterColor *type_colour(t_var *var) {
  ClutterColor *result = NULL;
  
  // would avoid this if typetags were 0 based ints...
  switch (var->typetag) {
  case T_INT:
    result = &c_int;
    break;
  case T_STRING:
    result = &c_string;
    break;
  case T_REAL:
    result = &c_real;
    break;
  case T_LIST_START:
  case T_LIST_END:
    result = &c_list;
    break;
  case T_PARAM:
    result = &c_param;
    break;
  case T_PARAM2:
    result = &c_param;
    break;
  case T_OSC:
    result = &c_osc;
    break;
  case T_OSC2:
    result = &c_osc;
    break;
  case T_WILDCARD:
    if (var->wildcard[0] != NULL && var->wildcard[1] == NULL
        && var->wildcard[0]->typetag != T_WILDCARD) {
      result = type_colour(var->wildcard[0]);
    }
    break;
  }

  return(result);
}

/**/

char *svg_filename() {
  static char result[64];
  static int n = 0;
  snprintf(result, 64, "saves/save-%d-%d.svg",
           getpid(),
           n++
           );
  return(result);
}

/**/

t_var *dup_wildcard (t_var *var, t_wildcard lookup_from, t_wildcard lookup_to) {
  int match = 0;
  t_var *result;
  for (int i = 0; lookup_from[i] != NULL; ++i) {
    if (var == lookup_from[i]) {
      result = lookup_to[i];
      match = 1;
      break;
    }
  }
  if (!match) {
    int k = 0;
    while (lookup_from[k] != NULL) {
      ++k;
    }
    assert(k <= MAX_CONSTRAINTS);
    
    result = next_mempool();
    memcpy((void *) result, (void *) var, sizeof(t_var));
    lookup_from[k] = var;
    lookup_from[k+1] = NULL;
    lookup_to[k] = result;
    lookup_to[k+1] = NULL;
  }
  return(result);
}

/**/


t_var *dup_var (t_var *var, t_wildcard lookup_from, t_wildcard lookup_to) {
  if (var == NULL) {
    return(var);
  }

  if (var->typetag == T_WILDCARD) {
    return(dup_wildcard(var, lookup_from, lookup_to));
  }
  
  t_var *new = next_mempool();
  memcpy((void *) new, (void *) var, sizeof(t_var));
  
  if (new->typetag == T_FUNCTION) {
    new->f.input = dup_var(new->f.input, lookup_from, lookup_to);
    
    if (new->f.input->typetag != T_WILDCARD) {
      new->f.input->input_to = new;
    }
    
    new->f.output = dup_var(new->f.output, lookup_from, lookup_to);
    if (new->f.output->typetag != T_WILDCARD) {
      new->f.output->output_from = new;
    }
  }
  else {
    if (new->typetag == T_LIST_START) {
      assert(new->list_type != NULL);
      new->list_type = dup_var(new->list_type, lookup_from, lookup_to);
    }
  }

  return new;
}

/**/

void show_function_recurse (t_var *var, char *result) {
  t_var *tmp;
  char tmpstr[256];
  
  switch (var->typetag) {
  case T_FUNCTION:
    
    if (var->text != NULL) {
      char c = clutter_text_get_text(var->text)[0];
      int infix = 1;
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        infix = 0;
      }

      if (infix) {
        strncat(result, "(", MAX_CHARS);
      }

      strncat(result, clutter_text_get_text(var->text), MAX_CHARS);

      if (infix) {
        strncat(result, ") ", MAX_CHARS);
      }
      else {
        strncat(result, " ", MAX_CHARS);
      }
    }
    
    if (var->f.input->assigned) {
      strncat(result, "(", MAX_CHARS);
      show_function_recurse(var->f.input, result);
      strncat(result, ")", MAX_CHARS);
      strncat(result, " ", MAX_CHARS);
    }
    
    if (var->f.output->assigned) {
      if (var->f.output->typetag == T_FUNCTION) {
        show_function_recurse(var->f.output, result);
      }
    }
    
    break;

  case T_LIST_START:
    tmp = var->list_next;
    strncat(result, "(lToP [", MAX_CHARS);
    if (tmp != NULL) {
      while(tmp->typetag != T_LIST_END) {
        if (tmp->typetag == T_NOTHING) {
          strncat(result, "Nothing", MAX_CHARS);
        }
        else {
          strncat(result, "Just ", MAX_CHARS);
          show_function_recurse(tmp, result);
        }
        tmp = tmp->list_next;
        if (tmp->typetag != T_LIST_END) {
          strncat(result, ", ", MAX_CHARS);
        }
      }
    }
    strncat(result, "])", MAX_CHARS);
    break;
    
  case T_STRING:
    if (var->text != NULL) {
      strncat(result, "\"", MAX_CHARS);
      strncat(result, var->s, MAX_CHARS);
      strncat(result, "\"", MAX_CHARS);
    }
    else {
      strncat(result, "?", MAX_CHARS);
    }
    break;
  case T_INT:
    if (var->text != NULL) {
      sprintf(tmpstr, "%d", var->i);
      strncat(result, tmpstr, MAX_CHARS);
    }
    else {
      strncat(result, "?", MAX_CHARS);
    }
    break;
  case T_REAL:
    if (var->text != NULL) {
      sprintf(tmpstr, "%f", var->r);
      strncat(result, tmpstr, MAX_CHARS);
    }
    else {
      strncat(result, "?", MAX_CHARS);
    }
    break;
  default:
    if (var->text != NULL) {
      strncat(result, clutter_text_get_text(var->text), MAX_CHARS);
    }
    else {
      strncat(result, "?", MAX_CHARS);
    }
  }
}

char *show_function (t_var *var) {
  static char result[MAX_CHARS];
  result[0] = '\0';
  show_function_recurse(var, result);
  return(result);
}

/**/

t_var *named_in (t_var *var) {
  if (var->f.input->typetag != T_FUNCTION || var->f.input->text != NULL) {
    var = var->f.input;
  }
  return(var);
}

/**/

void draw_output_node (cairo_t *cr, t_var *var) {
  gfloat x, y;

  if (! var->text) {
    fprintf(stderr, "no text to draw args of\n");
    return;
  }

  cairo_set_line_width (cr, LINE_WIDTH - LINE_GAP);
  
  clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);

  ClutterColor *c = type_colour(var->applied_as);
  if (c == NULL) {
    c = &white;
  }
  cairo_set_source_rgb(cr, 
                       (float) c->red / 255.0f, 
                       (float) c->green / 255.0f, 
                       (float) c->blue / 255.0f
                       );
  cairo_arc(cr,
            x, y,
            LINE_WIDTH - LINE_GAP,
            0,
            M_PI * 2
            );
  cairo_stroke(cr);
}

/**/

void draw_input_node (cairo_t *cr, t_var *var, float angle) {
  t_var *tmp = var;
  int n = 0;
  gfloat x, y, width, height;
  gfloat offset = (M_PI / 4);

  if (! var->text) {
    fprintf(stderr, "no text to draw args of\n");
    return;
  }

  cairo_set_line_width (cr, LINE_WIDTH - LINE_GAP);
  
  clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);
  clutter_actor_get_size(CLUTTER_ACTOR(var->text), &width, &height);
  x += width; 
  y += height;

  while (tmp->typetag == T_FUNCTION) {
    ClutterColor *c;
    if (tmp->f.input->typetag == T_FUNCTION) {
      c = type_colour(tmp->f.input->applied_as);
    }
    else {
      c = type_colour(tmp->f.input);
    }
    
    if (c == NULL && tmp->f.input->applied_as->typetag == T_WILDCARD) {
      int cs = 0;
      while (tmp->f.input->applied_as->wildcard[cs] != NULL) {
        ++cs;
      }

      float r = (2 * M_PI) / (float) cs;
      for (int i = 0; i < cs; ++i) {
        c = type_colour(tmp->f.input->applied_as->wildcard[i]);
        if (c == NULL) {
          fprintf(stderr, "really shouldn't happen..\n");
          c = &white;
        }

        cairo_set_source_rgb(cr, 
                             (float) c->red / 255.0f, 
                             (float) c->green / 255.0f, 
                             (float) c->blue / 255.0f
                             );
        cairo_arc(cr,
                  x, y,
                  (n * LINE_WIDTH) + (float) LINE_WIDTH / 2,
                  offset + r * (float) i,
                  offset + r * (float) (i + 1)
                  );
        cairo_stroke(cr);
      }
    }
    else {
      float angle0, angle1;
      if (var->f.input->assigned) {
        angle0 = 0 - angle + M_PI;
        angle1 = 0 - angle + M_PI * 0.5;
      }
      else {
        angle0 = 0;
        angle1 = M_PI * 2;
      }
      
      if (c == NULL) {
        c = &white;
      }
      cairo_set_source_rgb(cr, 
                           (float) c->red / 255.0f, 
                           (float) c->green / 255.0f, 
                           (float) c->blue / 255.0f
                           );
      cairo_arc(cr,
                x, y,
                (n * LINE_WIDTH) + (float) LINE_WIDTH / 2,
                angle0,
                angle1
                );
      cairo_stroke(cr);
    }
    
    n++;
    tmp = tmp->f.output;
  }
}

/**/

t_coords *draw_connect (cairo_t *cr, t_var *var, t_var *from, t_var *to, 
                        t_coords *next_coords,
                        int tail
                        ) {
  gfloat x, y, x2, y2;
  float angle;
  float opp, adj, hyp, off_x, off_y;
  t_var *ptr = var;
  t_coords *coords = (t_coords *) malloc(sizeof(t_coords));
  coords->n = 0;
  int n = 0;
  int total_lines = 0;
  x = y = x2 = y2 = 0;
  hyp = 0;
  
  clutter_actor_get_position(CLUTTER_ACTOR(from->text), &x, &y);
  if (tail) {
    gfloat width, height;
    clutter_actor_get_size(CLUTTER_ACTOR(from->text), &width, &height);
    x += width; 
    y += height;
  }

  clutter_actor_get_position(CLUTTER_ACTOR(to->text), &x2, &y2);
  angle = atan2(x2 - x, y2 - y) + (M_PI / 2);

  opp = sin(angle);
  adj = cos(angle);

  draw_output_node(cr, to);
  if (var == from) {
    draw_input_node(cr, from, angle);
  }

  while (ptr->typetag == T_FUNCTION) {
    total_lines++;
    ptr = ptr->f.output;
  }
  ptr = var;
  
  while (ptr->typetag == T_FUNCTION) {
    ClutterColor *colour = NULL;
#ifndef MONOCHROME
    if (ptr->f.input->typetag == T_FUNCTION) {
      colour = type_colour(ptr->f.input->applied_as);
    }
    else {
      colour = type_colour(ptr->f.input);
    }
    if (colour == NULL) {
      colour = &white;
    }
#else
    if (colour == NULL) {
      colour = &darkgrey;
    }
#endif

    cairo_set_line_width (cr, LINE_WIDTH - LINE_GAP);
    cairo_set_source_rgba(cr, 
                          (float) colour->red / 255.0f, 
                          (float) colour->green / 255.0f, 
                          (float) colour->blue / 255.0f, 
                          (float) colour->alpha / 255.0f
                          );
    
    off_x = opp * hyp;
    off_y = adj * hyp;
    
    // join lines together with a nice arc
    if (next_coords != NULL && n > 0) {
      float cx = x2;
      float cy = y2;
      float ax = x2 + off_x;
      float ay = y2 + off_y;
      float bx = next_coords->coords[n - 1].x;
      float by = next_coords->coords[n - 1].y;
      
      cairo_arc_negative(cr,
                         cx, cy,
                         hyp,
                         0 - atan2(cx - bx, cy - by) - (M_PI / 2),
                         0 - atan2(cx - ax, cy - ay) - (M_PI / 2)
                         );
    }
    else {
      cairo_move_to(cr, x2 + off_x, y2 + off_y);
    }
    
    gfloat toX = x + off_x + (opp * LINE_WIDTH);
    gfloat toY = y + off_y + (adj * LINE_WIDTH);

    if (tail) {
      toX -= opp * ((float) LINE_WIDTH / 2.0);
      toY -= adj * ((float) LINE_WIDTH / 2.0);
    }

    cairo_line_to(cr, 
                  toX, toY
                  );

    coords->coords[n].x = toX;
    coords->coords[n].y = toY;
    coords->n = n + 1;

    cairo_stroke(cr);
    
    
    hyp += LINE_WIDTH;

    ptr = ptr->f.output;
    ++n;
  }

  return(coords);
}

/**/

void draw_list_var (cairo_t *cr, t_var *var) {
  if (var->assigned) {
    float x, y;
#ifdef MONOCHROME
    ClutterColor *c = &darkgrey;
#else
    ClutterColor *c = type_colour(var);
#endif
    cairo_set_source_rgb(cr, 
                         (float) c->red / 255.0f, 
                         (float) c->green / 255.0f, 
                         (float) c->blue / 255.0f
                         );

    cairo_set_line_width (cr, LINE_WIDTH - LINE_GAP);
    
    clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);
    cairo_move_to(cr, x, y);
    
    while(var->list_next != NULL) {
      var = var->list_next;
      clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);
      cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
  }
}

/**/

void border_text(cairo_t *cr, t_var *var) {
  if (var->text != editing) {
    cairo_set_line_width(cr, LINE_WIDTH - LINE_GAP);
    gfloat x, y, width, height;
    clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);
    clutter_actor_get_size(CLUTTER_ACTOR(var->text), &width, &height);
    
    cairo_rectangle(cr, x, y, width, height);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_fill(cr);
  }
}

/**/

t_coords *draw_var_lines (cairo_t *cr, t_var *var);

void draw_head_var (cairo_t *cr, t_var *var) {
  t_coords *coords = draw_var_lines(cr, var);
  //border_text(cr, var);
  if (var->f.input->assigned) {
    t_coords *coords2 = draw_connect(cr, var, var, var->f.input, coords, 1);
    if (coords2 != NULL) {
      free(coords2);
    }
  }
  else {
    draw_input_node(cr, var, 0);
  }
  if (coords != NULL) {
    free(coords);
  }
}


/**/

t_coords *draw_var_lines (cairo_t *cr, t_var *var) {
  t_coords *result = NULL;
  
  if (var->typetag == T_LIST_START && var->list_next != NULL) {
    draw_list_var(cr, var);
  }
  else {
    if (var->typetag == T_FUNCTION) {
      t_var *output = var->f.output;
      t_var *from = var->f.input;
      t_var *to = var->f.output->f.input;
      t_coords *next_coords = NULL;
    
      if (from->typetag == T_FUNCTION) {
        draw_head_var(cr, from);
      }
      else {
        if (from->typetag == T_LIST_START) {
          draw_list_var(cr, from);
        }
      }
      
      if (to != NULL) {
        if (output->typetag == T_FUNCTION) {
          next_coords = draw_var_lines(cr, output);
          if (from->assigned && to->assigned) {
            // draw lines from param of var to param of output
            // return coords of param side
            // connect lines on output side
            result = draw_connect(cr, output, from, to, next_coords, 0);
          }
          if (next_coords != NULL) {
            free(next_coords);
          }
        }
      }
    }
  }

  return(result);
}

/**/

t_var *text_to_curried(ClutterActor *text) {
  t_var *var = NULL;
  for (int i = 0; i < curried_n; ++i) {
    if ((void *) curried[i].text == (void *) text) {
      var = &curried[i];
      break;
    }
  }
  return(var);
}

/**/

void draw_lines (int send_to_haskell) {
  cairo_t *cr;
  
  cairo_surface_t *surface;

  if (save) {
    ClutterColor background;
    clutter_stage_get_color (CLUTTER_STAGE(stage), &background);
    fprintf(stderr, "saving\n");
    surface = cairo_svg_surface_create(svg_filename(), WIDTH, HEIGHT);
    cr = cairo_create(surface);
    cairo_rectangle(cr, 0.0, 0.0, WIDTH, HEIGHT);
    cairo_set_source_rgb(cr, 
                         (float) background.red / 255.0f, 
                         (float) background.green / 255.0f, 
                         (float) background.blue / 255.0f
                         );
    cairo_fill(cr);
  }
  else {
    cr  = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE (ctex));
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
  }

  cairo_set_tolerance (cr, 0.1);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgb(cr, 1, 1, 1);

  for (int i = 0; i < curried_n; i++) {
    t_var *var = &curried[i];
    if (var->output_from == NULL && var->input_to == NULL) {
      if (var->typetag == T_FUNCTION) {
        if (send_to_haskell) {
          printf("%s\n", show_function(var));
          fflush(stdout);
        }
        //fprintf(stderr, "%s\n", show_function(var));
        //fflush(stderr);
        draw_head_var(cr, var);
      }
      else {
        if (var->typetag == T_LIST_START) {
          // no need to print out lone lists..
          //if (send_to_haskell) {
          //  printf("%s\n", show_function(var));
          //  fflush(stdout);
          //}
          draw_list_var(cr, var);
        }
      }
    }
  }

  if (save) {
    float x, y;
    cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (cr, FONT_SIZE_CAIRO);
    
    for (int i = 0; i < words_n; ++i) {
      ClutterText *text = words[i];
      t_var *var = text_to_curried(CLUTTER_ACTOR(text));
      
      if (var == NULL) {
        fprintf(stderr, "arse %s\n", clutter_text_get_text(text));
      }
      else {

#ifdef MONOCHROME
        ClutterColor *c = &black;
#else
        ClutterColor *c = type_colour(var->applied_as);
        if (c == NULL) {
          c = &white;
        }
#endif
        
        cairo_set_source_rgb(cr, 
                             (float) c->red / 255.0f, 
                             (float) c->green / 255.0f, 
                             (float) c->blue / 255.0f
                             );
        
        clutter_actor_get_position(CLUTTER_ACTOR(text), &x, &y);
        cairo_move_to(cr, x, y + 17);
        cairo_show_text(cr, clutter_text_get_text(text));
      }
    }
    
    //cairo_show_page(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface); 
    save = 0;
  }
  else {
    cairo_destroy(cr);
  }
}

/**/

t_name *text_to_name (ClutterText *text) {
  char *str = (char *) clutter_text_get_text(text);
  t_name *name = find_name(global, str);
  if (name == NULL) {
    name = &local.names[local.n++];
    
    int typetag;
    int n = 0;
    if (str[0] == '\0') {
      typetag = T_STRING;
    }
    else {
      if (str[0] == '-') {
        // skip leading -
        n++;
      }
      typetag = T_INT;
      while (str[n] != '\0') {
        if (str[n] >= '0' && str[n] <= '9') {
          // do nothing
        }
        else {
          if (str[n] == '.' && typetag == T_INT) {
            typetag = T_REAL;
          }
          else {
            typetag = T_STRING;
            break;
          }
        }
        n++;
      }
    }

    if (typetag == T_REAL) {
      float n;
      sscanf(str, "%f", &n);
      name->var = r();
      name->var->r = n;
    }
    else {
      if (typetag == T_INT) {
        int n;
        sscanf(str, "%d", &n);
        name->var = i();
        name->var->i = n;
      }
      else {
        name->var = s();
        strncpy(name->var->s, str, MAXLEN);
      }
    }

    strncpy(name->name, str, MAXLEN);
    name->var->assigned = 1;
  }
  return(name);
}

/**/

void start_curry () {
  curried_n = 0;
  local.n = 0;
  t_wildcard lookup_from, lookup_to;

  for (int i = 0; i < words_n; ++i) {
    ClutterText *text = words[i];
    t_name *name = text_to_name(text);
    t_var *var = &curried[curried_n++];
    
    memcpy((void *) var, (void *) name->var, sizeof(t_var));
    if (var->typetag == T_FUNCTION) {
      lookup_to[0] = lookup_from[0] = NULL;
      
      var->f.input = dup_var(var->f.input, lookup_from, lookup_to);
      if (var->f.input->typetag != T_WILDCARD) {
        var->f.input->input_to = var;
      }

      var->f.output = dup_var(var->f.output, lookup_from, lookup_to);
      if (var->f.output->typetag != T_WILDCARD) {
        var->f.output->output_from = var;
      }
    }
    else {
      if (var->typetag == T_LIST_START) {
        lookup_to[0] = lookup_from[0] = NULL;
        var->list_type = dup_var(var->list_type, lookup_from, lookup_to);
      }
    }
    
    var->text = text;
    var->applied_as = var;
    clutter_actor_get_position(CLUTTER_ACTOR(text), &var->x, &var->y);
    clutter_actor_get_size(CLUTTER_ACTOR(text), &var->width, &var->height);
    var->applied_x = var->x;
    var->applied_y = var->y;
    var->applied_width = var->width;
    var->applied_height = var->height;
  }
}

/**/

int text_selected(ClutterText *text) {
  for(int i = 0; i < selected_words_n; ++i) {
    if (selected_words[i] == text) {
      return(TRUE);
    }
  }
  return(FALSE);
}

/**/

int var_selected(t_var *var) {
  return(text_selected(var->text));
}

/**/

void colour_text () {
  for (int i = 0; i < curried_n; i++) {
    t_var *var = &curried[i];
#ifdef MONOCHROME
    ClutterColor *c = &black;
#else
    ClutterColor *c = type_colour(var->applied_as);
    if (c == NULL) {
      c = &white;
    }
    if (selected_words_n && var_selected(var)) {
      c = &white;
    }
#endif
    clutter_text_set_color(var->text, c);
  }
}


/**/

int valigned (t_var *a, t_var *b) {
  return((a->y <= (b->y + b->height) && a->y >= b->y)
         || (b->y <= (a->y + a->height) && b->y >= a->y)
         );
}

/**/

void find_lists (void) {
  typedef struct {
    t_var *var, *to;
    float distance;
  } t_match;

  t_match matches[MAXNAMES];

  int match_n = 0;
  
  // find start and end of lists
  for (int i = 0; i < curried_n; ++i) {
    t_var *a = &curried[i];
    if (a->typetag == T_LIST_START) {
      t_match *match = NULL;
      for (int j = 0; j < curried_n; ++j) {
        t_var *b = &curried[j];
        if (b->typetag == T_LIST_END && b->output_from == NULL) {
          if ((b->x > a->x) && valigned(a, b)) {
            // if there's more than hit, overwrite with closest
            if (match == NULL || (match->distance > (b->x - a->x))) {
              if (match == NULL) {
                match = &matches[match_n++];
              }
              match->var = a;
              match->to = b;
              match->distance = b->x - a->x;
            }
          }
        }
      }
      if (match == NULL) {
        // no partner, ignore it
        a->typetag = T_NOLIST;
      }
      else {
        match->to->output_from = match->var;
        match->var->list_next = match->to;
      }
    }
  }

  // find things inside each list
  while(match_n > 0) {
    int lowest = 0;
    t_var *left, *right, *item;
    t_var *list_prev = NULL;
    t_match items[MAXNAMES];
    int item_n = 0;

    // start with smallest list first
    for (int i = 1; i < match_n; ++i) {
      if (matches[i].distance < matches[lowest].distance) {
        lowest = i;
      }
    }
    
    // T_LIST_START
    left = matches[lowest].var;
    assert(left->typetag == T_LIST_START);
    left->assigned = 1;

    // T_LIST_END
    right = left->list_next;
    assert(right->typetag == T_LIST_END);
    right->assigned = 1;

    // find things inside the list
    for (int i = 0; i < curried_n; ++i) {
      item = &curried[i];
      
      if (item->output_from != NULL) {
        continue;
      }
      if (item == left) {
        continue;
      }

      // TODO assigned_as
      
      // only allow these kinds of lists for now
      if (item->typetag != T_REAL 
          && item->typetag != T_INT 
          && item->typetag != T_STRING
          && item->typetag != T_NOTHING) {
        continue;
      }


      /*
        list_type always a wildcard at this stage, right?
        if (type_eq(left->list_type, item, -1) == 0) {
        continue;
        }
      */
      // inside check 
      if (valigned(item, left) && valigned(item, right)) {
        
        if (item->x > left->x && item->x < right->x) {
          t_match *match = &items[item_n++];
          match->var = item;
          match->distance = item->x - left->x;
        }
      }
    }

    // link in things in the list
    list_prev = NULL;

    while (item_n > 0) {
      int closest = 0;
      t_var *var;
      
      // find leftmost one not yet added to list
      for (int i = 1; i < item_n; ++i) {
        if (items[i].distance < items[closest].distance) {
          closest = i;
        }
      }
      var = items[closest].var;
        
      if (var->typetag != T_NOTHING && left->list_type->typetag == T_WILDCARD) {
        left->list_type = var;
      }

      // no mixed types (leftmost item gives the type)
      if (var->typetag == T_NOTHING || type_eq(left->list_type, var, -1)) {
        var->output_from = left;
        var->list_next = right;
        var->assigned = 1;
        if (list_prev) {
          list_prev->list_next = var;
        }
        else {
          left->list_next = var;
        }
        list_prev = var;
      }
      
      // wipe out the leftmost one with the last one
      items[closest].var = items[item_n - 1].var;
      items[closest].distance = items[item_n - 1].distance;
      item_n--;
    }
    
    // wipe out the smallest one with the last one
    matches[lowest].var = matches[match_n - 1].var;
    matches[lowest].distance = matches[match_n - 1].distance;
    match_n--;
  }
}

/**/

void curry (void) {
  static int curry_id = 1;
  reset_mempool();

  start_curry();
  
  find_lists();

  int done = 0;
  int n = curried_n;
  int edge_n = 0;
  
  while (!done) {
    done = 1; // optimism
    t_var *closest_from = NULL;
    t_var *closest_to = NULL;
    float closest_distance = -1;
    
    for (int i = 0; i < n; ++i) {
      if (curried[i].output_from != NULL || curried[i].input_to != NULL) {
        continue;
      }
      t_var *a = curried[i].applied_as;

      if (a->typetag != T_FUNCTION) {
        continue;
      }
      
      for (int j = 0; j < n; ++j) {
        if (i == j) {
          continue;
        }
        if (curried[j].output_from != NULL || curried[j].input_to != NULL) {
          continue;
        }
        t_var *b = curried[j].applied_as;
        if (type_eq(a->f.input, b, curry_id++)) {
          done = 0;

          float distance = 
            sqrt(sqr(curried[i].applied_x + curried[i].applied_width - curried[j].x) 
                 + sqr(curried[i].applied_y - curried[j].y)
                 );
          if (closest_distance < 0 || distance < closest_distance) {
            closest_distance = distance;
            closest_from = &curried[i];
            closest_to = &curried[j];
          }
        }
      }
    }

    if (!done) {
      //      gchar *str =  clutter_text_get_text(closest_from->text);
      //      gchar *str2 =  clutter_text_get_text(closest_to->text);
      
      // resolve any wildcards
      assert(type_eq(closest_from->applied_as->f.input, 
                     closest_to->applied_as, curry_id++));
      if (!type_eq(closest_from->applied_as->f.input, 
                   closest_to->applied_as, 0)) {
        fprintf(stderr, "-- footprints in the snow\n");
      }
      
      closest_from->applied_as->f.input = closest_to;
      closest_from->applied_as->assigned = 1;
      closest_to->assigned = 1;
      closest_to->input_to = closest_from->applied_as;

      //printf("well, %s, as ", show_type(closest_from));
      //printf("%s ", show_type(closest_from->applied_as));

      closest_from->applied_as = applied_type(closest_from->applied_as);
      
      closest_from->applied_x = closest_to->x;
      closest_from->applied_y = closest_to->y;
      closest_from->applied_width = closest_to->width;
      closest_from->applied_height = closest_to->height;

      //printf("becomes %s\n", show_type(closest_from->applied_as));
      
      edge_n++;
      if (edge_n >= MAXEDGES) {
        fprintf(stderr, "-- max edges hit!\n");
        done = 1;
      }
    }
  }
  draw_lines(1);
  colour_text();
  return;
}

/**/

void add_word (ClutterText *text) {
    words[words_n++] = text;
}

/**/

void delete_word (ClutterText *text) {
  for (int i = 0; i < words_n; ++i) {
    if (words[i] == text) {
      words[i] = words[words_n - 1];
      words_n--;
    }
  }
}

/**/

void edit (ClutterText *text) {
  if (editing != NULL) {
    clutter_text_set_cursor_visible(editing, FALSE);
  }
  if (text != NULL) {
    clutter_text_set_cursor_visible(text, TRUE);
    clutter_actor_grab_key_focus(CLUTTER_ACTOR(text));
  }
  editing = text;
}

/**/

void move_cursor (gfloat x, gfloat y) {
  clutter_actor_set_position(CLUTTER_ACTOR(cursor), 
                             x, y
                             );
  edit(NULL);
}

/**/

void move_cursor_by(gfloat dx, gfloat dy) {
  gfloat x, y;
  clutter_actor_get_position(CLUTTER_ACTOR(cursor), 
                             &x, &y
                             );
  move_cursor(x + dx, y + dy);
}

/**/

void reset_cursor () {
  clutter_actor_set_size(cursor, CURSOR_WIDTH, CURSOR_HEIGHT);
  clutter_stage_set_key_focus(CLUTTER_STAGE(stage), NULL);
  clutter_actor_show(cursor);
}

/**/

void hide_cursor() {
  clutter_actor_hide(cursor);
}

/**/

void cursor_jiggle () {
  gfloat x, y, width, height;
  int pad = 4;

  if (editing != NULL) {
    clutter_actor_get_size(CLUTTER_ACTOR(editing), &width, &height);
    clutter_actor_get_position(CLUTTER_ACTOR(editing), &x, &y);
    
    clutter_actor_set_size(cursor, width + pad * 2, height + pad * 2);
    clutter_actor_set_position(cursor, x - pad, y - pad);
  }
}

/**/

void move_children(t_var *var, gfloat dx, gfloat dy) {
  if (var == NULL) {
    return;
  }
  if (var->typetag == T_FUNCTION) {
    move_children(var->f.input, dx, dy);
    move_children(var->f.output, dx, dy);
  }
  else if (var->typetag == T_LIST_START) {
    t_var *tmp = var->list_next;
    while (tmp != NULL) {
      move_children(tmp, dx, dy);
      tmp = tmp->list_next;
    }
  }

  if (var->text != NULL && (selected_words_n == 0 || (!var_selected(var)))) {
    gfloat x, y;
    clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);
    clutter_actor_set_position(CLUTTER_ACTOR(var->text), 
                               x + dx, 
                               y + dy
                               );
    log_move(var->text);
  }
}

void select_words(float x, float y, float x2, float y2) {
  gfloat tx, ty, tw, th, tx2, ty2;

  selected_words_n = 0;
  
  for (int i = 0; i < curried_n; i++) {
    clutter_actor_get_position(CLUTTER_ACTOR(curried[i].text), &tx, &ty);
    clutter_actor_get_size(CLUTTER_ACTOR(curried[i].text), &tw, &th);
    tx2 = tx + tw;
    ty2 = ty + th;
    if (
        ((tx > x && tx < x2)
         || (tx2 > x && tx2 < x2)
         )
        && 
        ((ty > y && ty < y2)
         || (ty2 > y && ty2 < y2)
         )
        ) {
      //fprintf(stderr, "selected %s\n", clutter_text_get_text(curried[i].text));
      selected_words[selected_words_n++] = curried[i].text;
    }
  }
  colour_text();
}


void var_move_by(t_var *var, gfloat dx, gfloat dy) {
  gfloat x, y;
  clutter_actor_get_position(CLUTTER_ACTOR(var->text), &x, &y);
  clutter_actor_set_position(CLUTTER_ACTOR(var->text), 
                             x + dx,
                             y + dy
                             );
}


static gboolean on_stage_motion (ClutterActor *stage, 
                                 ClutterEvent *event, 
                                 gpointer data) {
  
  clutter_event_get_coords(event, &mouse_x, &mouse_y);
  if (clutter_event_get_state(event) & CLUTTER_BUTTON1_MASK) {
    // drag
    if (text_drag && (editing || selected_words_n)) {
      gfloat x, y, dx, dy;
      
      clutter_actor_get_position(CLUTTER_ACTOR(editing), &x, &y);
      dx = (mouse_x + mouse_x_press) - x;
      dy = (mouse_y + mouse_y_press) - y;
      
      if (selected_words_n) {
        for (int i = 0; i < curried_n; ++i) {
          t_var *var = &curried[i];
          if (var_selected(var)) {
            var_move_by(var, dx, dy);
            if (text_drag_tree) {
              move_children(var, dx, dy);
            }
          }
        }
      }
      else {
        t_var *var = text_to_curried(CLUTTER_ACTOR(editing));
        if (text_drag_tree) {
          move_children(var, dx, dy);
        }
        else {
          var_move_by(var, dx, dy);
        }
        cursor_jiggle();
      }

      //log_move(editing);
      draw_lines(0);
    }
    else {
      if (editing) {
        edit(NULL);
        reset_cursor();
      }
      if (!stage_drag) {
        stage_drag = TRUE;
        stage_drag_x = mouse_x;
        stage_drag_y = mouse_y;
      }
      else {
        gfloat w = abs(stage_drag_x - mouse_x);
        gfloat h = abs(stage_drag_y - mouse_y);
        gfloat x = mouse_x < stage_drag_x ? mouse_x : stage_drag_x;
        gfloat y = mouse_y < stage_drag_y ? mouse_y : stage_drag_y;
        clutter_actor_set_position(cursor, x, y);
        clutter_actor_set_size(cursor, 
                               w < CURSOR_WIDTH ? CURSOR_WIDTH : w,
                               h < CURSOR_HEIGHT ? CURSOR_HEIGHT : h
                               );
        select_words(x, y, x + w, y + h);
      }
      //move_cursor(mouse_x - (CURSOR_WIDTH/2), mouse_y - (CURSOR_HEIGHT/2));
    }
  }
  else {
    if (stage_drag == TRUE) {
      stage_drag = FALSE;
      if (selected_words_n > 0) {
        hide_cursor();
      }
      else {
        reset_cursor();
      }
    }
  }
  return(TRUE);
}

/**/

static gboolean on_text_motion (ClutterText *text,
                                ClutterEvent *event,
                                gpointer data) {
  
  if (clutter_event_get_state(event) & CLUTTER_BUTTON1_MASK) {
    if (stage_drag == FALSE && text_drag == FALSE) {
      edit(text);
      cursor_jiggle();
      return(TRUE);
    }
  }
  
  return(FALSE);
}

/**/

static gboolean on_text_changed (ClutterText *text,
                                 gchar       *new_text,
                                 gint         new_text_length,
                                 gpointer     position,
                                 gpointer     user_data
                                 ) {
  int len = strlen(clutter_text_get_text(text));
  if (len == 0) {
    edit(NULL);
    reset_cursor();
    clutter_container_remove_actor(CLUTTER_CONTAINER(stage), 
                                   CLUTTER_ACTOR(text)
                                   );
    delete_word(text);
    log_delete(text);
    // todo free text?
  }
  else {
    log_edit(text);
    edit(text);
    cursor_jiggle();
  }
  curry();
  return(TRUE);
}

/**/

void shift_cursor () {
  gfloat x, y, width, height;
  
  if (editing == NULL) {
    clutter_actor_get_position(CLUTTER_ACTOR(cursor), &x, &y);
    move_cursor(x + SPACE_WIDTH, y);
  }
  else {
    clutter_text_activate(editing);
    clutter_actor_get_position(CLUTTER_ACTOR(editing), &x, &y);
    clutter_actor_get_size(CLUTTER_ACTOR(editing), &width, &height);
    reset_cursor();
    move_cursor(x + width + SPACE_WIDTH, y);
  }
}

/**/

void delete_selected() {
  for (int i = 0; i < curried_n; ++i) {
    t_var *var = &curried[i];
    if (var_selected(var)) {
      clutter_container_remove_actor(CLUTTER_CONTAINER(stage), 
                                     CLUTTER_ACTOR(var->text)
                                     );
      delete_word(var->text);
    }
  }
  curry();
  reset_cursor();
}

/**/

static gboolean on_captured_event (ClutterActor *actor,
                                   ClutterEvent *event,
                                   gpointer user_data
                                   ) {
  ClutterEventType type = clutter_event_type(event);
  if (type == CLUTTER_KEY_PRESS) {
    guint k = clutter_event_get_key_symbol(event);
    switch (k) {
    case ' ':
      break_word();
      return(TRUE);
    case 65288:
    case 65535:
      if (selected_words_n > 0) {
        delete_selected();
      }
      break;
    case 65361:
      // left
      break;
    case 65362:
      // up
      break;
    case 65363:
      // right
      break;
    case 65364:
      // down
      break;
    default:
      // fprintf(stderr, "key: %d\n", k);
      break;
    }
  }
  return(FALSE);
}

/**/

static gboolean on_insert_text (ClutterText *self,
                                gchar       *new_text,
                                gint         new_text_length,
                                gpointer     position,
                                gpointer     user_data) {
  return(TRUE);
}

/**/

static gboolean on_stage_button_press (ClutterActor *stage, 
                                       ClutterEvent *event, 
                                       gpointer data) {
  text_drag = FALSE;
  if (selected_words_n > 0) {
    selected_words_n = 0;
    colour_text();
    reset_cursor();
  }
  clutter_event_get_coords(event, &mouse_x, &mouse_y);
  move_cursor(mouse_x - (CURSOR_WIDTH/2), mouse_y - (CURSOR_HEIGHT/2));
  reset_cursor();

  return(TRUE);
}

/**/

static gboolean on_text_button_press (ClutterText *text, 
                                      ClutterEvent *event, 
                                      gpointer data) {
  gfloat x, y;
  gboolean result = TRUE;

  ClutterModifierType state = clutter_event_get_state (event);
  gboolean shift_pressed = (state & CLUTTER_SHIFT_MASK ? TRUE : FALSE);
  gboolean control_pressed = (state & CLUTTER_CONTROL_MASK ? TRUE : FALSE);

  clutter_event_get_coords(event, &mouse_x, &mouse_y);
  clutter_actor_get_position(CLUTTER_ACTOR(text), &x, &y);
  
  //fprintf(stderr, "pressed %s\n", clutter_text_get_text(text));
  mouse_x_press = x - mouse_x;
  mouse_y_press = y - mouse_y;

  if (shift_pressed || control_pressed) {
    text_drag = TRUE;
    text_drag_tree = control_pressed;

    if (selected_words_n > 1 && !text_selected(text)) {
      selected_words_n = 0;
      colour_text();
      reset_cursor();
    }
  }
  else {
    result = FALSE;
    
    if (selected_words_n > 0) {
      selected_words_n = 0;
      colour_text();
      reset_cursor();
    }
  }

  if (editing != text) {
    edit(text);
    cursor_jiggle();
  }

  return(result);
}

/**/

ClutterText *text_new () {
#ifdef MONOCHROME
  ClutterText *text = 
    CLUTTER_TEXT(clutter_text_new_full("Sans " FONT_SIZE_CLUTTER, "", &black));
#else
  ClutterText *text = 
    CLUTTER_TEXT(clutter_text_new_full("Sans " FONT_SIZE_CLUTTER, "", &white));
#endif
  clutter_actor_set_reactive(CLUTTER_ACTOR(text), TRUE);
  clutter_text_set_editable(text, TRUE);
  clutter_text_set_activatable(text, TRUE);
  clutter_container_add_actor(CLUTTER_CONTAINER(stage), CLUTTER_ACTOR(text));
  clutter_text_set_cursor_color(text, &grey);

  g_signal_connect(CLUTTER_ACTOR(text), 
                   "insert-text", 
                   G_CALLBACK(on_insert_text), 
                   NULL);

  g_signal_connect(CLUTTER_ACTOR(text), 
                   "text-changed", 
                   G_CALLBACK(on_text_changed), 
                   NULL);

  g_signal_connect(CLUTTER_ACTOR(text), 
                   "captured-event", 
                   G_CALLBACK(on_captured_event), 
                   NULL);
  g_signal_connect(CLUTTER_ACTOR(text), 
                   "button-press-event", 
                   G_CALLBACK(on_text_button_press), 
                   NULL);
  g_signal_connect(CLUTTER_ACTOR(text), 
                   "motion-event", 
                   G_CALLBACK(on_text_motion), 
                   NULL);
  
  log_create(text);

  return(text);
}

/**/

void break_word () {
  gfloat x, y, width, height;
  gchar left_str[MAX_CHARS];
  gchar right_str[MAX_CHARS];
  
  if (editing == NULL) {
    shift_cursor();
    return;
  }

  const gchar *str = clutter_text_get_text(editing);
  gint pos = clutter_text_get_cursor_position(editing);

  //printf("pos: %d, len: %d\n", pos, strlen(str));
  if (pos == 0) {
    // start of word
    clutter_actor_get_position(CLUTTER_ACTOR(editing), &x, &y);
    clutter_actor_set_position(CLUTTER_ACTOR(editing), x + SPACE_WIDTH, y);
    cursor_jiggle();
    return;
  }
  
  if (pos == -1 || pos == strlen(str)) {
    // end of word
    shift_cursor();
    return;
  }

  // OK, break the word
  strncpy(left_str, str, pos);
  strcpy(right_str, str + pos);

  ClutterText *text = text_new();
  add_word(text);

  clutter_text_set_text(editing, left_str);

  clutter_actor_get_position(CLUTTER_ACTOR(editing), &x, &y);
  clutter_actor_get_size(CLUTTER_ACTOR(editing), &width, &height);

  clutter_actor_set_position(CLUTTER_ACTOR(text), x + width + SPACE_WIDTH, y);
  clutter_text_set_text(text, right_str);
  clutter_text_set_cursor_position(text, 0);

  edit(text);
}

/**/

void key (gunichar c) {
  if (editing == NULL) {
    //printf("new text with '%c' (%d)\n", c, c);
    if (words_n >= MAXWORDS) {
      return;
    }
    ClutterText *text = text_new();
    add_word(text);
    edit(text);
    gfloat x, y;
    clutter_actor_get_position(CLUTTER_ACTOR(cursor), &x, &y);
    clutter_actor_set_position(CLUTTER_ACTOR(text), x, y);
    clutter_actor_grab_key_focus(CLUTTER_ACTOR(text));
    clutter_text_insert_unichar(text, c);
    log_move(text);
  }
  else {
    fprintf(stderr, "-- oops, can't happen?\n");
  }
}

/**/

static gboolean on_stage_button_release (ClutterActor *group, 
                                         ClutterEvent *event, 
                                         gpointer data) {
  if (text_drag) {
    log_release(editing);
    curry();
  }
  return(TRUE);
}

/**/

static gboolean on_stage_key_press (ClutterActor *stage, 
                                    ClutterEvent *event, 
                                    gpointer data) {
  ClutterModifierType state = clutter_event_get_state(event);
  gboolean control_pressed = (state & CLUTTER_CONTROL_MASK ? TRUE : FALSE);
  gunichar key_unichar = clutter_event_get_key_unicode ((ClutterEvent *) event);
  char *spesh = "[]+-*/~<>";

  if (control_pressed) {
    //printf("pressed: %d\n", key_unichar);
    switch (key_unichar) {
      case 2:
        //toggle_background();
        break;
      case 19:
        save = 1;
        draw_lines(0);
        break;
    }
  }
  else {
    if (key_unichar != 0) {
      if ((key_unichar >= 'a' && key_unichar <= 'z')
          || (key_unichar >= '0' && key_unichar <= '9')
          || (index(spesh, key_unichar) != NULL)
          ) {
        key(key_unichar);
        return TRUE;
      }
    }
  }

  return(FALSE);
}

/**/

void init_cursor () {
  ClutterColor seethrough = {0, 0, 0, 0};
  cursor = clutter_rectangle_new_with_color(&seethrough);
  clutter_actor_set_position(cursor, WIDTH/2, HEIGHT/2);
  reset_cursor();
#ifdef MONOCHROME
  clutter_rectangle_set_border_color(CLUTTER_RECTANGLE(cursor), &black);
#else
  clutter_rectangle_set_border_color(CLUTTER_RECTANGLE(cursor), &white);
#endif
  clutter_rectangle_set_border_width(CLUTTER_RECTANGLE(cursor), 2);
  clutter_container_add_actor(CLUTTER_CONTAINER(stage), cursor);
}

/**/

void init_texture () {
  ctex = clutter_cairo_texture_new(WIDTH, HEIGHT);
  cairo_t *cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE (ctex));

  // TODO what's all this?
  cairo_set_tolerance (cr, 0.1);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba(cr, 1, 1, 1, 1);
  cairo_destroy(cr);
  clutter_container_add_actor(CLUTTER_CONTAINER(stage),
                              ctex
                              );
  clutter_actor_set_position(ctex, 0, 0);
}

/**/

void init_stage () {
#ifdef MONOCHROME
  ClutterColor *background = &white;
#else
  ClutterColor *background = &black;
#endif
  stage = clutter_stage_get_default();
  clutter_actor_set_size (stage, WIDTH, HEIGHT);
  clutter_stage_set_color (CLUTTER_STAGE(stage), background);
  clutter_stage_set_title(CLUTTER_STAGE(stage), "Edit");
  g_signal_connect(stage, "button-press-event", 
                   G_CALLBACK(on_stage_button_press), NULL);
  g_signal_connect(CLUTTER_ACTOR(stage), 
                   "button-release-event", 
                   G_CALLBACK(on_stage_button_release), 
                   NULL);
  g_signal_connect(CLUTTER_ACTOR(stage), 
                   "key-press-event", 
                   G_CALLBACK(on_stage_key_press), 
                   NULL);
  g_signal_connect(CLUTTER_ACTOR(stage), 
                   "motion-event", 
                   G_CALLBACK(on_stage_motion), 
                   NULL);
  g_signal_connect(CLUTTER_ACTOR(stage), 
                   "captured-event", 
                   G_CALLBACK(on_captured_event), 
                   NULL);
}

/**/

ClutterText *playback_lookup(unsigned int id) {
  ClutterText *result = NULL;
  for (int i = 0; i < playback_map_n; ++i) {
    if (playback_map[i].id == id) {
      result = playback_map[i].text;
      break;
    }
  }
  return(result);
}

/**/

void playback_lookup_add(unsigned int id, ClutterText *text) {
  playback_map[playback_map_n].id = id;
  playback_map[playback_map_n].text = text;
  ++playback_map_n;
}

void playback_lookup_delete(unsigned int id) {
  for (int i = 0; i < playback_map_n; ++i) {
    if (playback_map[i].id == id) {
      playback_map[i].id = playback_map[playback_map_n - 1].id;
      playback_map[i].text = playback_map[playback_map_n - 1].text;
      break;
    }
  }
  --playback_map_n;
}

/**/

void playback_hello(t_log_entry *entry) {
  ClutterText *text = text_new();
  add_word(text);
  edit(text);
  playback_lookup_add(entry->id, text);
}

/**/

void playback_move(t_log_entry *entry) {
  ClutterText *text = playback_lookup(entry->id);
  clutter_actor_set_position(CLUTTER_ACTOR(text), 
                             entry->x, entry->y
                             );
}

/**/

void playback_edit(t_log_entry *entry) {
  ClutterText *text = playback_lookup(entry->id);
  clutter_text_set_text(text, entry->text);
}

/**/

void playback_release(t_log_entry *entry) {
  ClutterText *text = playback_lookup(entry->id);
  // todo?
}

/**/

void playback_bye(t_log_entry *entry) {
  ClutterText *text = playback_lookup(entry->id);
  playback_lookup_delete(entry->id);
  clutter_container_remove_actor(CLUTTER_CONTAINER(stage), 
                                 CLUTTER_ACTOR(text)
                                 );
  delete_word(text);
}

/**/

static gboolean playback_thread (gpointer data) {
  t_log_entry *entry;

  double t = log_now();
  while ((entry = log_next(t)) != NULL) {
    printf("entry %d, %s\n", entry->id, entry->command);
    switch (entry->command[0]) {
    case 'h': // hello
      playback_hello(entry);
      break;
    case 'm': // move
      playback_move(entry);
      break;
    case 'e': // edit
      playback_edit(entry);
      break;
    case 'r': // release
      playback_release(entry);
      break;
    case 'b': // bye
      playback_bye(entry);
      break;
    }
  }
  
  return(TRUE);
}


/**/

int main (int argc, char *argv[]) {
/*  int readlog = argc > 1;
  if (readlog) {
    log_open_read(argv[1]);
  }
  else {
*/
    log_open_write();
//  }

  if (argc >= 3) {
    WIDTH = atoi(argv[1]);
    HEIGHT = atoi(argv[2]);
    if (WIDTH <= 1) WIDTH = 1024;
    if (HEIGHT <= 1) HEIGHT = 768;
    printf("%sx%s %dx%d\n", argv[1], argv[2], WIDTH, HEIGHT);
  }

  global = get_namespace();
  local.n = 0;

  g_thread_init (NULL);
  clutter_threads_init ();
  printf("init\n");
  clutter_init (&argc, &argv);
  printf("initted\n");
  init_stage();
  init_texture();
  init_cursor();
  clutter_actor_show(stage);
  
  /*if (readlog) {
    clutter_threads_add_timeout (50,
                                 playback_thread,
                                 NULL);
  }
  */
  clutter_threads_enter ();
  clutter_main();
  clutter_threads_leave ();

  log_close();
  return EXIT_SUCCESS;
}

