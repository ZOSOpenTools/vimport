#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include "s99.h"
#include "dio.h"
#include "dioint.h"
#include "wrappers.h"
#include <_Nascii.h>
#include <unistd.h>

/*#define DEBUG 1*/
#define DD_SYSTEM "????????"
#define ERRNO_NONEXISTANT_FILE (67)
#define DIO_MSG_BUFF_LEN (4095)

const struct s99_rbx s99rbxtemplate = {"S99RBX",S99RBXVR,{0,1,0,0,0,0,0},0,0,0};

void errmsg(struct DFILE* dfile, const char* format, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, format);
  vsnprintf(dfile->msgbuff, dfile->msgbufflen, format, arg_ptr);
  if (__isASCII()) {
    size_t msglen = strlen(dfile->msgbuff);
    if (msglen > 0) {
      __e2a_l(dfile->msgbuff, msglen);
    }
  } 
  va_end(arg_ptr);
}

static enum DIOERR dsdd_alloc(struct DFILE* dfile, struct s99_common_text_unit* dsn, struct s99_common_text_unit* dd, struct s99_common_text_unit* disp)
{
  struct s99rb* __ptr32 parms;
  enum s99_verb verb = S99VRBAL;
  struct s99_flag1 s99flag1 = {0};
  struct s99_flag2 s99flag2 = {0};
  size_t num_text_units = 3;
  int rc;
  struct s99_rbx s99rbx = s99rbxtemplate;

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dsn, dd, disp );
  if (!parms) {
    errmsg(dfile, "Unable to initialize SVC99 (DYNALLOC) control blocks.");
    return DIOERR_SVC99INIT_ALLOC_FAILURE;
  }
  rc = S99(parms);
  if (rc) {
    errmsg(dfile, "SVC99 failed. See error log for details.");
#ifdef DEBUG
    s99_fmt_dmp(dfile->logstream, parms);
#endif
    s99_prt_msg(dfile, dfile->logstream, parms, rc);
    return DIOERR_SVC99_ALLOC_FAILURE;
  }

  struct s99_common_text_unit* ddout = (struct s99_common_text_unit*) parms->s99txtpp[1];
  dd->s99tulng = ddout->s99tulng;
  memcpy(dd->s99tupar, ddout->s99tupar, dd->s99tulng);

  s99_free(parms);
  return DIOERR_NOERROR;
}

enum DIOERR ddfree(struct DFILE* dfile, struct s99_common_text_unit* dd)
{
  struct s99rb* __ptr32 parms;
  enum s99_verb verb = S99VRBUN;
  struct s99_flag1 s99flag1 = {0};
  struct s99_flag2 s99flag2 = {0};
  size_t num_text_units = 1;
  int rc;
  struct s99_rbx s99rbx = s99rbxtemplate;

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dd );
  if (!parms) {
    errmsg(dfile, "Unable to initialize SVC99 (DYNFREE) control blocks.");
    return DIOERR_SVC99INIT_FREE_FAILURE;
  }
  rc = S99(parms);
  if (rc) {
#ifdef DEBUG
    s99_fmt_dmp(dfile->logstream, parms);
#endif
    s99_prt_msg(dfile, dfile->logstream, parms, rc);
    return DIOERR_SVC99_ALLOC_FAILURE;
  }

  s99_free(parms);
  return DIOERR_NOERROR;
}

enum DIOERR init_dsnam_text_unit(struct DFILE* dfile, const char* dsname, struct s99_common_text_unit* dsn)
{
  size_t dsname_len = (dsname == NULL) ? 0 : strlen(dsname);
  if (dsname == NULL || dsname_len == 0 || dsname_len > DS_MAX) {
    errmsg(dfile, "Dataset Name <%.*s> is invalid.", dsname_len, dsname);
    return DIOERR_INVALID_DATASET_NAME;
  }

  dsn->s99tulng = dsname_len;
  memcpy(dsn->s99tupar, dsname, dsname_len);
  return DIOERR_NOERROR;
}

