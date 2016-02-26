#ifndef RVM
#define RVM

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

using namespace std;
typedef const char* rvm_t;
typedef int trans_t;

typedef struct struct_t{
char* address;
bool in_use;
int size;
}segment_t;

typedef struct struct_undo{
const char* segname;
int offset;
int size;
char* data;
}undo_t;

typedef struct redo_record
{
const char* segname;
int offset;
char* new_data;
}redo_t;

typedef map<const char*,segment_t> map_t;
typedef map<trans_t,rvm_t> map_trans;
typedef map<void*,const char*> map_seg;
typedef map<trans_t, vector<redo_t *> > redo_log_t;
typedef map<trans_t, vector<undo_t *> >undo_log_t;
typedef map<const char*,rvm_t> directory_t;
typedef map<trans_t,vector<const char*> > map_segment_t;

extern map_t segments;
extern map_trans rvmmap;
extern map_seg map_segname;
extern redo_log_t redo_log;
extern undo_log_t undo_log;
extern directory_t directories;
extern map_segment_t map_segment;
 
void truncate_segment(rvm_t rvm, const char* segname);

#endif
