#include <stdio.h>
#include <syscall.h>
/*
int
main (int argc, char **argv)
{

  int i;

  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}*/

int
main (int argc, char **argv)
{

  printf("HELLO: %s\n", argv[1]);
  exit(57);
  return 100;
}
