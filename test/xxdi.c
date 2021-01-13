#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void xxdi_encode(void *addr, uint32_t size)
{
    int index;
    unsigned char *len_str = "unsigned int hex_string_len = ";
    unsigned char *pre_str = "unsigned char hex_string[] = {";

    fprintf(stdout, "%s%d;\n", len_str, size); 
    fprintf(stdout, "%s\n", pre_str); 
    while (size > 0) {
        fprintf(stdout, " ");
        index = 0;

        // 20 bytes in the line
        while (index < 10) {
            fprintf(stdout, "0x%02x", *((unsigned char *)addr));
            
            index++;
            addr++;
            size--;
 
            if (size <= 0)
                break;
            else
                fprintf(stdout, ", ");
        }
        fprintf(stdout, "\n");
    }
    
    fprintf(stdout, "};\n");
    return;

}
