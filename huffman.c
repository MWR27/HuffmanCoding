#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct huffnode {
    int c;
    uint64_t weight;
    struct huffnode *left;
    struct huffnode *right;
} huffnode;

typedef struct heap {
    huffnode *array[256];
    int size;
} heap;

huffnode huffman_tree;
char *table[256];

void build_tree(FILE *stream);
void build_table(huffnode *node, int index);
void write_encoding(FILE *in, FILE *out);
void write_tree(huffnode *node, FILE *file);
void decompress_file(FILE *compressed, FILE *newfile);
void rebuild_tree(FILE *stream, huffnode *node);
void push_minheap(heap *h, huffnode *item);
huffnode *pop_minheap(heap *h);
void write_int(uint64_t n, int size, FILE *stream);
uint64_t read_int(int size, FILE *stream);
void free_tree(huffnode *node);
char *create_file_name(char *filename, char *ext);
char *get_base_name(char *file_name);
char *get_extension(char *file_name);
void print_encoded_bits(FILE *stream);
void print_heap(heap *h);

int main(int argc, char *argv[]) {
    char *file_name;
    char mode;
    FILE *in, *out;
    if (argc == 3) {
        
        if (sscanf(argv[1], "-%c", &mode) != 1) {
            printf("Error: Unknown command \"%s\".", argv[1]);
            return -1;
        }
        //mode = (argv[1])[1];
        file_name = argv[2];
        printf("%c", mode);
        in = fopen(file_name, "rb");
        if (in == NULL) {
            printf("Error: %s does not exist.", file_name);
        }
        if (mode == 'c') {
            //printf(get_extension(file_name));
            
            build_tree(in);
            build_table(&huffman_tree, 0);
            char *base_name = get_base_name(file_name);
            char *out_name = create_file_name(base_name, "huffman");
            out = fopen(out_name, "wb");
            //free(base_name);
            //free(out_name);
            char *extension = get_extension(file_name);
            write_int(strlen(extension), 1, out);
            fputs(extension, out);
            free(extension);
            //putc('\0', out);
            write_tree(&huffman_tree, out);
            write_int(huffman_tree.weight, 6, out);
            rewind(in);
            write_encoding(in, out);
            fclose(in);
            fclose(out);
        } else if (mode == 'd') {
            char *base_name = get_base_name(file_name);
            char extension[256];
            int ext_size = read_int(1, in);
            fgets(extension, ext_size + 1, in);

            printf(extension);
            //char *out_file_name = malloc(strlen(base_name) + 1 + ext_size + 1);
            char *out_name = create_file_name(base_name, extension);
            out = fopen(out_name, "wb");
            free(base_name);
            free(out_name);

            decompress_file(in, out);
            fclose(in);
            fclose(out);
        }
    }
    printf("%d", fopen("yourmom", "r") == NULL);
    printf("done\n");

    return 0;
}

void build_tree(FILE *stream) {
    int c;
    uint64_t frequency_table[256] = {0};
    while ((c = getc(stream)) != EOF) {
        frequency_table[c]++;
    }

    heap priority_queue;
    priority_queue.size = 0;

    // initialize character nodes
    for (int i = 0; i < 256; i++) {
        if (frequency_table[i] > 0) {
            huffnode *item = (huffnode *) malloc(sizeof(huffnode));
            item->c = i;
            item->weight = frequency_table[i];
            item->left = NULL;
            item->right = NULL;
            push_minheap(&priority_queue, item);
        }
    }

    while (priority_queue.size > 1) {
        huffnode *node1 = pop_minheap(&priority_queue);
        huffnode *node2 = pop_minheap(&priority_queue);
        huffnode *internal_node = (huffnode *) malloc(sizeof(huffnode));
        internal_node->c = -1;
        internal_node->weight = node1->weight + node2->weight;
        internal_node->left = node1;
        internal_node->right = node2;
        push_minheap(&priority_queue, internal_node);
    }

    huffman_tree = *priority_queue.array[0];
}

void build_table(huffnode *node, int index) {
    static char buffer[256];
    if (node->c != -1) {
        char *code = (char *) malloc(index + 1);
        for (int i = 0; i < index; i++) {
            code[i] = buffer[i];
        }
        code[index] = '\0';
        table[node->c] = code;
        return;
    } else {
        buffer[index] = '0';
        build_table(node->left, index + 1);
        buffer[index] = '1';
        build_table(node->right, index + 1);
    }
}


void write_encoding(FILE *in, FILE *out) {
    char byte = 0;
    int offset = 7;
    int c = getc(in);
    while (c != EOF) {
        char *code = table[c];
        while (*code != '\0') {
            int bit = *code - '0';
            byte = (bit << offset) | byte;
            code++;
            if (offset == 0) {
                putc(byte, out);
                byte = 0;
                offset = 7;
            } else {
                offset--;
            }
        }
        c = getc(in);
    }
    if (offset < 7) {
        putc(byte, out);
    }
}

void write_tree(huffnode *node, FILE *file) {
    if (node->c != -1) {
        putc('1', file);
        putc(node->c, file);
        return;
    } else {
        putc('0', file);
        write_tree(node->left, file);
        write_tree(node->right, file);
    }
}

