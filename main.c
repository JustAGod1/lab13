#include <stdio.h>
#include <windows.h>
#include <locale.h>
#include <mmcobj.h>


typedef union {
    char value;
    struct {
        unsigned unsynchronisation: 1;
        unsigned extended: 1;
        unsigned experimental: 1;
    };
} Flags;

typedef struct {
    char id[4];
    unsigned size;
    union {
        short flags;
        struct {
            unsigned tagAlterPreservation: 1;
            unsigned fileAlterPreservation: 1;
            unsigned readOnly: 1;
            unsigned reserved: 5;
            unsigned compression: 1;
            unsigned encryption: 1;
            unsigned grouping: 1;
        };
    };


} FrameHeader;

typedef struct {
    FrameHeader header;
    char *data;
    char *newText;
} MP3Frame;

struct ListNode {
    MP3Frame value;
    struct ListNode *previous;
};

typedef struct ListNode ListNode;

typedef struct {
    struct ListNode *head;
} FramesList;

typedef struct {
    unsigned framesSize;
    char version[2];
    unsigned size;
    Flags flags;
    FramesList frames;
} MP3Tag;

void add(FramesList *list, MP3Frame frame) {
    ListNode *node = malloc(sizeof(ListNode));
    node->value = frame;
    node->previous = list->head;
    list->head = node;
}

unsigned readWeirdInt(FILE *f);
void writeWeirdInt(FILE *f, unsigned value);
unsigned calcSize(MP3Frame *frame) {
    unsigned result = 4;
    if (frame->newText != 0) {
        result += 3 + strlen(frame->newText);
    } else {
        result += frame->header.size;
    }
    result += 4;
    result += 2;

    return result;
}
FrameHeader readHeader(FILE *f) {
    FrameHeader h;
    fread(h.id, 1, 4, f);
    h.size = readWeirdInt(f);
    fread(&h.flags, 2, 1, f);
    return h;
}

MP3Tag readData(FILE *input) {
    char magic[3];
    fread(magic, 1, 3, input);
    if (magic[0] != 'I' && magic[1] != 'D' && magic[2] != '3') {
        printf("Unrecognized file format");
        exit(-1);
    }
    char version[2];
    fread(version, 1, 2, input);
    if (version[0] != 4) {
        printf("Unsupported major version %d", version[0]);
        exit(-1);
    }

    Flags flags;
    flags.value = (char) fgetc(input);

    unsigned size = readWeirdInt(input);

    unsigned padding = 0;
    if (flags.extended) {
        unsigned extendedSize;
        fread(&extendedSize, 4, 1, input);
        long long pos;
        fgetpos(input, &pos);
        pos += 2;
        fsetpos(input, &pos);
        fread(&padding, 4, 1, input);
        pos += extendedSize - 2;
        fsetpos(input, &pos);
    }

    FramesList frames;
    frames.head = 0;

    unsigned read = 0;
    while (size - padding > read) {
        FrameHeader header = readHeader(input);
        if (header.id[0] == '\0' && header.id[1] == '\0' && header.id[2] == '\0' && header.id[3] == '\0') {
            fpos_t pos;
            fgetpos(input, &pos);
            pos -= 10;
            fsetpos(input, &pos);
            break;
        }
        if (header.compression) {
            printf("Compressed frames are not supported");
            exit(-1);
        }
        if (header.encryption) {
            printf("Encrypted frames are not supported");
            exit(-1);
        }
        read += 10;
        char *data = malloc(header.size);
        fread(data, 1, header.size, input);
        MP3Frame frame;
        frame.header = header;
        frame.data = data;
        frame.newText = 0;
        add(&frames, frame);
        read += header.size;
    }

    while (read < size) {
        if (fgetc(input) );
        read++;
    }

    MP3Tag tag;
    tag.framesSize = read;
    tag.frames = frames;
    tag.flags = flags;
    tag.size = size;
    tag.version[0] = version[0];
    tag.version[1] = version[1];
    return tag;
}

typedef enum {
    SHOW, SET, GET
} TaskType;