static void strupper(char* str)
{
  for (int i=0; i<strlen(str); ++i) {
    if (islower(str[i])) {
      str[i] = toupper(str[i]);
    }
  }
}

static int has_member(struct DIFILE* difile)
{
  return difile->member_name[0] != '\0';
}
  
static enum DIOERR init_dataset_info(struct DFILE* dfile, const char* dataset_name, struct DIFILE* difile)
{
  size_t dataset_name_len = strlen(dataset_name);
  size_t ds_full_len;
  const char* dataset_name_start;
  const char* dataset_name_end;

  if ((dataset_name_len > DS_MAX+MEM_MAX+2+2+2) || (dataset_name_len < 2+2)) {
    errmsg(dfile, "Dataset name %s is not a valid dataset name of format: //<dataset> or //'<dataset>'.", dataset_name);
    return DIOERR_LE_DATASET_NAME_TOO_LONG_OR_TOO_SHORT;
  }

  if (memcmp(dataset_name, "//", 2)) {
    errmsg(dfile, "Dataset name %s does not start with // and therefore is not a valid dataset name.", dataset_name);
    return DIOERR_INVALID_LE_DATASET_NAME;
  }
  if (!memcmp(dataset_name, "//'", 3)) {
    if (dataset_name[dataset_name_len-1] != '\'') {
      errmsg(dfile, "Dataset name %s does not have balanced single quotes.", dataset_name);
      return DIOERR_LE_DATASET_NAME_QUOTE_MISMATCH;
    }
    dataset_name_start = &dataset_name[3];
    dataset_name_end = &dataset_name[dataset_name_len];
  } else {
    dataset_name_start = &dataset_name[2];
    dataset_name_end = &dataset_name[dataset_name_len-1];
    errmsg(dfile, "No support (yet) for open_dataset of relative dataset %s read/write - datasets must be fully qualified.", dataset_name);
    return DIOERR_RELATIVE_DATASET_NAME_NOT_IMPLEMENTED_YET;
  }
  ds_full_len = dataset_name_end - dataset_name_start - 1;
  const char* open_paren=strchr(dataset_name, '(');
  const char* close_paren=strchr(dataset_name, ')');

  size_t dslen;
  if (open_paren && close_paren) {
    dslen = open_paren - dataset_name_start;
    size_t memlen = close_paren - open_paren - 1;
    if (memlen > MEM_MAX) {
      errmsg(dfile, "Member name of %s is more than %d characters.", dataset_name, MEM_MAX);
      return DIOERR_MEMBER_NAME_TOO_LONG;
    }
    /* dataset member - valid */
    memcpy(difile->member_name, open_paren+1, memlen);
    difile->member_name[memlen] = '\0';
  } else if (!open_paren && !close_paren) {
    /* dataset - valid */
    dslen = dataset_name_end - dataset_name_start - 1;
    difile->member_name[0] = '\0';
  } else {
    /* mis-matched parens - invalid */
    errmsg(dfile, "Dataset %s is not a valid dataset name or dataset member name.", dataset_name);
    return DIOERR_LE_DATASET_NAME_PAREN_MISMATCH;
  }
  memcpy(difile->dataset_full_name, dataset_name_start, ds_full_len);
  difile->dataset_full_name[ds_full_len] = '\0';
  memcpy(difile->dataset_name, dataset_name_start, dslen);
  difile->dataset_name[dslen] = '\0';

  strupper(difile->dataset_full_name);
  strupper(difile->dataset_name);
  strupper(difile->member_name);

#ifdef DEBUG
  printf("Original <%s> full <%s> name <%s> member <%s>\n", 
    dataset_name, difile->dataset_full_name, difile->dataset_name, difile->member_name);
#endif
  return DIOERR_NOERROR;
}

static const char* dsorgs_internal(enum DSORG dsorg)
{
  switch(dsorg) {
    case D_PDS: return "PDS";
    case D_PDSE: return "PDSE";
    case D_SEQ: return "SEQ";
  }
  return "UNK";
}
const char* dsorgs(enum DSORG dsorg, char* buff)
{
  const char* es = dsorgs_internal(dsorg);
  size_t len = strlen(es);
  memcpy(buff, es, len+1);
  if (__isASCII()) {
    __e2a_l(buff, len);
  }
  return buff;
}

