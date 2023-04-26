//
// Created by daniel on 2020-03-22.
//

#ifndef NSH_GLIST_H
#define NSH_GLIST_H

#include <stdlib.h>

typedef struct GList{
	int len;
	unsigned capacity;
	char** array;
}GList;

struct GList *createListG();

void appendG(struct GList *list, char *value);

void *getValueAtIndexG(struct GList *list, int index);

int setValueAtIndexG(struct GList *list, int index, char *value);

int removeAtIndexG(struct GList *list, int index);

int addAtIndexG(struct GList *list, int index, char *value);


#endif //NSH_GLIST_H
