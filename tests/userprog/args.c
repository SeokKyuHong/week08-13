/* Prints the command-line arguments.
   This program is used for all of the args-* tests.  Grading is
   done differently for each of the args-* tests based on the
   output. */
/* 명령줄 인수를 인쇄합니다.
    이 프로그램은 모든 args-* 테스트에 사용됩니다. 
    채점은 출력을 기반으로 하는 각 args-* 테스트에 대해 다르게 수행됩니다. */

#include "tests/lib.h"

// 입력 받은 글자와 같게 들어왔는지 체크하는 것 같음
int
main (int argc, char *argv[]) 
{
  int i;

  test_name = "args";

  if (((unsigned long long) argv & 7) != 0)
    msg ("argv and stack must be word-aligned, actually %p", argv);

  msg ("begin");
  msg ("argc = %d", argc);
  for (i = 0; i <= argc; i++)
    if (argv[i] != NULL)
      msg ("argv[%d] = '%s'", i, argv[i]);
    else
      msg ("argv[%d] = null", i);
  msg ("end");

  return 0;
}