static const char* recfms_internal(enum DRECFM drecfm)
{
  switch(drecfm) {
    case D_F: return "F";
    case D_V: return "V";
    case D_U: return "U";
    case D_VA: return "VA";
    case D_FA: return "FA";
  }
  return "UNK";
}

const char* recfms(enum DRECFM drecfm, char* buff)
{
  const char* es = recfms_internal(drecfm);
  size_t len = strlen(es);
  memcpy(buff, es, len+1);
  if (__isASCII()) {
    __e2a_l(buff, len);
  }
  return buff;
}

const char* dccsids(int dccsid, char* buff)
{
  switch(dccsid) {
    case DCCSID_NOTSET:
      strcpy(buff, "?");
      break;
    case DCCSID_BINARY:
      strcpy(buff, "B");
      break;
    default:
      sprintf(buff, "%u", dccsid);
      break;
  }
  size_t len = strlen(buff);
  if (__isASCII()) {
    __e2a_l(buff, len);
  }
  return buff;
}

static const char* dstates(enum DSTATE dstate)
{
  switch(dstate) {
    case D_CLOSED: return "closed";
    case D_READ_BINARY: return "rb";
    case D_READWRITE_BINARY: return "rb+";
    case D_WRITE_BINARY: return "wb";
  }
  return "UNK";
}

static FILE* opendd(struct DFILE* dfile, struct DIFILE* difile, const char* openfmt)
{
 char copendd[DD_MAX+MEM_MAX+2+2+2+1+1];
  if (has_member(difile)) {
    sprintf(copendd, "//DD:%s(%s)", difile->ddname, difile->member_name);
  } else {
    sprintf(copendd, "//DD:%s", difile->ddname);
  }
  FILE* fp = fopen(copendd, openfmt);
  if (fp == NULL) {
    errmsg(dfile, strerror(errno));
  }
  return fp;
}

struct DFILE* open_dataset(const char* dataset_name, FILE* logstream)
{
  enum DIOERR rc;

  /*
   * We may want to change this, but for right now, we want
   * to ensure that zero-length variable records are properly
   * mapped
   */
  setenv("_EDC_ZERO_RECLEN", "1", 1);

  errno = 0;

  struct DFILE* dfile = calloc(1, sizeof(struct DFILE));
  if (!dfile) {
    return NULL;
  }
  dfile->msgbuff = calloc(1, DIO_MSG_BUFF_LEN+1);
  if (!dfile->msgbuff) {
    dfile->err = DIOERR_MALLOC_FAILED;
    return dfile;
  }
  dfile->msgbufflen = DIO_MSG_BUFF_LEN;
  dfile->logstream = logstream;

  struct DIFILE* difile = calloc(1, sizeof(struct DIFILE));
  if (!difile) {
    dfile->err = DIOERR_MALLOC_FAILED;
    return dfile;
  }

  dfile->internal = difile;

  char* dataset_name_copy = strdup(dataset_name);
  size_t len = strlen(dataset_name_copy);
  if (__isASCII()) {
    __a2e_l(dataset_name_copy, len);
  }
  rc = init_dataset_info(dfile, dataset_name_copy, difile);
  if (rc) {
    dfile->err = rc;
    return dfile;
  }

  struct s99_common_text_unit dsn = { DALDSNAM, 1, 0, 0 };
  struct s99_common_text_unit dd = { DALRTDDN, 1, sizeof(DD_SYSTEM)-1, DD_SYSTEM };
  struct s99_common_text_unit stats = { DALSTATS, 1, 1, {0x8} };

  rc = init_dsnam_text_unit(dfile, difile->dataset_name, &dsn);
  if (rc) {
    dfile->err = rc;
    return dfile;
  }
  rc = dsdd_alloc(dfile, &dsn, &dd, &stats);
  if (rc) {
    dfile->err = rc;
    return dfile;
  }

  memcpy(difile->ddname, dd.s99tupar, dd.s99tulng);
  difile->ddname[dd.s99tulng] = '\0';

