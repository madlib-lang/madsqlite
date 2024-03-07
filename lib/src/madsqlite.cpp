#include "sqlite3.h"
#include "gc.h"
#include "apply-pap.hpp"
#include "list.hpp"
#include "number.hpp"

// Sqlite errors

// #define SQLITE_OK           0   /* Successful result */
// /* beginning-of-error-codes */
// #define SQLITE_ERROR        1   /* Generic error */
// #define SQLITE_INTERNAL     2   /* Internal logic error in SQLite */
// #define SQLITE_PERM         3   /* Access permission denied */
// #define SQLITE_ABORT        4   /* Callback routine requested an abort */
// #define SQLITE_BUSY         5   /* The database file is locked */
// #define SQLITE_LOCKED       6   /* A table in the database is locked */
// #define SQLITE_NOMEM        7   /* A malloc() failed */
// #define SQLITE_READONLY     8   /* Attempt to write a readonly database */
// #define SQLITE_INTERRUPT    9   /* Operation terminated by sqlite3_interrupt()*/
// #define SQLITE_IOERR       10   /* Some kind of disk I/O error occurred */
// #define SQLITE_CORRUPT     11   /* The database disk image is malformed */
// #define SQLITE_NOTFOUND    12   /* Unknown opcode in sqlite3_file_control() */
// #define SQLITE_FULL        13   /* Insertion failed because database is full */
// #define SQLITE_CANTOPEN    14   /* Unable to open the database file */
// #define SQLITE_PROTOCOL    15   /* Database lock protocol error */
// #define SQLITE_EMPTY       16   /* Internal use only */
// #define SQLITE_SCHEMA      17   /* The database schema changed */
// #define SQLITE_TOOBIG      18   /* String or BLOB exceeds size limit */
// #define SQLITE_CONSTRAINT  19   /* Abort due to constraint violation */
// #define SQLITE_MISMATCH    20   /* Data type mismatch */
// #define SQLITE_MISUSE      21   /* Library used incorrectly */
// #define SQLITE_NOLFS       22   /* Uses OS features not supported on host */
// #define SQLITE_AUTH        23   /* Authorization denied */
// #define SQLITE_FORMAT      24   /* Not used */
// #define SQLITE_RANGE       25   /* 2nd parameter to sqlite3_bind out of range */
// #define SQLITE_NOTADB      26   /* File opened that is not a database file */
// #define SQLITE_NOTICE      27   /* Notifications from sqlite3_log() */
// #define SQLITE_WARNING     28   /* Warnings from sqlite3_log() */
// #define SQLITE_ROW         100  /* sqlite3_step() has another row ready */
// #define SQLITE_DONE        101  /* sqlite3_step() has finished executing */

#ifdef __cplusplus
extern "C"
{
#endif

  const int64_t madsqlite__Value_Blob = 0;
  const int64_t madsqlite__Value_Integer = 1;
  const int64_t madsqlite__Value_Null = 2;
  const int64_t madsqlite__Value_Real = 3;
  const int64_t madsqlite__Value_Text = 4;

  typedef struct madsqlite__Value {
    int64_t index;
    void *data;
  } madsqlite__Value_t;

  void madlibsqlite__connect(char *path, PAP_t *callback) {
    sqlite3 *db;
    int64_t error = sqlite3_open(path, &db);
    __applyPAP__(callback, 2, db, error);
  }

  void madlibsqlite__disconnect(sqlite3 *db) {
    sqlite3_close(db);
  }

  int onRowRead(void *cb, int columnCount, char **columnValues, char **columnNames) {
    madlib__list__Node_t *row = madlib__list__empty();
    for (int i = 0; i < columnCount; i++) {
      if (columnValues[i]) {
        size_t valueSize = strlen(columnValues[i]);
        char *value = (char *)GC_MALLOC(valueSize + 1);
        memcpy(value, columnValues[i], valueSize);
        value[valueSize] = '\0';
        row = madlib__list__internal__append(value, row);
      }
    }

    __applyPAP__(cb, 1, row);
    return 0;
  }


  // #define SQLITE_INTEGER  1
  // #define SQLITE_FLOAT    2
  // #define SQLITE_TEXT     3
  // #define SQLITE_BLOB     4
  // #define SQLITE_NULL     5

  // export type SqliteType
  // = BlobType(String)      0
  // | IntegerType(Integer)  1
  // | NullType              2
  // | RealType(Float)       3
  // | TextType(String)      4

  void madlibsqlite__query(sqlite3 *db, char *sql, PAP_t *callback, PAP_t *errorCallback) {
    char *error = 0;
    sqlite3_stmt *pstmt;

    int r = sqlite3_prepare_v3(db, sql, -1, 0, &pstmt, NULL);

    if (r != 0) {
      error = (char *) sqlite3_errmsg(db);
      size_t errorMessageSize = strlen(error);
      char *errorMessage = (char *)GC_MALLOC(errorMessageSize + 1);
      memcpy(errorMessage, error, errorMessageSize);
      errorMessage[errorMessageSize] = '\0';
      __applyPAP__(errorCallback, 2, r, errorMessage);
      return;
    }

    madlib__list__Node_t *rows = NULL, *tail = NULL;
    while(sqlite3_step(pstmt) == SQLITE_ROW) {
      madlib__list__Node_t *row = madlib__list__empty();
      int columnCount = sqlite3_column_count(pstmt);

      for (int i = columnCount - 1; i >= 0; i--) {
        madsqlite__Value_t *value = (madsqlite__Value_t *) GC_MALLOC(sizeof(madsqlite__Value_t));
        int colType = sqlite3_column_type(pstmt, i);
          switch(colType) {
            case SQLITE_INTEGER: {
              value->index = madsqlite__Value_Integer;
              value->data = (void *) sqlite3_column_int64(pstmt, i);
              break;
            }

            case SQLITE_FLOAT: {
              value->index = madsqlite__Value_Real;
              value->data = boxDouble(sqlite3_column_double(pstmt, i));
              break;
            }

            case SQLITE_TEXT:
            case SQLITE_BLOB: {
              char *colValue = (char *) sqlite3_column_text(pstmt, i);
              size_t valueSize = strlen(colValue);
              char *text = (char *)GC_MALLOC(valueSize + 1);
              memcpy(text, colValue, valueSize);
              text[valueSize] = '\0';

              value->index = madsqlite__Value_Text;
              value->data = text;
              break;
            }

            default: {
              value->index = madsqlite__Value_Null;
            }
          }
        row = madlib__list__internal__push(value, row);
      }

      madlib__list__Node_t *node = madlib__list__singleton(row);
      if (rows == NULL) {
        rows = node;
        tail = node;
      } else {
        tail->next = node;
        tail = node;
      }
    }
    sqlite3_finalize(pstmt);

    if (rows == NULL) {
      rows = madlib__list__empty();
    }

    __applyPAP__(callback, 1, rows);
  }

#ifdef __cplusplus
}
#endif
