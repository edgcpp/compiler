struct BF {
    int a : 4;
    int b : 8;
    int c : 20;
};
int read_b(struct BF* s) { return s->b; }
void write_b(struct BF* s, int v) { s->b = v; }