  difile->dstate = D_CLOSED;

#ifdef DEBUG
  printf("allocated ddname:%s\n", difile->ddname);
#endif

  /*
   * Note - there is a timing window here and it is not efficient to 
   * open the dataset twice (once to get the dataset characteristics and once to read or write)
   * but this is 'good enough' for now since the C I/O services don't let us do better 
   */

  difile->fp = opendd(dfile, difile, "rb+,type=record");
  if (difile->fp) {
    difile->dstate = D_READWRITE_BINARY;
  } else {
    if ((errno == ERRNO_NONEXISTANT_FILE) && has_member(difile)) {
      /*
       * This is a PDS or PDSE member, and the member does not exist yet.
       * We need to open the member in write to get the attributes of the actual
       * PDS(E) member, otherwise if we only specify the PDS(E), we will get
       * the attributes for working with the PDS(E) directly, which is wrong (unformatted)
       * This is 'ok' because if we don't have write access to the PDS(E) to create a 
       * member, we should know this now now rather than later.
       */
     
      difile->fp = opendd(dfile, difile, "wb,type=record");
      if (!difile->fp) {
        return dfile;
      }
      difile->dstate = D_WRITE_BINARY;
    } else {
      /*
       * Try to open 'rb' (perhaps file is write protected)
       */
      difile->fp = opendd(dfile, difile, "rb,type=record");
      if (difile->fp) {
        dfile->readonly = 1;
        difile->dstate = D_READ_BINARY;
      } else {
        errmsg(dfile, "Unable to obtain dataset %s for READ.", dataset_name_copy);
        dfile->err = DIOERR_FOPEN_FOR_READ_FAILED;
        return dfile;
      }
    }
  }

  fldata_t info;
  rc = __fldata(difile->fp, NULL, &info);
  if (rc) {
    errmsg(dfile, "Unable to obtain file information for %s.", dataset_name_copy);
    close_dataset(dfile);
    dfile->err = DIOERR_FLDATA_FAILED;
    return dfile;
  }

  if (info.__recfmF) {
    if (info.__recfmASA) {
      dfile->recfm = D_FA;
    } else {
      dfile->recfm = D_F;
    }
  } else if (info.__recfmV) {
    if (info.__recfmASA) {
      dfile->recfm = D_VA;
    } else {
      dfile->recfm = D_V;
    }
  } else if (info.__recfmU) {
    dfile->recfm = D_U;
  } else {
    errmsg(dfile, "Dataset %s is not F, V, or U format. open_dataset not supported at this time.", dataset_name_copy);
    dfile->err = DIOERR_UNSUPPORTED_RECFM;
    return dfile;
  }

  if (info.__dsorgPDSE) {
    dfile->dsorg = D_PDSE;
  } else if (info.__dsorgPO) {
    dfile->dsorg = D_PDS;
  } else if (info.__dsorgPS) {
    dfile->dsorg = D_SEQ;
  } else {
    errmsg(dfile, "Dataset %s is not PDS, PDSE, or SEQ organization. open_dataset not supported at this time.", dataset_name_copy);
    dfile->err = DIOERR_UNSUPPORTED_RECFM;
    return dfile;
  }

  dfile->reclen = info.__maxreclen;
  dfile->dccsid = DCCSID_NOTSET; 

#ifdef DEBUG
  char ccsidstr[DCCSID_MAX];
  printf("Dataset attributes: dsorg:%s recfm:%s lrecl:%d dstate:%s ccsid:%s\n", 
    dsorgs_internal(dfile->dsorg), recfms_internal(dfile->recfm), dfile->reclen, dstates(difile->dstate), dccsids(dfile->dccsid, ccsidstr));
#endif

  return dfile;
}

int has_length_prefix(enum DRECFM recfm)
{
  int length_prefix;
  switch (recfm) {
    case D_F:
    case D_FA:
    case D_U:
      length_prefix=0;
      break;
    case D_V:
    case D_VA:
      length_prefix=1;
  }
  return length_prefix;
}