typedef struct {
    TaskType type;
    char *first;
    char *second;
} Task;

unsigned startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
            lenstr = strlen(str);
    return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}
unsigned calcSize(MP3Frame *frame);

Task* nextTask(char **args, int *pos, int count) {
    char *arg = args[*pos];
    (*pos)++;
    if (startsWith("--show", arg)) {
        Task *task = malloc(sizeof(Task));
        task->type = SHOW;
        task->first = 0;
        task->second = 0;
        return task;
    }

    if (startsWith("--get=", arg)) {
        Task *task = malloc(sizeof(Task));
        task->type = GET;
        task->first = arg + 6;
        task->second = 0;
        return task;
    }

    if (startsWith("--set=", arg)) {
        if (*pos >= count) {
            printf("Insufficient command line arguments\n");
            return 0;
        }
        char *valueArg = args[*pos];
        (*pos)++;
        if (!startsWith("--value=", valueArg)) {
            printf("Unexpected token: %s\n", valueArg);
            return 0;
        }
        Task *task = malloc(sizeof(Task));
        task->type = SET;
        task->first = arg + 6;
        task->second = valueArg + 8;

        return task;
    }
    printf("Unexpected token: %s\n", arg);

    return 0;
}

char* extractFileName(char* arg) {
    if (startsWith("--filepath=", arg)) {
        return arg + 11;
    }
    printf("First argument have to be filepath\n");
    return 0;
}

void printFrame(MP3Frame *frame, FILE *output) {
    fprintf(output, "%c%c%c%c:\n", frame->header.id[0], frame->header.id[1], frame->header.id[2], frame->header.id[3]);
    fprintf(output, "  size: %ud\n", frame->header.size);
    fprintf(output, "  flags:\n");
    fprintf(output, "    tag alter preservation: %s\n", frame->header.tagAlterPreservation ? "true" : "false");
    fprintf(output, "    file alter preservation: %s\n", frame->header.fileAlterPreservation ? "true" : "false");
    fprintf(output, "    read only: %s\n", frame->header.readOnly ? "true" : "false");
    fprintf(output, "    compression: %s\n", frame->header.compression ? "true" : "false");
    fprintf(output, "    encryption: %s\n", frame->header.encryption ? "true" : "false");
    fprintf(output, "    grouping: %s\n", frame->header.grouping ? "true" : "false");
    fprintf(output, "  data: ");
    if (frame->header.id[0] == 'T') {
        fprintf(output, "%ls", (wchar_t*)(frame->data + 3));
    } else {
        for (int i = 0; i < frame->header.size; ++i) {
            fprintf(output, "%hx", frame->data[i]);
        }
    }
    fprintf(output, "\n");
}

void writeFrame(MP3Frame *frame, FILE *output) {
    fwrite(frame->header.id, 1, 4, output);
    if (frame->newText != 0) {
        size_t len = wcslen((wchar_t *) frame->newText) ;
        unsigned char *baked = calloc(len + 3, 1);
        memcpy(baked + 3, frame->newText, len);
        baked[len + 2] = 0;
        baked[len + 1] = 0;
        baked[0] = 0x01;
        baked[1] = 0xff;
        baked[2] = 0xfe;
        frame->data = baked;
        frame->header.size = len + 3;
    }

    writeWeirdInt(output, frame->header.size);
    fwrite(&frame->header.flags, 2, 1, output);
    fwrite(frame->data, 1, frame->header.size, output);
    fflush(output);
}



void printTag(MP3Tag *tag, FILE *output) {
    fprintf(output, "version: %x, %x\n", tag->version[0], tag->version[0]);
    fprintf(output, "size: %x\n", tag->size);
    fprintf(output, "flags:\n");
    fprintf(output,"  unsynchronisation: %s\n", tag->flags.unsynchronisation ? "true" : "false");
    fprintf(output,"  extended: %s\n", tag->flags.extended ? "true" : "false");
    fprintf(output,"  experimantal: %s\n", tag->flags.experimental ? "true" : "false");

    fprintf(output, "\n");

    ListNode *node = tag->frames.head;
    while (node != 0) {
        printFrame(&node->value, output);
        fprintf(output, "\n");
        node = node->previous;
    }
}

