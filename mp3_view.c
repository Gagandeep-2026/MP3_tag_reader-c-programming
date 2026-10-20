
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FIELD_WIDTH 15
#define VALUE_WIDTH 35

#define TAG_SIZE 10
#define FRAME_HEADER_SIZE 10

static void print_line(void)
{
    printf("+-%-*s-+-%-*s-+\n",
           FIELD_WIDTH, "---------------",
           VALUE_WIDTH, "-----------------------------------");
}

static void print_row(const char *field, const char *value)
{
    /* Do not call print_row() from inside this function. */
    printf("| %-*.*s | %-*.*s |\n",
           FIELD_WIDTH, FIELD_WIDTH, field,
           VALUE_WIDTH, VALUE_WIDTH, value);
}

static unsigned int decode_tag_size(const unsigned char *p)
{
    return ((unsigned int)(p[0] & 0x7F) << 21) |
           ((unsigned int)(p[1] & 0x7F) << 14) |
           ((unsigned int)(p[2] & 0x7F) << 7) |
           ((unsigned int)(p[3] & 0x7F));
}

static unsigned int decode_frame_size(const unsigned char *p,
                                      unsigned char version)
{
    if (version == 4)
    {
        return decode_tag_size(p);
    }

    return ((unsigned int)p[0] << 24) |
           ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) |
           (unsigned int)p[3];
}

static void clean_text(const unsigned char *src,
                       unsigned int size,
                       char *dest,
                       size_t dest_size)
{
    size_t i;
    size_t j = 0;
    unsigned char encoding;

    if (dest_size == 0)
        return;

    dest[0] = '\0';

    if (size == 0)
        return;

    encoding = src[0];

    /*
     * Encoding 0: ISO-8859-1
     * Encoding 3: UTF-8
     *
     * For encoding 1 or 2 (UTF-16), this simple reader
     * retains printable ASCII bytes where possible.
     */
    if (encoding == 0 || encoding == 3)
    {
        for (i = 1; i < size && j < dest_size - 1; i++)
        {
            if (src[i] == '\0')
                break;

            if (src[i] >= 32 || src[i] == '\t')
                dest[j++] = (char)src[i];
        }
    }
    else
    {
        for (i = 1; i < size && j < dest_size - 1; i++)
        {
            if (src[i] == '\0')
                break;

            if (src[i] >= 32 && src[i] < 127)
                dest[j++] = (char)src[i];
        }
    }

    dest[j] = '\0';

    /* Remove trailing spaces. */
    while (j > 0 && dest[j - 1] == ' ')
    {
        dest[--j] = '\0';
    }
}

static void set_field(const char *frame_id,
                      const char *value,
                      char *title,
                      char *artist,
                      char *album,
                      char *year,
                      char *genre,
                      char *composer)
{
    if (strcmp(frame_id, "TIT2") == 0)
        snprintf(title, 128, "%s", value);
    else if (strcmp(frame_id, "TPE1") == 0)
        snprintf(artist, 128, "%s", value);
    else if (strcmp(frame_id, "TALB") == 0)
        snprintf(album, 128, "%s", value);
    else if (strcmp(frame_id, "TYER") == 0 ||
             strcmp(frame_id, "TDRC") == 0)
        snprintf(year, 128, "%s", value);
    else if (strcmp(frame_id, "TCON") == 0)
        snprintf(genre, 128, "%s", value);
    else if (strcmp(frame_id, "TCOM") == 0)
        snprintf(composer, 128, "%s", value);
}

void display_mp3(const char *filename)
{
    FILE *fp;
    unsigned char header[TAG_SIZE];
    unsigned char frame_header[FRAME_HEADER_SIZE];
    unsigned char *frame_data = NULL;

    unsigned int tag_size;
    unsigned int remaining;
    unsigned int frame_size;

    unsigned char version;

    char title[128] = "Not Available";
    char artist[128] = "Not Available";
    char album[128] = "Not Available";
    char year[128] = "Not Available";
    char genre[128] = "Not Available";
    char composer[128] = "Not Available";
    char value[512];

    size_t bytes_read;

    fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        perror("Unable to open MP3 file");
        return;
    }

    bytes_read = fread(header, 1, TAG_SIZE, fp);

    if (bytes_read != TAG_SIZE ||
        memcmp(header, "ID3", 3) != 0)
    {
        printf("No valid ID3v2 tag found.\n");
        fclose(fp);
        return;
    }

    version = header[3];

    if (version != 3 && version != 4)
    {
        printf("Unsupported ID3 version: 2.%u\n", version);
        fclose(fp);
        return;
    }

    tag_size = decode_tag_size(&header[6]);

    /*
     * ID3 tag size is the total tag payload size,
     * excluding the 10-byte ID3 header.
     */
    remaining = tag_size;

    while (remaining >= FRAME_HEADER_SIZE)
    {
        bytes_read = fread(frame_header, 1, FRAME_HEADER_SIZE, fp);

        if (bytes_read != FRAME_HEADER_SIZE)
            break;

        remaining -= FRAME_HEADER_SIZE;

        /* Padding starts with zero bytes. */
        if (frame_header[0] == 0)
            break;

        /* Validate that the frame ID contains letters or digits. */
        {
            int valid_id = 1;
            int i;

            for (i = 0; i < 4; i++)
            {
                unsigned char c = frame_header[i];

                if (!((c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9')))
                {
                    valid_id = 0;
                    break;
                }
            }

            if (!valid_id)
                break;
        }

        frame_size = decode_frame_size(&frame_header[4], version);

        /*
         * Stop if the frame size is invalid or exceeds
         * the remaining bytes in the ID3 tag.
         */
        if (frame_size > remaining)
            break;

        if (frame_size == 0)
            continue;

        frame_data = malloc(frame_size);

        if (frame_data == NULL)
        {
            printf("Memory allocation failed.\n");
            fclose(fp);
            return;
        }

        bytes_read = fread(frame_data, 1, frame_size, fp);

        if (bytes_read != frame_size)
        {
            free(frame_data);
            frame_data = NULL;
            break;
        }

        remaining -= frame_size;

        if (frame_header[0] == 'T' && frame_size > 1)
        {
            clean_text(frame_data, frame_size,
                       value, sizeof(value));

            set_field((const char *)frame_header,
                      value, title, artist, album,
                      year, genre, composer);
        }

        free(frame_data);
        frame_data = NULL;
    }

    fclose(fp);

    /* Print the heading and column names only once. */
    printf("\n       MP3 TAG READER FOR ID3V2 VERSION 2.%u\n\n",
           version);

    print_line();
    print_row("Field", "Value");
    print_line();

    print_row("Title", title);
    print_row("Artist", artist);
    print_row("Album", album);
    print_row("Year", year);
    print_row("Genre", genre);
    print_row("Composer", composer);

    print_line();
}