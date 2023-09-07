#include "platform/fs.h"
#include "platform/log.h"
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void fs_pwd()
{
    char cwd[1024] = {0};
    char * rc = getcwd(cwd, sizeof(cwd));
    if(rc == NULL)
    {
        platform_log("getcwd error: %s\n", strerror(errno));
        return;
    }
    printf("Current working directory: %s%s%s\n", "\033[38;2;100;100;255m", cwd, "\033[0m");
}


char * fs_readfile(char const * path)
{
    char* content = NULL;

    FILE* file = fopen(path, "r");
    if (file == NULL) 
    {
        fs_pwd();
        printf("  ");
        platform_log("%s (%s)\n", strerror(errno), path);
        goto error;
    }

    fseek(file, 0 , SEEK_END);
    int32_t size = (int32_t)ftell(file);

    if (size == -1)
    {
        goto error;
    }
    rewind(file);

    content = malloc(size + 1);
    content[size] = '\0';
    size_t n = fread(content, size, 1, file);
    if(n != 1)
    {
        platform_log("%s: could not read wholef file %d bytes\n", path, size);
        goto error;
    }
    assert(content[size] == '\0');
    fclose(file);
    return content;
error:
    if(content)
    {
        free(content);
    }
    return NULL;
}