MP3Frame *findFrame(MP3Tag *tag, char *name) {
    ListNode *node = tag->frames.head;
    while (node != 0) {
        if (memcmp(node->value.header.id, name, 4) == 0) return &node->value;
        node = node->previous;
    }
}

wchar_t* convert(char *input, unsigned size, UINT src) {
    wchar_t *output = malloc(size * 10);
    long resultSize = MultiByteToWideChar(src, 0, input, -1, output, (int) size * 10);
    if (resultSize == 0) {
        exit(GetLastError());
    }

    output[resultSize] = '\0';
    output[resultSize + 1] = '\0';
    return (wchar_t*) realloc(output, resultSize + 2);

}
int main(int argc, char **args) {
    setlocale(LC_ALL, "Rus");
    char *path = extractFileName(args[1]);
    if (path == 0)
        return -1;
    FILE *input = fopen(path, "rb");
    rewind(input);
    fseek(input, 0L, SEEK_END);
    long sz = ftell(input);
    printf("sz: %ld\n", sz);
    rewind(input);
    FILE *output = fopen("output.txt", "wb");

    MP3Tag tag = readData(input);

    int pos = 2;
    while (pos < argc) {
        Task *task = nextTask(args, &pos, argc);
        if (task == 0) return -1;
        switch (task->type) {
            case SHOW: {
                printTag(&tag, output);
                break;
            }
            case GET: {
                MP3Frame *frame = findFrame(&tag, task->first);
                if (frame == 0) {
                    printf("Can't find frame with id \"%s\"", task->first);
                    return -1;
                }
                printFrame(frame, output);
                break;
            }
            case SET: {
                MP3Frame *frame = findFrame(&tag, task->first);
                if (frame == 0) {
                    printf("Can't find frame with id \"%s\"", task->first);
                    return -1;
                }
                frame->newText = convert(task->second, strlen(task->second), GetConsoleOutputCP());
                break;
            }
        }
        free(task);
    }

    FILE *result = fopen("result.mp3", "wb");
    ListNode *node = tag.frames.head;
    unsigned size = 0;

    while (node != 0) {
        size += calcSize(&node->value);
        node = node->previous;
    }

    fwrite("ID3", 1, 3, result);
    fwrite(tag.version, 1, 2, result);

    Flags flags = tag.flags;
    flags.extended = 0;

    fputc(flags.value, result);
    writeWeirdInt(result, tag.size);
    node = tag.frames.head;
    while (node != 0) {
        writeFrame(&node->value, result);
        node = node->previous;
    }
    int delta = (int) tag.size - size;
    while (delta > 0) {
        fputc(0, result);
        delta--;
    }
    size_t a;
    size_t counter = 0;
    char buffer[128];
    while ((a = fread(&buffer[0], 1, 128, input)) != 0) {
        fwrite(&buffer[0], 1, a, result);
        counter += a;
    }

    printf("%d\n", counter);
    printf("ferror input: %d\n", ferror(input));
    printf("feof input: %d\n", feof(input));

    return 0;
}



unsigned readWeirdInt(FILE *f) {
    unsigned char m[4];

    fread(m, 1, 4, f);

    unsigned result = ((unsigned) m[0]) << 21 | ((unsigned) m[1]) << 14 | ((unsigned) m[2]) << 7 | m[3];

    return result;

}

void writeWeirdInt(FILE *f, unsigned value) {
    unsigned a = (value & 0b00000000000000000000000001111111) << 0;
    unsigned b = (value & 0b00000000000000000111111110000000) << 1;
    unsigned c = (value & 0b00000000011111111000000000000000) << 2;
    unsigned d = (value & 0b00011111100000000000000000000000) << 3;
    unsigned r = a | b | c | d;

    char *end = ((char *) &r) + 3;
    fwrite(end, 1, 1, f);
    fwrite(end - 1, 1, 1, f);
    fwrite(end - 2, 1, 1, f);
    fwrite(end - 3, 1, 1, f);
}


