typedef struct
  {
    char a;
    int b;
  } A;

extern A foo[] __attribute ((visibility ("protected")));

A* find(char a);
char a(const A*);
int b(const A*);
