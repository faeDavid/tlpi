#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]){

    for (int i = 0; i < argc; i++){
        printf("Arg: %s\n", argv[i]);
    }

    return 0;
}