#define INIT_READ_BUFFER_SIZE (1<<24) /* 16MB */
#define DS_MAX_REC_SIZE (32768)
static enum DIOERR read_dataset_internal(struct DFILE* dfile)
{
  struct DIFILE* difile = (struct DIFILE*) (dfile->internal);
  char record[DS_MAX_REC_SIZE];
  int rc;

  errno = 0;

  if (difile->dstate == D_WRITE_BINARY) {
    rc=fclose(difile->fp);
    if (rc) {
      errmsg(dfile, strerror(errno));
      return DIOERR_FCLOSE_FAILED_ON_READ;
    }
    difile->dstate = D_CLOSED;
  }

  if (difile->dstate == D_READWRITE_BINARY) {
    rewind(difile->fp);
  }

  if (difile->dstate == D_CLOSED) {
    difile->fp = opendd(dfile, difile, "rb+,type=record");
    if (!difile->fp) {
      return DIOERR_OPENDD_FOR_READ_FAILED;
    }
  }
  difile->dstate = D_READWRITE_BINARY;

  if ((difile->read_buffer_size == 0) || (dfile->buffer == NULL)) {
    difile->read_buffer_size = INIT_READ_BUFFER_SIZE;
    dfile->buffer = malloc(difile->read_buffer_size);
    if (!dfile->buffer) {
      errmsg(dfile, "Unable to acquire storage to read dataset %s.", difile->dataset_name);
      return DIOERR_READ_BUFFER_ALLOC_FAILED;
    }
  }
  difile->cur_read_offset = 0;

  int length_prefix = has_length_prefix(dfile->recfm);

  size_t size = 1;
  size_t count = dfile->reclen;
  size_t bytes_to_copy;
  int isbinary = 0;
  uint16_t reclen;
  errno = 0;
  while (1) {
    rc = fread(record, size, count, difile->fp);
    if (errno) {
      errmsg(dfile, strerror(errno));
      return DIOERR_FREAD_FAILED;
    }
    if (feof(difile->fp)) {
      break;
    }
    bytes_to_copy = rc;
    if (length_prefix) {
      bytes_to_copy += sizeof(uint16_t);
    }
    if (difile->cur_read_offset + bytes_to_copy > difile->read_buffer_size) {
      errmsg(dfile, "To be implemented - need to write code to grow buffer for reading in file.");
      return DIOERR_LARGE_READ_BUFFER_NOT_IMPLEMENTED_YET;
    }
    reclen = rc;
    if (length_prefix) {
      memcpy(&dfile->buffer[difile->cur_read_offset], &reclen, sizeof(reclen));
      difile->cur_read_offset += sizeof(reclen);
    }
    memcpy(&dfile->buffer[difile->cur_read_offset], record, bytes_to_copy);
    if (!is_binary)
      isbinary = is_binary(&dfile->buffer[difile->cur_read_offset], bytes_to_copy); 
#ifdef DEBUG
    printf("%5.5u <%*.*s>\n", reclen, reclen, reclen, record);
#endif
    difile->cur_read_offset += rc;
  }
  dfile->bufflen = difile->cur_read_offset;
  dfile->is_binary = isbinary;
  return DIOERR_NOERROR;
}

enum DIOERR read_dataset(struct DFILE* dfile)
{
  enum DIOERR rc = read_dataset_internal(dfile);
  dfile->err = rc;
  return rc;
}

static enum DIOERR write_dataset_internal(struct DFILE* dfile)
{
  struct DIFILE* difile = (struct DIFILE*) (dfile->internal);
  char record[DS_MAX_REC_SIZE];
  int rc;

  errno = 0;

  if ((dfile->bufflen == 0) || (dfile->buffer == NULL)) {
    errmsg(dfile, "No buffer and/or buffer length not positive - no action performed.");
    return DIOERR_INVALID_BUFFER_PASSED_TO_WRITE;
  }

  if (difile->dstate == D_READ_BINARY) {
    rc=fclose(difile->fp);
    if (rc) {
      errmsg(dfile, strerror(errno));
      return DIOERR_FCLOSE_FAILED_ON_WRITE;
    }
    difile->dstate = D_CLOSED;
  }