void decompress_file(FILE *compressed, FILE *dest) {
    rebuild_tree(compressed, &huffman_tree);
    uint64_t original_size = read_int(6, compressed);

    int offset = 7;
    int c = getc(compressed);
    huffnode *node = &huffman_tree;
    while (original_size--) {
        while (node->left != NULL) {
            if (((c >> offset) & 1) == 0) {
                node = node->left;
            } else {
                node = node->right;
            }
            
            if (offset == 0) {
                offset = 7;
                c = getc(compressed);
                //node = &huffman_tree;
            } else {
                offset--;
            }
            
        }
        putc(node->c, dest);
        node = &huffman_tree;
    }
}

void rebuild_tree(FILE *stream, huffnode *node) {
    unsigned char byte = getc(stream);
    if (byte == '1') {
        byte = getc(stream);
        node->c = byte;
        node->left = NULL;
        node->right = NULL;
        return;
    } else {
        node->left = (huffnode *) malloc(sizeof(huffnode));
        rebuild_tree(stream, node->left);
        node->right = (huffnode *) malloc(sizeof(huffnode));
        rebuild_tree(stream, node->right);
    }
}

void push_minheap(heap *h, huffnode *item) {
    huffnode **arr = h->array;
    int item_pos = h->size;
    int parent_pos = (item_pos - 1) / 2;
    arr[item_pos] = item;
    h->size++;

    // sift up
    while (item->weight < arr[parent_pos]->weight) {
        arr[item_pos] = arr[parent_pos];
        arr[parent_pos] = item;
        item_pos = parent_pos;
        parent_pos = (item_pos - 1) / 2;
    }
}

huffnode *pop_minheap(heap *h) {
    huffnode **arr = h->array;
    huffnode *min = arr[0];
    huffnode *item = arr[h->size - 1];
    arr[0] = item;
    int item_pos = 0;
    h->size--;
    int left_child_pos = 2 * item_pos + 1;
    int right_child_pos = 2 * item_pos + 2;

    // sift down
    while (left_child_pos < h->size) {
        int swap_pos = (right_child_pos >= h->size || arr[left_child_pos]->weight < arr[right_child_pos]->weight) ? left_child_pos : right_child_pos;
        if (item->weight > arr[swap_pos]->weight) {
            arr[item_pos] = arr[swap_pos];
            arr[swap_pos] = item;
            item_pos = swap_pos;
        } else {
            break;
        }
        left_child_pos = 2 * item_pos + 1;
        right_child_pos = 2 * item_pos + 2;
    }

    return min;
}

void write_int(uint64_t n, int size, FILE *stream) {
    for (int i = size - 1; i >= 0; i--) {
        unsigned char byte = (n >> (8 * i)) & 0xff;
        putc(byte, stream);
    }
}

uint64_t read_int(int size, FILE *stream) {
    uint64_t n = getc(stream);
    for (int i = 1; i <= size - 1; i++) {
        n = (n << 8) + getc(stream);
    }
    return n;
}

void free_tree(huffnode *node) {
    // All non-leaf nodes have exactly 2 children, so it is not necessary to check both nodes
    if (node->left == NULL) {
        free(node);
        return;
    }
    free_tree(node->left);
    free_tree(node->right);
    if (node != &huffman_tree) {
        free(node);
    }
}

char *create_file_name(char *name, char *extension) {
    char *file_name = (char *) malloc(strlen(name) + 1 + strlen(extension) + 1);
    int i;
    for (i = 0; name[i] != '\0'; i++) {
        file_name[i] = name[i];
    }
    file_name[i] = '.';
    i++;
    for (int j = 0; extension[j] != '\0'; i++, j++) {
        file_name[i] = extension[j];
    }
    file_name[i] = '\0';

    return file_name;
    /*
    int dot_pos = -1;
    int i;
    for (i = 0; file_name[i] != '\0'; i++) {
        if (file_name[i] == '.') {
            dot_pos = i;
        }
    }
    if (dot_pos == -1) {
        dot_pos = i;
    }

    char *new_file_name = (char *) malloc(dot_pos + 1 + strlen(extension) + 1);
    for (i = 0; i < dot_pos; i++) {
        new_file_name[i] = file_name[i];
    }
    new_file_name[dot_pos] = '.';
    i++;
    for (int j = 0; extension[j] != '\0'; i++, j++) {
        new_file_name[i] = extension[j];
    }
    new_file_name[i] = '\0';

    return new_file_name;
    */
}

char *get_base_name(char *file_name) {
    int dot_pos = -1;
    int i;
    for (i = 0; file_name[i] != '\0'; i++) {
        if (file_name[i] == '.') {
            dot_pos = i;
        }
    }
    if (dot_pos == -1) {
        dot_pos = i;
    }
    if (dot_pos == 0) {
        return NULL;
    }
    char *base_name = (char *) malloc(dot_pos + 1);
    strncpy(base_name, file_name, dot_pos);
    base_name[dot_pos] = '\0';
    return base_name;
}

char *get_extension(char *file_name) {
    int dot_pos = -1;
    for (int i = 0; file_name[i] != '\0'; i++) {
        if (file_name[i] == '.') {
            dot_pos = i;
        }
    }
    if (dot_pos == -1) {
        return NULL;
    } else {
        char *extension = (char *) malloc(strlen(file_name) - dot_pos + 1);
        strcpy(extension, file_name + dot_pos + 1);
        return extension;
    }
}

void print_encoded_bits(FILE *stream) {
    int c;
    while ((c = getc(stream)) != EOF) {
        printf("%s", table[c]);
    }
    putchar('\n');
}

// for testing purposes
void print_heap(heap *h) {
    for (int i = 0; i < h->size; i++) {
        printf("[%d]", (h->array[i])->weight);
        
    }
    putchar('\n');
}