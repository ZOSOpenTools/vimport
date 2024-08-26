#include "dio.h"
#include <stdio.h>
#include <limits.h>

int main(int argc, char* argv[]) {
  int rc;
  struct DFILE* dfile;
  char ds[54+1+2+2];
  char dsorgbuff[DSORG_MAX];
  char recfmbuff[RECFM_MAX];
  char ccsidbuff[DCCSID_MAX];
  char mem[MEM_MAX+1];
  char hlqstr[MEM_MAX+1];
  char hml[DS_MAX+1];
  char unix[_POSIX_PATH_MAX];

  if (argc != 3) {
    fprintf(stderr, "Syntax: argv[0] <hlq> <relative-dataset>\n");
    return 4;
  }
  const char* hlq = argv[1];
  const char* relds = argv[2];

  if (strlen(hlq) > 8 || strlen(ds) > 45) {
    fprintf(stderr, "HLQ and/or relative dataset name too long: %s %s\n", hlq, ds);
    return 16;
  }
  sprintf(ds, "//'%s.%s'", hlq, relds);

  dfile = open_dataset(ds, stderr);
  if (!dfile || dfile->err) {
    if (dfile) {
      fprintf(stderr, "%s\n", dfile->msgbuff);
    }
    return 4;
  }
  printf("Dataset attributes for dataset %s: mem:%s hlq:%s hightomid:%s dsorg:%s recfm:%s lrecl:%d ccsid:%s\n",
    relds, member_name(dfile, mem), high_level_qualifier(dfile, hlqstr), high_to_mid_level_qualifier(dfile, hml), dsorgs(dfile->dsorg, dsorgbuff), recfms(dfile->recfm, recfmbuff), dfile->reclen, dccsids(dfile->dccsid, ccsidbuff));

  printf("Unix file: %s\n", map_to_unixfile(dfile, unix));

  rc = read_dataset(dfile);
  if (rc) {
    fprintf(stderr, "%s\n", dfile->msgbuff);
    return 4;
  }

  printf("Read %d bytes from dataset %s\n", dfile->bufflen, relds);

  rc = close_dataset(dfile);
  if (rc) {
    fprintf(stderr, "%s\n", dfile->msgbuff);
    return 4;
  }
  return 0;
}
