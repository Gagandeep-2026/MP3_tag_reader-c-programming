#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "mp3_edit.h"
#include "mp3_view.h"

#define ID3_HEADER_SIZE 10
#define ID3_FRAME_HEADER_SIZE 10
#define ID3_MAX_TAG_SIZE (64 * 1024 * 1024)

static int read_syncsafe_size(const unsigned char *size)
{
    return ((int)(size[0] & 0x7F) << 21) |
           ((int)(size[1] & 0x7F) << 14) |
           ((int)(size[2] & 0x7F) << 7) |
           (int)(size[3] & 0x7F);
}

static void write_syncsafe_size(unsigned char *size, int value)
{
    size[0] = (unsigned char)((value >> 21) & 0x7F);
    size[1] = (unsigned char)((value >> 14) & 0x7F);
    size[2] = (unsigned char)((value >> 7) & 0x7F);
    size[3] = (unsigned char)(value & 0x7F);
}

static void write_be_size(unsigned char *size, uint32_t value)
{
    size[0] = (unsigned char)((value >> 24) & 0xFF);
    size[1] = (unsigned char)((value >> 16) & 0xFF);
    size[2] = (unsigned char)((value >> 8) & 0xFF);
    size[3] = (unsigned char)(value & 0xFF);
}

static int write_all(FILE *fp, const void *data, size_t size)
{
    return fwrite(data, 1, size, fp) == size;
}

static int update_id3v1_field(FILE *ptr, long offset_from_end,
                              const char *value, size_t width)
{
    unsigned char tag[128];
    char field[30];

    if (width > sizeof(field))
        return 0;

    if (fseek(ptr, -128L, SEEK_END) != 0)
        return 0;

    if (fread(tag, 1, sizeof(tag), ptr) != sizeof(tag))
        return 0;

    if (memcmp(tag, "TAG", 3) != 0)
        return 0;

    memset(field, ' ', width);

    size_t len = strlen(value);
    if (len > width)
        len = width;

    memcpy(field, value, len);

    if (fseek(ptr, offset_from_end, SEEK_END) != 0)
        return 0;

    if (!write_all(ptr, field, width))
        return 0;

    return fflush(ptr) == 0;
}

static FILE *file_check(char *file_name)
{
    FILE *ptr = fopen(file_name, "r+b");

    if (ptr == NULL)
    {
        perror("fopen");
        return NULL;
    }

    unsigned char header[10];

    rewind(ptr);

    if (fread(header, 1, sizeof(header), ptr) == sizeof(header) &&
        memcmp(header, "ID3", 3) == 0)
    {
        return ptr;
    }

    unsigned char tag[128];

    if (fseek(ptr, -128L, SEEK_END) == 0 &&
        fread(tag, 1, sizeof(tag), ptr) == sizeof(tag) &&
        memcmp(tag, "TAG", 3) == 0)
    {
        return ptr;
    }

    fclose(ptr);
    printf("ERROR : ID3V1/V2 TAG is not found\n");
    return NULL;
}


//   Updates an existing ID3v2 text frame or adds a new one.
//   Rebuilds the tag in a temporary file and copies the audio bytes
//   unchanged after the updated tag.
 
//   This implementation supports ordinary ID3v2.3 and ID3v2.4 tags
//   without extended headers, unsynchronisation, or footer flags.
 
