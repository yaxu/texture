#include <clutter/clutter.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"

float time_offset;
FILE *logfh = NULL;
int log_readmode = 0;

t_log_entry *next = NULL;

double log_now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return(tv.tv_sec + (tv.tv_usec / 1000000.0));
}

t_log_entry *log_next(double t) {
  static t_log_entry result;
  if (next != NULL && ((t - time_offset) > next->t)) {
    result = *next;
    log_read();
  }
  else {
    return(NULL);
  }
  return(&result);
}

t_log_entry *log_read() {
  char buf[256];
  static t_log_entry result;

  if (fgets(buf, 256, logfh) != NULL) {
    printf("read %s\n", buf);
    sscanf(buf, "%lf %x %s %s", 
           &result.t,
           &result.id,
           result.command,
           result.text
           );
    if (strcmp(result.command, "move") == 0) {
      sscanf(result.text, "%fx%f",
             &result.x,
             &result.y
             );
    }
  }
  else {
    return(NULL);
  }
  return(&result);
}

void log_open_read(char *fn) {
  logfh = fopen(fn, "r");

  if (logfh == NULL) {
    printf("couldn't open log %s: %s\n", fn, strerror(errno));
    exit(1);
  }

  log_readmode = 1;
  next = log_read();
  time_offset = log_now() - next->t;
}

void log_open_write() {
  char fn[128];
  struct stat s;
  struct timeval tv;

  log_readmode = 0;
  
  if (stat("logs", &s) == -1) {
    printf("making log dir logs/\n");
    mkdir("logs", 0777);
  }
  else {
    if (!S_ISDIR(s.st_mode)) {
      fprintf(stderr, "logs should be a directory, seems to be a file\n");
    }
  }
  gettimeofday(&tv, NULL);
  snprintf(fn, 128, "logs/log-%d-%d.text",
           (int) tv.tv_sec,
           getpid()
           );
  logfh = fopen(fn, "w");
}

/**/

void log_close() {
  if (logfh != NULL) {
    fclose(logfh);
    logfh = NULL;
  }
}

/**/

void log_write_now() {
  if (logfh == NULL) {
    return;
  }
  fprintf(logfh, "%.6f ", log_now());
}

void log_create(ClutterText *text) {
  if (logfh == NULL) {
    return;
  }
  log_write_now();
  fprintf(logfh, "%x hello\n", (unsigned int) text);
  fflush(logfh);
}

void log_delete(ClutterText *text) {
  if (logfh == NULL) {
    return;
  }
  log_write_now();
  fprintf(logfh, "%x bye\n", (unsigned int) text);
  fflush(logfh);
}

void log_move(ClutterText *text) {
  gfloat x, y;
  if (logfh == NULL) {
    return;
  }
  clutter_actor_get_position(CLUTTER_ACTOR(text), &x, &y);
  log_write_now();
  fprintf(logfh, "%x move %.0fx%.0f\n", 
          (unsigned int) text, 
          x, y
          );
  fflush(logfh);
}

void log_edit(ClutterText *text) {
  if (logfh == NULL) {
    return;
  }
  log_write_now();
  fprintf(logfh, "%x edit %s\n", 
          (unsigned int) text, 
          clutter_text_get_text(text)
          );
  fflush(logfh);
}

void log_release(ClutterText *text) {
  if (logfh == NULL) {
    return;
  }
  log_write_now();
  fprintf(logfh, "%x release\n", 
          (unsigned int) text
          );
  fflush(logfh);
}

