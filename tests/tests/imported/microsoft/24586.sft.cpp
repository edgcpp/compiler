//options_all:--microsoft --xref xrefs
//filter:grep NUM_LOOP xrefs | sed 's/^0x.*	NUM_LOOP/NUM_LOOP/'
#define NUM_LOOP 20

int main() {
    for (int i = 0; i < NUM_LOOP; i++) {}
}