static int update_id3v2_frame(FILE *ptr, const char *file_name,
                              const char *frame_id, const char *value)
{
    unsigned char header[10];
    unsigned char *old_body = NULL;
    unsigned char *new_body = NULL;
    unsigned char *file_data = NULL;
    unsigned char frame_header[10];
    unsigned char *frame_payload = NULL;
    FILE *tmp = NULL;

    int version;
    int tag_size;
    int new_tag_size;
    size_t value_len = strlen(value);
    size_t payload_size = value_len + 1; /* Encoding byte + text */
    size_t frame_size;
    size_t body_used = 0;
    //size_t body_capacity;
    size_t pos = 0;
    size_t file_size;
    size_t audio_offset;
    size_t new_body_used;
    long end_pos;
    int found = 0;
    int success = 0;
    char *tmp_name = NULL;

    if (strlen(frame_id) != 4 || value_len > 1024 * 1024)
        return 0;

    if (fseek(ptr, 0, SEEK_END) != 0)
        return 0;

    end_pos = ftell(ptr);
    if (end_pos < 10)
        return 0;

    file_size = (size_t)end_pos;

    rewind(ptr);

    if (fread(header, 1, sizeof(header), ptr) != sizeof(header) ||
        memcmp(header, "ID3", 3) != 0)
    {
        return 0;
    }

    version = header[3];

    if (version != 3 && version != 4)
        return 0;

    /* This implementation does not parse extended headers or unsync tags. */
    if ((header[5] & 0xC0) != 0)
        return 0;

    tag_size = read_syncsafe_size(header + 6);

    if (tag_size < 0 || tag_size > ID3_MAX_TAG_SIZE ||
        (size_t)tag_size > file_size - 10)
    {
        return 0;
    }

    audio_offset = 10 + (size_t)tag_size;
    old_body = malloc((size_t)tag_size + 1);

    if (old_body == NULL)
        goto cleanup;

    if (fread(old_body, 1, (size_t)tag_size, ptr) != (size_t)tag_size)
        goto cleanup;

    /*
     * Validate the frame layout and determine where padding starts.
     * Retain complete existing frames, excluding the padding.
     */
    while (pos + 10 <= (size_t)tag_size)
    {
        unsigned char *fh = old_body + pos;
        uint32_t fs;

        if (fh[0] == 0)
            break;

        if (version == 4)
        {
            int ss = read_syncsafe_size(fh + 4);
            if (ss < 0)
                goto cleanup;
            fs = (uint32_t)ss;
        }
        else
        {
            fs = ((uint32_t)fh[4] << 24) |
                 ((uint32_t)fh[5] << 16) |
                 ((uint32_t)fh[6] << 8) |
                 (uint32_t)fh[7];
        }

        if ((size_t)fs > (size_t)tag_size - pos - 10)
            goto cleanup;

        if (memcmp(fh, frame_id, 4) == 0)
        {
            found = 1;

            if (fs < 1)
                goto cleanup;

            /*
             * Keep the original frame length so no other frame or
             * audio bytes are overwritten. Text is truncated if needed.
             */
            size_t available = (size_t)fs - 1;
            size_t copy_len = value_len;

            if (copy_len > available)
                copy_len = available;

            fh[10] = 0; /* ISO-8859-1 text encoding */
            memset(fh + 11, 0, available);
            memcpy(fh + 11, value, copy_len);

            body_used = (size_t)tag_size;
            break;
        }

        pos += 10 + (size_t)fs;
    }

    if (!found)
    {
        /* Add the missing frame after all valid frames. */
        body_used = pos;

        if (payload_size > UINT32_MAX - 10)
            goto cleanup;

        frame_size = 10 + payload_size;

        if (body_used > (size_t)ID3_MAX_TAG_SIZE - frame_size)
            goto cleanup;

        new_body_used = body_used + frame_size;
        new_body = calloc(new_body_used ? new_body_used : 1, 1);

        if (new_body == NULL)
            goto cleanup;

        if (body_used > 0)
            memcpy(new_body, old_body, body_used);

        memcpy(frame_header, frame_id, 4);

        if (version == 4)
            write_syncsafe_size(frame_header + 4, (int)payload_size);
        else
            write_be_size(frame_header + 4, (uint32_t)payload_size);

        frame_header[8] = 0;
        frame_header[9] = 0;

        frame_payload = calloc(payload_size, 1);
        if (frame_payload == NULL)
            goto cleanup;

        frame_payload[0] = 0; /* ISO-8859-1 */
        memcpy(frame_payload + 1, value, value_len);

        memcpy(new_body + body_used, frame_header, 10);
        memcpy(new_body + body_used + 10, frame_payload, payload_size);

        free(old_body);
        old_body = new_body;
        new_body = NULL;
        tag_size = (int)new_body_used;
    }

    new_tag_size = tag_size;

    if (new_tag_size < 0 || new_tag_size > ID3_MAX_TAG_SIZE)
        goto cleanup;

    file_data = malloc(file_size - audio_offset + 1);
    if (file_data == NULL)
        goto cleanup;

    if (fseek(ptr, (long)audio_offset, SEEK_SET) != 0)
        goto cleanup;

    size_t audio_size = file_size - audio_offset;

    if (fread(file_data, 1, audio_size, ptr) != audio_size)
        goto cleanup;

    tmp_name = malloc(strlen(file_name) + 16);
    if (tmp_name == NULL)
        goto cleanup;

    sprintf(tmp_name, "%s.tmp", file_name);

    tmp = fopen(tmp_name, "wb");
    if (tmp == NULL)
    {
        perror("temporary file");
        goto cleanup;
    }

    memcpy(header + 6, "\0\0\0\0", 4);
    write_syncsafe_size(header + 6, new_tag_size);

    if (!write_all(tmp, header, sizeof(header)) ||
        !write_all(tmp, old_body, (size_t)new_tag_size) ||
        !write_all(tmp, file_data, audio_size) ||
        fflush(tmp) != 0)
    {
        goto cleanup;
    }

    if (fclose(tmp) != 0)
    {
        tmp = NULL;
        goto cleanup;
    }

    tmp = NULL;

    if (rename(tmp_name, file_name) != 0)
    {
        perror("rename");
        goto cleanup;
    }

    success = 1;

cleanup:
    if (tmp != NULL)
        fclose(tmp);

    if (!success && tmp_name != NULL)
        remove(tmp_name);

    free(tmp_name);
    free(old_body);
    free(new_body);
    free(file_data);
    free(frame_payload);

    return success;
}

