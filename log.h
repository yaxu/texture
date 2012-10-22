typedef struct {
  double t;
  unsigned int id;
  char command[16];
  char text[32];
  float x;
  float y;
} t_log_entry;

t_log_entry *log_read();

double log_now();
t_log_entry *log_next(double t);
void log_open_read(char *fn);
void log_open_write();
void log_close();
void log_write_now();
void log_create(ClutterText *text);
void log_delete(ClutterText *text);
void log_move(ClutterText *text);
void log_edit(ClutterText *text);
void log_release(ClutterText *text);
