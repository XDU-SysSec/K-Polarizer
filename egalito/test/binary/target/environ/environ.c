#include <stdio.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, char **argv, char **envp) {
    for(int i = 0; environ[i]; i ++) {
        printf("environ[%d] = \"%s\"\n", i, environ[i]);
    }

    printf("note: envp is %lx\n", (unsigned long)envp);
    printf("note: *envp is %s\n", *envp);
    printf("getenv: PATH=%s\n", getenv("PATH"));
    puts("iterating through envp manually:");
    for(char **env = envp; *env; env ++) {
        puts(*env);
    }
    puts("done");
    return 0;
}