  if (difile->dstate == D_READWRITE_BINARY) {
    rc=fclose(difile->fp);
    if (rc) {
      errmsg(dfile, strerror(errno));
      return DIOERR_FCLOSE_FAILED_ON_WRITE;
    }
    difile->dstate = D_CLOSED;  
  }

  if (difile->dstate == D_CLOSED) {
    difile->fp = opendd(dfile, difile, "wb,type=record");
    if (!difile->fp) {
      return DIOERR_OPENDD_FOR_WRITE_FAILED;
    }
  }
  difile->dstate = D_WRITE_BINARY;

  int length_prefix = has_length_prefix(dfile->recfm);

  size_t size = 1;
  size_t buffer_offset = 0;

  int err=0;
  if (length_prefix) {
    uint16_t reclen;
    while (buffer_offset < dfile->bufflen) {
      reclen = *((uint16_t*)(&dfile->buffer[buffer_offset]));
      buffer_offset += sizeof(uint16_t);
      rc = fwrite(&dfile->buffer[buffer_offset], size, reclen, difile->fp);
      /*
       * If we didn't write out a full record, something went wrong (e.g. dataset full)
       */
      if (rc < reclen) {
        err = 1;
        break;
      }
      buffer_offset += rc;
    }
  } else {
    while (buffer_offset < dfile->bufflen) {
      rc = fwrite(&dfile->buffer[buffer_offset], size, dfile->reclen, difile->fp);
      /*
       * If we didn't write out a full record, something went wrong (e.g. dataset full)
       */
      if (rc < dfile->reclen) {
        err = 1;
        break;
      }
      buffer_offset += rc;
    }
  }

  if (err) {
    errmsg(dfile, strerror(errno));
    return DIOERR_FWRITE_FAILED;
  } else {
    return DIOERR_NOERROR;
  }
}

enum DIOERR write_dataset(struct DFILE* dfile)
{
  enum DIOERR rc = write_dataset_internal(dfile);
  dfile->err = rc;
  return rc;
}

static enum DIOERR close_dataset_internal(struct DFILE* dfile)
{
  int rc = 0;
  struct DIFILE* difile = (struct DIFILE*) dfile->internal;

  rc = fclose(difile->fp);
  if (rc) {
    errmsg(dfile, strerror(errno));
    return DIOERR_FCLOSE_FAILED_ON_CLOSE;
  } 

  return DIOERR_NOERROR;
}

enum DIOERR close_dataset(struct DFILE* dfile)
{
  enum DIOERR rc = close_dataset_internal(dfile);
  dfile->err = rc;
  return rc;
}

const char* low_level_qualifier(struct DFILE* dfile, char* llq_copy)
{
  struct DIFILE* difile = (struct DIFILE*) dfile->internal;

  const char* last_dot = strrchr(difile->dataset_name, '.');
  const char* llq;

  /*
   * A valid dataset name always has at least one dot, so this check is
   * not required unless the call is made as the DFILE structure is being established.
   */
  if (last_dot == NULL) {
    llq = difile->dataset_name;
  } else {
    llq = last_dot + 1;
  }
  size_t llqlen = strlen(llq);
  if (llqlen > LLQ_MAX-1) {
    llqlen = LLQ_MAX-1;
  }

  memcpy(llq_copy, llq, llqlen);
  llq_copy[llqlen] = '\0';
  if (__isASCII()) {
    __e2a_l(llq_copy, llqlen);
  }
  return llq_copy;
}

const char* member_name(struct DFILE* dfile, char* member_copy)
{
  struct DIFILE* difile = (struct DIFILE*) dfile->internal;

  if (has_member(difile)) {
    size_t len = strlen(difile->member_name);
    memcpy(member_copy, difile->member_name, len);
    member_copy[len] = '\0';
    if (__isASCII()) {
      __e2a_l(member_copy, len);
    }
    return member_copy;
  }
  return NULL;
}