static void print_success(const char *field, const char *value)
{
    printf("                                                         \n");
    printf("            %s UPDATED SUCCESSFULLY\n", field);
    printf("---------------------------------------------------------\n");
    printf("| %-15s | %-35.35s |\n", field, value);
    printf("---------------------------------------------------------\n");
}

static void edit_field(char *file_name, char *edit_text,
                       const char *frame_id, const char *field,
                       long id3v1_offset, size_t id3v1_width)
{
    FILE *ptr = file_check(file_name);

    if (ptr == NULL)
        return;

    int result = update_id3v2_frame(ptr, file_name, frame_id, edit_text);
    fclose(ptr);

    if (!result)
    {
        ptr = fopen(file_name, "r+b");

        if (ptr != NULL)
        {
            result = update_id3v1_field(ptr, id3v1_offset,
                                        edit_text, id3v1_width);
            fclose(ptr);
        }
    }

    if (result)
        print_success(field, edit_text);
    else
        printf("ERROR : unable to update %s\n", field);
}

void title_edit(char *file_name, char *edit_text)
{
    edit_field(file_name, edit_text, "TIT2", "Title", -125, 30);
}

void artist_edit(char *file_name, char *edit_text)
{
    edit_field(file_name, edit_text, "TPE1", "Artist", -95, 30);
}

void album_edit(char *file_name, char *edit_text)
{
    edit_field(file_name, edit_text, "TALB", "Album", -65, 30);
}

void year_edit(char *file_name, char *edit_text)
{
    edit_field(file_name, edit_text, "TYER", "Year", -31, 4);
}

void comment_edit(char *file_name, char *edit_text)
{
    /*
     * ID3v1 comments are supported here. ID3v2 COMM frames have a
     * language and description structure, so they are not treated as
     * ordinary text frames.
     */
    FILE *ptr = file_check(file_name);

    if (ptr == NULL)
        return;

    int result = update_id3v1_field(ptr, -30, edit_text, 28);
    fclose(ptr);

    if (result)
        print_success("Comment", edit_text);
    else
        printf("ERROR : unable to update Comment. ID3v1 comment not found.\n");
}

void content_edit(char *file_name, char *edit_text)
{
    (void)file_name;
    (void)edit_text;
    printf("ERROR : content editing is not supported in this version\n");
}