/*documentation:

NAME       : Gagandeep MT
ADMIN ID   : 26012_071.
START DATE : 2/20/2026.
END DATE   : 20/10/2026

MP3 TAG READER:
Developed a C-based MP3 Tag Reader that extracts and displays metadata stored in MP3 audio files using ID3v2 tags. 
The project demonstrates practical implementation of file handling, binary data processing, structures, pointers, and 
command-line arguments** in C.

 KEY FEATURES
* Reads and validates MP3 files.
* Detects and processes ID3v2 metadata.
* Extracts information such as:

  * Title
  * Artist
  * Album
  * Year
  * Genre
  * Comment
* Displays extracted metadata in a user-friendly format.
* Handles file operations using standard C library functions.
* Includes validation and error handling for invalid files or missing tags.

TECHNOLOGIES USED
C | File Handling | Structures | Pointers | Binary File Processing | ID3v2 Metadata**

USAGE:
---------------------------------------------------------------------------------------
ERROR : ./a.out : Invalid arguments
usage :
To view please pass like : ./a.out -v mp3_file_name
To edit please pass like : ./a.out -e -t/-a/-A/-y/-m/-c changing_text mp3_file_name
To help please pass like : ./a.out --help
---------------------------------------------------------------------------------------

SAMPLE OUTPUT:
ID3V2.3.0

1. output for view:

        MP3 TAG READER FOR ID3V2 VERSION 2.3

+-----------------+-------------------------------------+
| Field           | Value                               |
+-----------------+-------------------------------------+
| Title           | new_title                           |
| Artist          | Sid Sriram                          |
| Album           | Ala Vaikunthapurramuloo (2020)      |
| Year            | 2022                                |
| Genre           | Telugu                              |
| Composer        | Thaman S                            |
+-----------------+-------------------------------------+

2. output for edit

            Title UPDATED SUCCESSFULLY
---------------------------------------------------------
| Title           | new_title                           |
---------------------------------------------------------
*/

#include<stdio.h>
#include"mp3_view.h"
#include"mp3_edit.h"
#include<string.h>

int main(int argc,char *argv[])
{
    if(argc<2)
    {
        printf("Usage:\n");
        printf("To view Pass Like : ./a.out -v file_name.mp3\n");
        printf("TO edit Pass Like : ./a.out -e -t|-a|-A|-y|-m|-c file_name.mp3 Enter_the_text_Edit\n");
        printf("To Help pass Like : ./a.out -h \n");
        return 0;
    }
    if(strcmp(argv[1],"-v")==0)
    {
        if(argc!=3)
        {
            printf("usage : ouput_file -v filename");
            return 1;
        }
        display_mp3(argv[2]);//pass to view function
    }
    else if(strcmp(argv[1],"-e")==0)
    {
        if(argc!=5)
        {
            printf("Usage : 1.output_file 2.-e 3.-t|-a|-A|-y|-m|-c 4.MP3_file 5.new_title");
            return 1;
        }
        //edit_mp3(argv[2]);//pass to edit function
        if(strcmp(argv[2],"-t")==0)
        {
            title_edit(argv[3],argv[4]);
        }
        else if(strcmp(argv[2],"-a")==0)
        {
            artist_edit(argv[3],argv[4]);
        }
        else if(strcmp(argv[2],"-A")==0)
        {
            album_edit(argv[3],argv[4]);
        }
        else if(strcmp(argv[2],"-y")==0)
        {
            year_edit(argv[3],argv[4]);
        }
        else if(strcmp(argv[2],"-m")==0)
        {
            content_edit(argv[3],argv[4]);
        }
        else if(strcmp(argv[2],"-c")==0)
        {
            comment_edit(argv[3],argv[4]);
        }
        else{
            printf("Operation type invalid\n");
            return 0;
        }

    }
    else if(strcmp(argv[1],"-h")==0)
    {
        if(argc!=2)
        {
            printf("usage : output_file -h");
            return 1;
        }
        printf("1.-v to vive mp3 file content\n");
        printf("2.-e to edit mp3 file content\n");
        printf("    2.1.-t to edit the song Title\n");
        printf("    2.2.-a to edit the song Artist\n");
        printf("    2.3.-A to edit the song Album\n");
        printf("    2.4.-y to edit the song Year\n");
        printf("    2.5.-m to edit the song content\n");
        printf("    2.6.-c to edit the song Comment\n");
        
        return 0;

    }
    else{
        printf("Operation type invalid\n");
            return 0;
    }
}