const char* high_level_qualifier(struct DFILE* dfile, char* hlq_copy) {
  struct DIFILE* difile = (struct DIFILE*) dfile->internal;

  const char* first_dot = strchr(difile->dataset_full_name, '.');
  const char* hlq;

  if (first_dot == NULL) {
    strcpy(hlq_copy, difile->dataset_full_name); // No dot means the entire name is the HLQ
  } else {
    size_t hlqlen = first_dot - difile->dataset_full_name;
    if (hlqlen > DS_FULL_MAX-1) {
      hlqlen = DS_FULL_MAX-1;
    }
    memcpy(hlq_copy, difile->dataset_full_name, hlqlen);
    hlq_copy[hlqlen] = '\0';
    if (__isASCII()) {
      __e2a_l(hlq_copy, hlqlen);
    }
  }
  return hlq_copy;
}
 
const char* high_to_mid_level_qualifier(struct DFILE* dfile, char* hmql_copy) {
  struct DIFILE* difile = (struct DIFILE*) dfile->internal;

  const char* last_dot = strrchr(difile->dataset_full_name, '.');

  if (last_dot == NULL) {
    strcpy(hmql_copy, difile->dataset_full_name); // No dot means the entire name is the HMQL
  } else {
    size_t hmql_len = last_dot - difile->dataset_full_name;
    if (hmql_len > DS_FULL_MAX-1) {
      hmql_len = DS_FULL_MAX-1;
    }
    memcpy(hmql_copy, difile->dataset_full_name, hmql_len);
    hmql_copy[hmql_len] = '\0';
    if (__isASCII()) {
      __e2a_l(hmql_copy, hmql_len);
    }
  }
  return hmql_copy;
}

const char* llq_to_extension(struct DFILE* dfile, char* llq, char* extension) {
  if (strcasecmp(llq, "COBOL") == 0) {
    strcpy(extension, "cbl");
  } else if (strcasecmp(llq, "H") == 0) {
    strcpy(extension, "h");
  } else if (strcasecmp(llq, "ASM") == 0) {
    strcpy(extension, "asm");
  } else if (strcasecmp(llq, "C") == 0) {
    strcpy(extension, "c");
  } else if (strcasecmp(llq, "JCL") == 0) {
    strcpy(extension, "jcl");
  } else if (strcasecmp(llq, "PLI") == 0) {
    strcpy(extension, "pli");
  } else if (strcasecmp(llq, "SRC") == 0) {
    strcpy(extension, "c"); // is SRC usually C source?
  } else if (strcasecmp(llq, "TXT") == 0) {
    strcpy(extension, "txt");
  } else {
    strcpy(extension, "txt"); // Is this a good default?
  }

  if (__isASCII()) {
    __e2a_l(extension, strlen(extension));
  }

  return extension;
}

const char* map_to_unixfile(struct DFILE* dfile, char* unixfile) {
  char member[MEM_MAX+1];
  char hlq[HLQ_MAX+1];
  char llq[LLQ_MAX+1];
  char hightomid[DS_MAX+1];
  char extension[EXTENSION_MAX];

  // We we want to avoid double converting, so call the APIs in EBCDIC mode and then switch back
  int orig = __ae_thread_swapmode(__AE_EBCDIC_MODE);

  high_to_mid_level_qualifier(dfile, hightomid);
  high_level_qualifier(dfile, hlq);
  low_level_qualifier(dfile, llq);
  member_name(dfile, member);
  
  struct DIFILE* difile = (struct DIFILE*) dfile->internal;
  if (has_member(difile))
    sprintf(unixfile, "%s.%s.%s", hightomid, member, llq_to_extension(dfile, llq, extension));
  else
    sprintf(unixfile, "%s.%s", hightomid,  llq_to_extension(dfile, llq, extension));

  __ae_thread_swapmode(orig);

  if (__isASCII()) {
    __e2a_l(unixfile, strlen(unixfile));
  }

  return unixfile;
}

int is_binary(const char *str, int length) {
  for (int i = 0; i < length; i++) {
    char c = str[i];

    // NUL byte (0x00) and Newline (0x15)
    if (c == 0x00 || c == 0x15) {
      return 1;
    }
  }

  return 0;
}
